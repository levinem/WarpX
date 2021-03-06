#ifndef WARPX_WarpXParticleContainer_H_
#define WARPX_WarpXParticleContainer_H_

#include "WarpXDtType.H"

#include <AMReX_Particles.H>
#include <AMReX_AmrCore.H>

#include <memory>

enum struct ConvertDirection{WarpX_to_SI, SI_to_WarpX};

struct PIdx
{
    enum { // Particle Attributes stored in amrex::ParticleContainer's struct of array
        w = 0,  // weight
        ux, uy, uz, Ex, Ey, Ez, Bx, By, Bz,
#ifdef WARPX_DIM_RZ
        theta, // RZ needs all three position components
#endif
        nattribs
    };
};

struct DiagIdx
{
    enum {
        w = 0,
        x, y, z, ux, uy, uz,
        nattribs
    };
};

struct TmpIdx
{
    enum {
        xold = 0,
        yold, zold, uxold, uyold, uzold,
        nattribs
    };
};

namespace ParticleStringNames
{
    const std::map<std::string, int> to_index = {
        {"w",     PIdx::w    },
        {"ux",    PIdx::ux   },
        {"uy",    PIdx::uy   },
        {"uz",    PIdx::uz   },
        {"Ex",    PIdx::Ex   },
        {"Ey",    PIdx::Ey   },
        {"Ez",    PIdx::Ez   },
        {"Bx",    PIdx::Bx   },
        {"By",    PIdx::By   },
        {"Bz",    PIdx::Bz   }
#ifdef WARPX_DIM_RZ
        ,{"theta", PIdx::theta}
#endif
    };
}

class WarpXParIter
    : public amrex::ParIter<0,0,PIdx::nattribs>
{
public:
    using amrex::ParIter<0,0,PIdx::nattribs>::ParIter;

    WarpXParIter (ContainerType& pc, int level);

#if (AMREX_SPACEDIM == 2)
    void GetPosition (amrex::Cuda::ManagedDeviceVector<amrex::ParticleReal>& x,
                      amrex::Cuda::ManagedDeviceVector<amrex::ParticleReal>& y,
                      amrex::Cuda::ManagedDeviceVector<amrex::ParticleReal>& z) const;
    void SetPosition (const amrex::Cuda::ManagedDeviceVector<amrex::ParticleReal>& x,
                      const amrex::Cuda::ManagedDeviceVector<amrex::ParticleReal>& y,
                      const amrex::Cuda::ManagedDeviceVector<amrex::ParticleReal>& z);
#endif
    const std::array<RealVector, PIdx::nattribs>& GetAttribs () const {
        return GetStructOfArrays().GetRealData();
    }

    std::array<RealVector, PIdx::nattribs>& GetAttribs () {
        return GetStructOfArrays().GetRealData();
    }

    const RealVector& GetAttribs (int comp) const {
        return GetStructOfArrays().GetRealData(comp);
    }

    RealVector& GetAttribs (int comp) {
        return GetStructOfArrays().GetRealData(comp);
    }

    IntVector& GetiAttribs (int comp) {
        return GetStructOfArrays().GetIntData(comp);
    }
};

class MultiParticleContainer;

class WarpXParticleContainer
    : public amrex::ParticleContainer<0,0,PIdx::nattribs>
{
public:
    friend MultiParticleContainer;

    // amrex::StructOfArrays with DiagIdx::nattribs amrex::ParticleReal components
    // and 0 int components for the particle data.
    using DiagnosticParticleData = amrex::StructOfArrays<DiagIdx::nattribs, 0>;
    // DiagnosticParticles is a vector, with one element per MR level.
    // DiagnosticParticles[lev] is typically a key-value pair where the key is
    // a pair [grid_index, tile_index], and the value is the corresponding
    // DiagnosticParticleData (see above) on this tile.
    using DiagnosticParticles = amrex::Vector<std::map<std::pair<int, int>, DiagnosticParticleData> >;

    WarpXParticleContainer (amrex::AmrCore* amr_core, int ispecies);
    virtual ~WarpXParticleContainer() {}

    virtual void InitData () = 0;

    virtual void FieldGatherES (const amrex::Vector<std::array<std::unique_ptr<amrex::MultiFab>, 3> >& E,
                                const amrex::Vector<std::unique_ptr<amrex::FabArray<amrex::BaseFab<int> > > >& masks) {}

    virtual void FieldGather (int lev,
                              const amrex::MultiFab& Ex, const amrex::MultiFab& Ey,
                              const amrex::MultiFab& Ez, const amrex::MultiFab& Bx,
                              const amrex::MultiFab& By, const amrex::MultiFab& Bz) {}

#ifdef WARPX_DO_ELECTROSTATIC
    virtual void EvolveES (const amrex::Vector<std::array<std::unique_ptr<amrex::MultiFab>, 3> >& E,
                                 amrex::Vector<std::unique_ptr<amrex::MultiFab> >& rho,
                           amrex::Real t, amrex::Real dt) = 0;
#endif // WARPX_DO_ELECTROSTATIC

    virtual void Evolve (int lev,
                         const amrex::MultiFab& Ex, const amrex::MultiFab& Ey, const amrex::MultiFab& Ez,
                         const amrex::MultiFab& Bx, const amrex::MultiFab& By, const amrex::MultiFab& Bz,
                         amrex::MultiFab& jx, amrex::MultiFab& jy, amrex::MultiFab& jz,
                         amrex::MultiFab* cjx, amrex::MultiFab* cjy, amrex::MultiFab* cjz,
                         amrex::MultiFab* rho, amrex::MultiFab* crho,
                         const amrex::MultiFab* cEx, const amrex::MultiFab* cEy, const amrex::MultiFab* cEz,
                         const amrex::MultiFab* cBx, const amrex::MultiFab* cBy, const amrex::MultiFab* cBz,
                         amrex::Real t, amrex::Real dt, DtType a_dt_type=DtType::Full) = 0;

    virtual void PostRestart () = 0;

    virtual void GetParticleSlice(const int direction,     const amrex::Real z_old,
                                  const amrex::Real z_new, const amrex::Real t_boost,
                                  const amrex::Real t_lab, const amrex::Real dt,
                                  DiagnosticParticles& diagnostic_particles) {}

    void AllocData ();

    ///
    /// This pushes the particle positions by one half time step.
    /// It is used to desynchronize the particles after initializaton
    /// or when restarting from a checkpoint.
    /// This is the electrostatic version of the particle push.
    ///
    void PushXES (amrex::Real dt);

    ///
    /// This pushes the particle positions by one half time step.
    /// It is used to desynchronize the particles after initializaton
    /// or when restarting from a checkpoint.
    /// This is the electromagnetic version of the particle push.
    ///
    void PushX (         amrex::Real dt);
    void PushX (int lev, amrex::Real dt);

    ///
    /// This pushes the particle momenta by dt.
    ///
    virtual void PushP (int lev, amrex::Real dt,
                        const amrex::MultiFab& Ex,
                        const amrex::MultiFab& Ey,
                        const amrex::MultiFab& Ez,
                        const amrex::MultiFab& Bx,
                        const amrex::MultiFab& By,
                        const amrex::MultiFab& Bz) = 0;

    void DepositCharge(amrex::Vector<std::unique_ptr<amrex::MultiFab> >& rho,
                       bool local = false);
    std::unique_ptr<amrex::MultiFab> GetChargeDensity(int lev, bool local = false);

    virtual void DepositCharge(WarpXParIter& pti,
                               RealVector& wp,
                               const int * const ion_lev,
                               amrex::MultiFab* rho,
                               int icomp,
                               const long offset,
                               const long np_to_depose,
                               int thread_num,
                               int lev,
                               int depos_lev);

    virtual void DepositCurrent(WarpXParIter& pti,
                                RealVector& wp,
                                RealVector& uxp,
                                RealVector& uyp,
                                RealVector& uzp,
                                const int * const ion_lev,
                                amrex::MultiFab* jx,
                                amrex::MultiFab* jy,
                                amrex::MultiFab* jz,
                                const long offset,
                                const long np_to_depose,
                                int thread_num,
                                int lev,
                                int depos_lev,
                                amrex::Real dt);

    // If particles start outside of the domain, ContinuousInjection
    // makes sure that they are initialized when they enter the domain, and
    // NOT before. Virtual function, overriden by derived classes.
    // Current status:
    // PhysicalParticleContainer: implemented.
    // LaserParticleContainer: implemented.
    // RigidInjectedParticleContainer: not implemented.
    virtual void ContinuousInjection(const amrex::RealBox& injection_box) {}
    // Update optional sub-class-specific injection location.
    virtual void UpdateContinuousInjectionPosition(amrex::Real dt) {}

    ///
    /// This returns the total charge for all the particles in this ParticleContainer.
    /// This is needed when solving Poisson's equation with periodic boundary conditions.
    ///
    amrex::Real sumParticleCharge(bool local = false);

    std::array<amrex::Real, 3> meanParticleVelocity(bool local = false);

    amrex::Real maxParticleVelocity(bool local = false);

    void AddNParticles (int lev,
                        int n, const amrex::ParticleReal* x, const amrex::ParticleReal* y, const amrex::ParticleReal* z,
                        const amrex::ParticleReal* vx, const amrex::ParticleReal* vy, const amrex::ParticleReal* vz,
                        int nattr, const amrex::ParticleReal* attr, int uniqueparticles, int id=-1);

    void AddOneParticle (int lev, int grid, int tile,
                         amrex::ParticleReal x, amrex::ParticleReal y, amrex::ParticleReal z,
                         std::array<amrex::ParticleReal,PIdx::nattribs>& attribs);

    void AddOneParticle (ParticleTileType& particle_tile,
                         amrex::ParticleReal x, amrex::ParticleReal y, amrex::ParticleReal z,
                         std::array<amrex::ParticleReal,PIdx::nattribs>& attribs);

    virtual void ReadHeader (std::istream& is);

    virtual void WriteHeader (std::ostream& os) const;

    virtual void ConvertUnits (ConvertDirection convert_dir){};

    static void ReadParameters ();

    static int NextID () { return ParticleType::NextID(); }

    void setNextID(int next_id) { ParticleType::NextID(next_id); }

    bool do_splitting = false;

    // split along diagonals (0) or axes (1)
    int split_type = 0;

    using amrex::ParticleContainer<0, 0, PIdx::nattribs>::AddRealComp;
    using amrex::ParticleContainer<0, 0, PIdx::nattribs>::AddIntComp;

    void AddRealComp (const std::string& name, bool comm=true)
    {
        particle_comps[name] = NumRealComps();
        AddRealComp(comm);
    }

    void AddIntComp (const std::string& name, bool comm=true)
    {
        particle_icomps[name] = NumIntComps();
        AddIntComp(comm);
    }

    int DoBoostedFrameDiags () const { return do_boosted_frame_diags; }

    virtual void buildIonizationMask (const amrex::MFIter& mfi, const int lev,
                                      amrex::Gpu::ManagedDeviceVector<int>& ionization_mask)
    {};

    std::map<std::string, int> getParticleComps () { return particle_comps;}

protected:

    std::map<std::string, int> particle_comps;
    std::map<std::string, int> particle_icomps;

    int species_id;

    amrex::Real charge;
    amrex::Real mass;

    //! instead of depositing (current, charge) on the finest patch level, deposit to the coarsest grid
    bool m_deposit_on_main_grid = false;

    //! instead of gathering fields from the finest patch level, gather from the coarsest
    bool m_gather_from_main_grid = false;

    static int do_not_push;

    // Whether to allow particles outside of the simulation domain to be
    // initialized when they enter the domain.
    // This is currently required because continuous injection does not
    // support all features allowed by direct injection.
    int do_continuous_injection = 0;

    int do_field_ionization = 0;
    int ionization_product;
    std::string ionization_product_name;
    int ion_atomic_number;
    int ionization_initial_level = 0;
    amrex::Gpu::ManagedVector<amrex::Real> ionization_energies;
    amrex::Gpu::ManagedVector<amrex::Real> adk_power;
    amrex::Gpu::ManagedVector<amrex::Real> adk_prefactor;
    amrex::Gpu::ManagedVector<amrex::Real> adk_exp_prefactor;
    std::string physical_element;

    int do_boosted_frame_diags = 1;

    amrex::Vector<amrex::FArrayBox> local_rho;
    amrex::Vector<amrex::FArrayBox> local_jx;
    amrex::Vector<amrex::FArrayBox> local_jy;
    amrex::Vector<amrex::FArrayBox> local_jz;

    using DataContainer = amrex::Gpu::ManagedDeviceVector<amrex::ParticleReal>;
    using PairIndex = std::pair<int, int>;

    amrex::Vector<DataContainer> m_xp, m_yp, m_zp;

    // Whether to dump particle quantities.
    // If true, particle position is always dumped.
    int plot_species = 1;
    // For all particle attribs (execept position), whether or not
    // to dump to file.
    amrex::Vector<int> plot_flags;
    // list of names of attributes to dump.
    amrex::Vector<std::string> plot_vars;

    amrex::Vector<std::map<PairIndex, std::array<DataContainer, TmpIdx::nattribs> > > tmp_particle_data;

private:
    virtual void particlePostLocate(ParticleType& p, const amrex::ParticleLocData& pld,
                                    const int lev) override;

};

#endif
