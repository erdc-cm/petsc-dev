// 
// File:          Ex1_System_Impl.hh
// Symbol:        Ex1.System-v0.0.0
// Symbol Type:   class
// Babel Version: 0.10.12
// Description:   Server-side implementation for Ex1.System
// 
// WARNING: Automatically generated; only changes within splicers preserved
// 
// babel-version = 0.10.12
// 

#ifndef included_Ex1_System_Impl_hh
#define included_Ex1_System_Impl_hh

#ifndef included_sidl_cxx_hh
#include "sidl_cxx.hh"
#endif
#ifndef included_Ex1_System_IOR_h
#include "Ex1_System_IOR.h"
#endif
// 
// Includes for all method dependencies.
// 
#ifndef included_Ex1_System_hh
#include "Ex1_System.hh"
#endif
#ifndef included_gov_cca_CCAException_hh
#include "gov_cca_CCAException.hh"
#endif
#ifndef included_gov_cca_Services_hh
#include "gov_cca_Services.hh"
#endif
#ifndef included_sidl_BaseInterface_hh
#include "sidl_BaseInterface.hh"
#endif
#ifndef included_sidl_ClassInfo_hh
#include "sidl_ClassInfo.hh"
#endif


// DO-NOT-DELETE splicer.begin(Ex1.System._includes)
#include "TOPS.hh"
// DO-NOT-DELETE splicer.end(Ex1.System._includes)

namespace Ex1 { 

  /**
   * Symbol "Ex1.System" (version 0.0.0)
   */
  class System_impl
  // DO-NOT-DELETE splicer.begin(Ex1.System._inherits)
  // Insert-Code-Here {Ex1.System._inherits} (optional inheritance here)
  // DO-NOT-DELETE splicer.end(Ex1.System._inherits)
  {

  private:
    // Pointer back to IOR.
    // Use this to dispatch back through IOR vtable.
    System self;

    // DO-NOT-DELETE splicer.begin(Ex1.System._implementation)
    TOPS::Structured::Solver solver;
    gov::cca::Services myServices;
    // DO-NOT-DELETE splicer.end(Ex1.System._implementation)

  private:
    // private default constructor (required)
    System_impl() 
    {} 

  public:
    // sidl constructor (required)
    // Note: alternate Skel constructor doesn't call addref()
    // (fixes bug #275)
    System_impl( struct Ex1_System__object * s ) : self(s,true) { _ctor(); }

    // user defined construction
    void _ctor();

    // virtual destructor (required)
    virtual ~System_impl() { _dtor(); }

    // user defined destruction
    void _dtor();

    // static class initializer
    static void _load();

  public:

    /**
     * user defined non-static method.
     */
    void
    computeResidual (
      /* in */ ::sidl::array<double> x,
      /* in */ ::sidl::array<double> f
    )
    throw () 
    ;


    /**
     * Starts up a component presence in the calling framework.
     * @param services the component instance's handle on the framework world.
     * Contracts concerning Svc and setServices:
     * 
     * The component interaction with the CCA framework
     * and Ports begins on the call to setServices by the framework.
     * 
     * This function is called exactly once for each instance created
     * by the framework.
     * 
     * The argument Svc will never be nil/null.
     * 
     * Those uses ports which are automatically connected by the framework
     * (so-called service-ports) may be obtained via getPort during
     * setServices.
     */
    void
    setServices (
      /* in */ ::gov::cca::Services services
    )
    throw ( 
      ::gov::cca::CCAException
    );


    /**
     * Execute some encapsulated functionality on the component. 
     * Return 0 if ok, -1 if internal error but component may be 
     * used further, and -2 if error so severe that component cannot
     * be further used safely.
     */
    int32_t
    go() throw () 
    ;
  };  // end class System_impl

} // end namespace Ex1

// DO-NOT-DELETE splicer.begin(Ex1.System._misc)
// Insert-Code-Here {Ex1.System._misc} (miscellaneous things)
// DO-NOT-DELETE splicer.end(Ex1.System._misc)

#endif
