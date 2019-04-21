/*
 * \brief  CPU Session Handler
 * \author Johannes Fischer
 * \date   2019-03-21
 */

#ifndef _RTCR_CORE_MODULE_PD_H_
#define _RTCR_CORE_MODULE_PD_H_

/* Genode includes */
#include <base/component.h>
#include <base/log.h>
#include <base/service.h>
#include <base/env.h>
#include <base/child.h>
#include <base/service.h>
#include <base/snprintf.h>
#include <util/list.h>


#include <rtcr_core/core_module_base.h>
#include <rtcr/target_state.h>
#include <rtcr_core/pd/pd_session.h>
#include <rtcr_core/pd/ref_badge_info.h>
#include <rtcr/kcap_badge_info.h>

#include <foc_native_pd/client.h>


namespace Rtcr {
    class Core_module_pd;
}

using namespace Rtcr;

class Rtcr::Core_module_pd : public virtual Rtcr::Core_module_base
{
private:
    Genode::Env        &_env;
    Genode::Allocator  &_md_alloc;
    Genode::Entrypoint &_ep;

protected:
    Pd_root *_pd_root;
    Genode::Local_service *_pd_service;
    Pd_session_component *_pd_session;

    /**
     * Capability map in a condensed form
     * Refactored from `checkpointer.h`
     */
    Genode::List<Kcap_badge_info> _kcap_mappings;

    /**
     * Refactored from `target.child.h`. Previously `Target_child::Resources::_init_pd()`
     */
    Pd_session_component *_find_pd_session(const char *label, Pd_root &pd_root);
    void _initialize_pd_session(const char* label, bool &bootstrap);

    /**
     * \brief Destroys the _kcap_mappings list.
     */
    void _destroy_list(Genode::List<Kcap_badge_info> &list);
    void _destroy_list(Genode::List<Ref_badge_info> &list);

    /**
     * \brief Prepares the capability map state_infos
     *
     * First the method fetches the capability map information from child's cap map structure in an
     * intercepted dataspace.
     *
     * Second it prepares the capability map state_infos.
     * For each badge-kcap tuple found in child's cap map the method checks whether a corresponding
     * list element in state_infos exists. If there is no list element, then it is created and marked.
     * Else it is just marked. After this, the old badge-kcap tuples, which where not marked, are deleted
     * from state_infos. Now an updated capability map is ready to used for the next steps to store the
     * kcap for each RPC object.
     */


   
    void _destroy_stored_pd_session(Target_state &state, Stored_pd_session_info &stored_info);

    void _prepare_native_caps(Target_state &state,
			      Genode::List<Stored_native_capability_info> &stored_infos,
			      Genode::List<Native_capability_info> &child_infos);
  
    void _destroy_stored_native_cap(Target_state &state, Stored_native_capability_info &stored_info);

    void _prepare_signal_sources(Target_state &state,
				 Genode::List<Stored_signal_source_info> &stored_infos,
				 Genode::List<Signal_source_info> &child_infos);
    void _destroy_stored_signal_source(Target_state &state, Stored_signal_source_info &stored_info);

    void _prepare_signal_contexts(Target_state &state,
				  Genode::List<Stored_signal_context_info> &stored_infos,
				  Genode::List<Signal_context_info> &child_infos);
    void _destroy_stored_signal_context(Target_state &state, Stored_signal_context_info &stored_info);

  /* This method is an exact copy from core_module_rm. It is copied in order to
     minimize the dependencies between both classes. */
    void _destroy_stored_region_map(Target_state &state,
				    Stored_region_map_info &stored_info);

  /* This method is an exact copy from core_module_rm. It is copied in order to
     minimize the dependencies between both classes. */
    void _destroy_stored_attached_region(Target_state &state,
					 Stored_attached_region_info &stored_info);

    // level: 1
    void _create_kcap_mappings(Target_state &state);
    
    void _checkpoint(Target_state &state);

public:    

  /* implement virtual methods of Core_module_base */
  virtual Pd_root &pd_root();
  

  /* implement virtual method of Core_module_base */
  virtual Genode::addr_t find_kcap_by_badge(Genode::uint16_t badge);	
  
  //public:

  virtual Genode::Local_service &pd_service() {
    return *_pd_service;
  };
  
  virtual Genode::Rpc_object<Genode::Pd_session> &pd_session() {
    return *_pd_session;
  };
  
    Core_module_pd(Genode::Env &env,
	      Genode::Allocator &md_alloc,
		   Genode::Entrypoint &ep);

    ~Core_module_pd();

};


#endif /* _RTCR_CORE_MODULE_PD_H_ */
