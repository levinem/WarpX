#ifndef WARPX_PhysicalParticleContainer_H_
#define WARPX_PhysicalParticleContainer_H_

#include <PlasmaInjector.H>
#include <WarpXParticleContainer.H>
#include <NCIGodfreyFilter.H>

#include <AMReX_IArrayBox.H>

#include <map>

class PhysicalParticleContainer
    : public WarpXParticleContainer
{
public:
    PhysicalParticleContainer (amrex::AmrCore* amr_core,
                               int ispecies,
                               const std::string& name);

    PhysicalParticleContainer (amrex::AmrCore* amr_core);

    virtual ~PhysicalParticleContainer () {}

    virtual void InitData () override;

    void InitIonizationModule ();

#ifdef WARPX_DO_ELECTROSTATIC
    virtual void FieldGatherES(const amrex::Vector<std::array<std::unique_ptr<amrex::MultiFab>, 3> >& E,
                               const amrex::Vector<std::unique_ptr<amrex::FabArray<amrex::BaseFab<int> > > >& masks) override;

    virtual void EvolveES (const amrex::Vector<std::array<std::unique_ptr<amrex::MultiFab>, 3> >& E,
                           amrex::Vector<std::unique_ptr<amrex::MultiFab> >& rho,
                           amrex::Real t, amrex::Real dt) override;
#endif // WARPX_DO_ELECTROSTATIC

    virtual void FieldGather (int lev,
                              const amrex::MultiFab& Ex,
                              const amrex::MultiFab& Ey,
                              const amrex::MultiFab& Ez,
                              const amrex::MultiFab& Bx,
                              const amrex::MultiFab& By,
                              const amrex::MultiFab& Bz) final;

    void FieldGather (WarpXParIter& pti,
                      RealVector& Exp,
                      RealVector& Eyp,
                      RealVector& Ezp,
                      RealVector& Bxp,
                      RealVector& Byp,
                      RealVector& Bzp,
                      amrex::FArrayBox const * exfab,
                      amrex::FArrayBox const * eyfab,
                      amrex::FArrayBox const * ezfab,
                      amrex::FArrayBox const * bxfab,
                      amrex::FArrayBox const * byfab,
                      amrex::FArrayBox const * bzfab,
                      const int ngE, const int e_is_nodal,
                      const long offset,
                      const long np_to_gather,
                      int thread_num,
                      int lev,
                      int depos_lev);

    virtual void Evolve (int lev,
                         const amrex::MultiFab& Ex,
                         const amrex::MultiFab& Ey,
                         const amrex::MultiFab& Ez,
                         const amrex::MultiFab& Bx,
                         const amrex::MultiFab& By,
                         const amrex::MultiFab& Bz,
                         amrex::MultiFab& jx,
                         amrex::MultiFab& jy,
                         amrex::MultiFab& jz,
                         amrex::MultiFab* cjx,
                         amrex::MultiFab* cjy,
                         amrex::MultiFab* cjz,
                         amrex::MultiFab* rho,
                         amrex::MultiFab* crho,
                         const amrex::MultiFab* cEx,
                         const amrex::MultiFab* cEy,
                         const amrex::MultiFab* cEz,
                         const amrex::MultiFab* cBx,
                         const amrex::MultiFab* cBy,
                         const amrex::MultiFab* cBz,
                         amrex::Real t,
                         amrex::Real dt,
                         DtType a_dt_type=DtType::Full) override;

    virtual void PushPX(WarpXParIter& pti,
                        amrex::Cuda::ManagedDeviceVector<amrex::ParticleReal>& xp,
                        amrex::Cuda::ManagedDeviceVector<amrex::ParticleReal>& yp,
                        amrex::Cuda::ManagedDeviceVector<amrex::ParticleReal>& zp,
                        amrex::Real dt, DtType a_dt_type=DtType::Full);

    virtual void PushP (int lev, amrex::Real dt,
                        const amrex::MultiFab& Ex,
                        const amrex::MultiFab& Ey,
                        const amrex::MultiFab& Ez,
                        const amrex::MultiFab& Bx,
                        const amrex::MultiFab& By,
                        const amrex::MultiFab& Bz) override;

    void PartitionParticlesInBuffers(
                        long& nfine_current,
                        long& nfine_gather,
                        long const np,
                        WarpXParIter& pti,
                        int const lev,
                        amrex::iMultiFab const* current_masks,
                        amrex::iMultiFab const* gather_masks,
                        RealVector& uxp,
                        RealVector& uyp,
                        RealVector& uzp,
                        RealVector& wp );

    void copy_attribs(WarpXParIter& pti,const amrex::ParticleReal* xp,
                        const amrex::ParticleReal* yp, const amrex::ParticleReal* zp);

    virtual void PostRestart () final {}

    void SplitParticles(int lev);

    virtual void buildIonizationMask (const amrex::MFIter& mfi, const int lev,
                                      amrex::Gpu::ManagedDeviceVector<int>& ionization_mask) override;

    // Inject particles in Box 'part_box'
    virtual void AddParticles (int lev);

    void AddPlasma(int lev, amrex::RealBox part_realbox = amrex::RealBox());

    void MapParticletoBoostedFrame(amrex::Real& x, amrex::Real& y, amrex::Real& z, std::array<amrex::Real, 3>& u);

    void AddGaussianBeam(amrex::Real x_m, amrex::Real y_m, amrex::Real z_m,
                         amrex::Real x_rms, amrex::Real y_rms, amrex::Real z_rms,
                         amrex::Real q_tot, long npart, int do_symmetrize);

    void CheckAndAddParticle(amrex::Real x, amrex::Real y, amrex::Real z,
                             std::array<amrex::Real, 3> u, amrex::Real weight);

    virtual void GetParticleSlice(const int direction, const amrex::Real z_old,
                                  const amrex::Real z_new, const amrex::Real t_boost,
                                  const amrex::Real t_lab, const amrex::Real dt,
                                  DiagnosticParticles& diagnostic_particles) final;

    virtual void ConvertUnits (ConvertDirection convert_dir) override;

/**
 * \brief Apply NCI Godfrey filter to all components of E and B before gather
 * \param lev MR level
 * \param box box onto which the filter is applied
 * \param exeli safeguard to avoid destructing arrays between ParIter iterations on GPU
 * \param filtered_Ex Array containing filtered value
 * \param Ex Field array before filtering (not modified)
 * \param ex_ptr pointer to the array used for field gather.
 *
 * The NCI Godfrey filter is applied on Ex, the result is stored in filtered_Ex
 * and the pointer is modified (before this function is called, it points to Ex
 * and after this function is called, it points to Ex_filtered)
 */
    void applyNCIFilter (
        int lev, const amrex::Box& box,
        amrex::Elixir& exeli, amrex::Elixir& eyeli, amrex::Elixir& ezeli,
        amrex::Elixir& bxeli, amrex::Elixir& byeli, amrex::Elixir& bzeli,
        amrex::FArrayBox& filtered_Ex, amrex::FArrayBox& filtered_Ey,
        amrex::FArrayBox& filtered_Ez, amrex::FArrayBox& filtered_Bx,
        amrex::FArrayBox& filtered_By, amrex::FArrayBox& filtered_Bz,
        const amrex::FArrayBox& Ex, const amrex::FArrayBox& Ey,
        const amrex::FArrayBox& Ez, const amrex::FArrayBox& Bx,
        const amrex::FArrayBox& By, const amrex::FArrayBox& Bz,
        amrex::FArrayBox const * & exfab, amrex::FArrayBox const * & eyfab,
        amrex::FArrayBox const * & ezfab, amrex::FArrayBox const * & bxfab,
        amrex::FArrayBox const * & byfab, amrex::FArrayBox const * & bzfab);

protected:

    std::string species_name;
    std::unique_ptr<PlasmaInjector> plasma_injector;

    // When true, adjust the transverse particle positions accounting
    // for the difference between the Lorentz transformed time of the
    // particle and the time of the boosted frame.
    bool boost_adjust_transverse_positions = false;
    bool do_backward_propagation = false;

    // Inject particles during the whole simulation
    void ContinuousInjection (const amrex::RealBox& injection_box) override;

};

#endif
