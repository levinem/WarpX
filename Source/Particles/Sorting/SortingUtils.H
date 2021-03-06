#ifndef WARPX_PARTICLES_SORTING_SORTINGUTILS_H_
#define WARPX_PARTICLES_SORTING_SORTINGUTILS_H_

#include <WarpXParticleContainer.H>
#include <AMReX_CudaContainers.H>
#include <AMReX_Gpu.H>
#ifdef AMREX_USE_GPU
    #include <thrust/partition.h>
    #include <thrust/distance.h>
#endif

/* \brief Fill the elements of the input vector with consecutive integer,
 *        starting from 0
 *
 * \param[inout] v Vector of integers, to be filled by this routine
 */
void fillWithConsecutiveIntegers( amrex::Gpu::DeviceVector<long>& v )
{
#ifdef AMREX_USE_GPU
    // On GPU: Use thrust
    thrust::sequence( v.begin(), v.end() );
#else
    // On CPU: Use std library
    std::iota( v.begin(), v.end(), 0L );
#endif
}

/* \brief Find the indices that would reorder the elements of `predicate`
 * so that the elements with non-zero value precede the other elements
 *
 * \param[in, out] index_begin Point to the beginning of the vector which is
 *            to be filled with these indices
 * \param[in, out] index_begin Point to the end of the vector which is
 *            to be filled with these indices
 * \param[in] Vector that indicates the elements that need to be reordered first
 */
template< typename ForwardIterator >
ForwardIterator stablePartition(ForwardIterator const index_begin,
                               ForwardIterator const index_end,
                               amrex::Gpu::DeviceVector<int> const& predicate)
{
#ifdef AMREX_USE_GPU
    // On GPU: Use thrust
    int const* AMREX_RESTRICT predicate_ptr = predicate.dataPtr();
    ForwardIterator const sep = thrust::stable_partition(
        thrust::cuda::par(amrex::Cuda::The_ThrustCachedAllocator()),
        index_begin, index_end,
        [predicate_ptr] AMREX_GPU_DEVICE (long i) { return predicate_ptr[i]; }
    );
#else
    // On CPU: Use std library
    ForwardIterator const sep = std::stable_partition(
        index_begin, index_end,
        [&predicate](long i) { return predicate[i]; }
    );
#endif
    return sep;
}

/* \brief Return the number of elements between `first` and `last`
 *
 * \param[in] fist Points to a position in a vector
 * \param[in] last Points to another position in a vector
 * \return The number of elements between `first` and `last`
 */
template< typename ForwardIterator >
int iteratorDistance(ForwardIterator const first,
                     ForwardIterator const last)
{
#ifdef AMREX_USE_GPU
    // On GPU: Use thrust
    return thrust::distance( first, last );
#else
    return std::distance( first, last );
#endif
}

/* \brief Functor that fills the elements of the particle array `inexflag`
 *  with the value of the spatial array `bmasks`, at the corresponding particle position.
 *
 * This is done only for the elements from `start_index` to the end of `inexflag`
 *
 * \param[in] pti Contains information on the particle positions
 * \param[in] bmasks Spatial array, that contains a flag indicating
 *         whether each cell is part of the gathering/deposition buffers
 * \param[out] inexflag Vector to be filled with the value of `bmasks`
 * \param[in] geom Geometry object, necessary to locate particles within the array `bmasks`
 * \param[in] start_index Index that which elements start to be modified
 */
class fillBufferFlag
{
    public:
        fillBufferFlag( WarpXParIter const& pti, amrex::iMultiFab const* bmasks,
                        amrex::Gpu::DeviceVector<int>& inexflag,
                        amrex::Geometry const& geom, long const start_index=0 ) :
                        m_start_index(start_index) {

            // Extract simple structure that can be used directly on the GPU
            m_particles = &(pti.GetArrayOfStructs()[0]);
            m_buffer_mask = (*bmasks)[pti].array();
            m_inexflag_ptr = inexflag.dataPtr();
            m_domain = geom.Domain();
            for (int idim=0; idim<AMREX_SPACEDIM; idim++) {
                m_prob_lo[idim] = geom.ProbLo(idim);
                m_inv_cell_size[idim] = geom.InvCellSize(idim);
            }
        };


        AMREX_GPU_HOST_DEVICE AMREX_FORCE_INLINE
        void operator()( const long i ) const {
            // Select a particle
            auto const& p = m_particles[i+m_start_index];
            // Find the index of the cell where this particle is located
            amrex::IntVect const iv = amrex::getParticleCell( p,
                                m_prob_lo, m_inv_cell_size, m_domain );
            // Find the value of the buffer flag in this cell and
            // store it at the corresponding particle position in the array `inexflag`
            m_inexflag_ptr[i+m_start_index] = m_buffer_mask(iv);
        };

    private:
        amrex::GpuArray<amrex::Real,AMREX_SPACEDIM> m_prob_lo;
        amrex::GpuArray<amrex::Real,AMREX_SPACEDIM> m_inv_cell_size;
        amrex::Box m_domain;
        int* m_inexflag_ptr;
        WarpXParticleContainer::ParticleType const* m_particles;
        amrex::Array4<int const> m_buffer_mask;
        long const m_start_index;
};

/* \brief Functor that copies the elements of `src` into `dst`,
 *       while reordering them according to `indices`
 *
 * \param[in] src Source vector
 * \param[out] dst Destination vector
 * \param[in] indices Array of indices that indicate how to reorder elements
 */
template <typename T>
class copyAndReorder
{
    public:
        copyAndReorder(
            amrex::Gpu::ManagedDeviceVector<T> const& src,
            amrex::Gpu::ManagedDeviceVector<T>& dst,
            amrex::Gpu::DeviceVector<long> const& indices ) {
            // Extract simple structure that can be used directly on the GPU
            m_src_ptr = src.dataPtr();
            m_dst_ptr = dst.dataPtr();
            m_indices_ptr = indices.dataPtr();
        };

        AMREX_GPU_DEVICE AMREX_FORCE_INLINE
        void operator()( const long ip ) const {
            m_dst_ptr[ip] = m_src_ptr[ m_indices_ptr[ip] ];
        };

    private:
        T const* m_src_ptr;
        T* m_dst_ptr;
        long const* m_indices_ptr;
};

#endif // WARPX_PARTICLES_SORTING_SORTINGUTILS_H_
