/*
 * \brief  Ram implementation of core module
 * \author Johannes Fischer
 * \date   2019-04-04
 */

#include <rtcr_core/core_module_ram.h>

using namespace Rtcr;


Core_module_ram::Core_module_ram(Genode::Env &env,
				 Genode::Allocator &md_alloc,
				 Genode::Entrypoint &ep)
    :
    _env(env),
    _md_alloc(md_alloc),
    _ep(ep)
 {
 
}  

void Core_module_ram::_init(const char* label,
			    Genode::size_t granularity,
			    bool &bootstrap)
{
  _ram_root = new (_md_alloc) Ram_root(_env, _md_alloc, _ep, granularity, bootstrap);
  _ram_service = new (_md_alloc) Genode::Local_service("RAM", _ram_root);
  _ram_session = _find_session(label, ram_root());
}

Ram_session_component *Core_module_ram::_find_session(const char *label, Ram_root &ram_root)
{
#ifdef DEBUG
    Genode::log("Ckpt::\033[33m", __func__, "\033[0m()");
#endif
    
    /* Preparing argument string */
    char args_buf[160];
    Genode::snprintf(args_buf, sizeof(args_buf),
		     "ram_quota=%u, phys_start=0x%lx, phys_size=0x%lx, label=\"%s\"",
		     4*1024*sizeof(long), 0UL, 0UL, label);

    /* Issuing session method of Ram_root */
    Genode::Session_capability ram_cap = ram_root.session(args_buf, Genode::Affinity());

    /* Find created RPC object in Ram_root's list */
    Ram_session_component *ram_session = ram_root.session_infos().first();
    if(ram_session) ram_session = ram_session->find_by_badge(ram_cap.local_name());
    if(!ram_session) {
	Genode::error("Creating custom RAM session failed: Could not find RAM session in RAM root");
	throw Genode::Exception();
    }

    return ram_session;
}


Core_module_ram::~Core_module_ram()
{

}


void Core_module_ram::_checkpoint(Target_state &state)
{
#ifdef VERBOSE
    Genode::log("Ckpt::\033[33m", __func__, "\033[0m()");
#endif    
    Genode::List<Ram_session_component> &child_infos =  _ram_root->session_infos();
    Genode::List<Stored_ram_session_info> &stored_infos = state._stored_ram_sessions;    
    Ram_session_component *child_info = nullptr;
    Stored_ram_session_info *stored_info = nullptr;

    /* Update state_info from child_info */
    /* If a child_info has no corresponding state_info, create it */
    child_info = child_infos.first();
    while(child_info) {
	/* Find corresponding state_info */
	stored_info = stored_infos.first();
	if(stored_info) stored_info = stored_info->find_by_badge(child_info->cap().local_name());

	/* No corresponding stored_info => create it */
	if(!stored_info) {
	    Genode::addr_t childs_kcap = find_kcap_by_badge(child_info->cap().local_name());
	    stored_info = new (state._alloc) Stored_ram_session_info(*child_info, childs_kcap);
	    stored_infos.insert(stored_info);
	}

	/* Update stored_info */
	_prepare_ram_dataspaces(state,
				stored_info->stored_ramds_infos,
				child_info->parent_state().ram_dataspaces);

	child_info = child_info->next();
    }

    /* Delete old stored_infos, if the child misses corresponding infos in its list */
    stored_info = stored_infos.first();
    while(stored_info) {
	Stored_ram_session_info *next_info = stored_info->next();

	/* Find corresponding child_info */
	child_info = child_infos.first();
	if(child_info) child_info = child_info->find_by_badge(stored_info->badge);

	/* No corresponding child_info => delete it */
	if(!child_info) {
	    stored_infos.remove(stored_info);
	    _destroy_stored_ram_session(state, *stored_info);
	}

	stored_info = next_info;
    }
}


void Core_module_ram::_destroy_stored_ram_session(Target_state &state,
						  Stored_ram_session_info &stored_info)
{
#ifdef VERBOSE
    Genode::log("Ckpt::\033[33m", __func__, "\033[0m()");
#endif    

    while(Stored_ram_dataspace_info *info = stored_info.stored_ramds_infos.first()) {
	stored_info.stored_ramds_infos.remove(info);
	_destroy_stored_ram_dataspace(state, *info);
    }
    Genode::destroy(state._alloc, &stored_info);
}


void Core_module_ram::_prepare_ram_dataspaces(Target_state &state,
					      Genode::List<Stored_ram_dataspace_info> &stored_infos,
					      Genode::List<Ram_dataspace_info> &child_infos)
{
#ifdef VERBOSE
    Genode::log("Ckpt::\033[33m", __func__, "\033[0m()");
#endif    

    Ram_dataspace_info *child_info = nullptr;
    Stored_ram_dataspace_info *stored_info = nullptr;

    /* Update state_info from child_info
     * If a child_info has no corresponding state_info, create it */
    child_info = child_infos.first();
    while(child_info) {
	/* Find corresponding state_info */
	stored_info = stored_infos.first();
	if(stored_info) stored_info = stored_info->find_by_badge(child_info->cap.local_name());

	/* No corresponding stored_info => create it */
	if(!stored_info) {
	    stored_info = &_create_stored_ram_dataspace(state, *child_info);
	    stored_infos.insert(stored_info);
	}

	/* Nothing to update in stored_info */

	/* Remeber this dataspace for checkpoint, if not already in list */
	Dataspace_translation_info *trans_info = _dataspace_translations.first();
	if(trans_info) trans_info = trans_info->find_by_resto_badge(child_info->cap.local_name());
	if(!trans_info) {
	    trans_info = new (_md_alloc) Dataspace_translation_info(stored_info->memory_content,
								 child_info->cap,
								 child_info->size);
	    _dataspace_translations.insert(trans_info);
	}

	child_info = child_info->next();
    }

    /* Delete old stored_infos, if the child misses corresponding infos in its list */
    stored_info = stored_infos.first();
    while(stored_info) {
	Stored_ram_dataspace_info *next_info = stored_info->next();

	/* Find corresponding child_info */
	child_info = child_infos.first();
	if(child_info) child_info = child_info->find_by_badge(stored_info->badge);

	/* No corresponding child_info => delete it */
	if(!child_info) {
	    stored_infos.remove(stored_info);
	    _destroy_stored_ram_dataspace(state, *stored_info);
	}

	stored_info = next_info;
    }
}


Stored_ram_dataspace_info &Core_module_ram::_create_stored_ram_dataspace(Target_state &state,
									 Ram_dataspace_info &child_info)
{
#ifdef VERBOSE
    Genode::log("Ckpt::\033[33m", __func__, "\033[0m()");
#endif    


    /* The dataspace with the memory content of the ram dataspace will be
     * referenced by the stored ram dataspace */
    Genode::Ram_dataspace_capability ramds_cap;

    /* Exclude dataspaces which are known region maps (except managed dataspaces
     * from the incremental checkpoint mechanism) */
    
//    Ref_badge_info *region_map_dataspace = _region_maps.first();
//    if(region_map_dataspace) region_map_dataspace = region_map_dataspace->find_by_badge(child_info.cap.local_name());
    Ref_badge_info *region_map_dataspace = find_region_map_by_badge(child_info.cap.local_name());

    if(region_map_dataspace) {
	Genode::log("Dataspace ", child_info.cap, " is a region map.");
    } else {
	/* Check whether the dataspace is somewhere in the stored session RPC objects */
	ramds_cap = Genode::reinterpret_cap_cast<Genode::Ram_dataspace>(state.find_stored_dataspace(
									    child_info.cap.local_name()));

	if(!ramds_cap.valid()) {
	    Genode::log("Dataspace ", child_info.cap, " is not known. "
			"Creating dataspace with size ", Genode::Hex(child_info.size));
	    ramds_cap = state._env.ram().alloc(child_info.size);
	} else {
	    Genode::log("Dataspace ", child_info.cap, " is known from last checkpoint.");
	}
    }

    Genode::addr_t childs_kcap = find_kcap_by_badge(child_info.cap.local_name());
    return *new (state._alloc) Stored_ram_dataspace_info(child_info, childs_kcap, ramds_cap);
}


void Core_module_ram::_destroy_stored_ram_dataspace(Target_state &state,
						    Stored_ram_dataspace_info &stored_info)
{
#ifdef VERBOSE
    Genode::log("Ckpt::\033[33m", __func__, "\033[0m()");
#endif    
    
    /* Pre-condition: This stored object is removed from its list, thus, a
     * search for a stored dataspace will not return its memory content
     * dataspace */
    Genode::Dataspace_capability stored_ds_cap = state.find_stored_dataspace(stored_info.badge);
    if(!stored_ds_cap.valid()) {
	state._env.ram().free(stored_info.memory_content);
    }

    Genode::destroy(state._alloc, &stored_info);
}




void Core_module_ram::_checkpoint_temp_wrapper(Target_state &state)
{
#ifdef DEBUG
    Genode::log("Dataspaces to checkpoint:");
    Dataspace_translation_info *info = _dataspace_translations.first();
    while(info) {
	Genode::log(" ", *info);
	info = info->next();
    }

#endif

    /* Create a list of managed dataspaces */
    _create_managed_dataspace_list();

#ifdef DEBUG
    Genode::log("Managed dataspaces:");
    Simplified_managed_dataspace_info const *smd_info = _managed_dataspaces.first();
    if(!smd_info) Genode::log(" <empty>\n");
    while(smd_info) {
	Genode::log(" ", *smd_info);

	Simplified_managed_dataspace_info::Simplified_designated_ds_info const *sdd_info =
	    smd_info->designated_dataspaces.first();
	if(!sdd_info) Genode::log("  <empty>\n");
	while(sdd_info) {
	    Genode::log("  ", *sdd_info);
	    sdd_info = sdd_info->next();
	}

	smd_info = smd_info->next();
    }
#endif

    /* Detach all designated dataspaces */
    _detach_designated_dataspaces();

    /* Copy child dataspaces' content and to stored dataspaces' content */
    _checkpoint_dataspaces(state);
}



void Core_module_ram::_create_managed_dataspace_list()
{
#ifdef DEBUG
    Genode::log("Resto::\033[33m", __func__, "\033[0m(...)");
#endif

    Genode::List<Ram_session_component> &ram_sessions = _ram_root->session_infos();
    typedef Simplified_managed_dataspace_info::Simplified_designated_ds_info Sim_dd_info;

    Ram_session_component *ram_session = ram_sessions.first();
    while(ram_session) {
	Ram_dataspace_info *ramds_info = ram_session->parent_state().ram_dataspaces.first();
	while(ramds_info) {
	    // RAM dataspace is managed
	    if(ramds_info->mrm_info) {
		Genode::List<Sim_dd_info> sim_dd_infos;
		Designated_dataspace_info *dd_info = ramds_info->mrm_info->dd_infos.first();
		while(dd_info) {
		    Genode::Ram_dataspace_capability dd_info_cap =
			Genode::reinterpret_cap_cast<Genode::Ram_dataspace>(dd_info->cap);

		    sim_dd_infos.insert(new (_md_alloc) Sim_dd_info(dd_info_cap,
								 dd_info->rel_addr,
								 dd_info->size,
								 dd_info->attached));

		    dd_info = dd_info->next();
		}

		_managed_dataspaces.insert(new (_md_alloc) Simplified_managed_dataspace_info(ramds_info->cap,
											  sim_dd_infos));
	    }

	    ramds_info = ramds_info->next();
	}

	ram_session = ram_session->next();
    }
}


void Core_module_ram::_detach_designated_dataspaces()
{
#ifdef VERBOSE
    Genode::log("Ckpt::\033[33m", __func__, "\033[0m(...)");
#endif

    Genode::List<Ram_session_component> &ram_sessions = _ram_root->session_infos();
    Ram_session_component *ram_session = ram_sessions.first();
    while(ram_session) {
	Ram_dataspace_info *ramds_info = ram_session->parent_state().ram_dataspaces.first();
	while(ramds_info) {
	    if(ramds_info->mrm_info) {
		Designated_dataspace_info *dd_info = ramds_info->mrm_info->dd_infos.first();
		while(dd_info) {
		    if(dd_info->attached) dd_info->detach();
		    dd_info = dd_info->next();
		}
	    }
	    ramds_info = ramds_info->next();
	}
	ram_session = ram_session->next();
    }
}




void Core_module_ram::_checkpoint_dataspaces(Target_state &state)
{
#ifdef VERBOSE
    Genode::log("Ckpt::\033[33m", __func__, "\033[0m()");
#endif    

    Dataspace_translation_info *memory_info = _dataspace_translations.first();
    while(memory_info) {
	if(!memory_info->processed) {
	    /* Resolve managed dataspace of the incremental checkpointing mechanism */
	    Simplified_managed_dataspace_info *smd_info = _managed_dataspaces.first();
	    if(smd_info) smd_info = smd_info->find_by_badge(memory_info->resto_ds_cap.local_name());

	    if(smd_info) {
		/* Dataspace is managed */			
		Simplified_managed_dataspace_info::Simplified_designated_ds_info *sdd_info =
		    smd_info->designated_dataspaces.first();
		while(sdd_info) {
		    if(sdd_info->modified) {
			_checkpoint_dataspace_content(state,
						      memory_info->ckpt_ds_cap,
						      sdd_info->dataspace_cap,
						      sdd_info->addr,
						      sdd_info->size);
		    }

		    sdd_info = sdd_info->next();
		}

	    } else {
		/* Dataspace is not managed */
		_checkpoint_dataspace_content(state,
					      memory_info->ckpt_ds_cap,
					      memory_info->resto_ds_cap,
					      0,
					      memory_info->size);
	    }

	    memory_info->processed = true;
	}

	memory_info = memory_info->next();
    }
}


void Core_module_ram::_checkpoint_dataspace_content(Target_state &state,
						    Genode::Dataspace_capability dst_ds_cap,
						    Genode::Dataspace_capability src_ds_cap,
						    Genode::addr_t dst_offset,
						    Genode::size_t size)
{
#ifdef DEBUG
    Genode::log("Ckpt::\033[33m", __func__, "\033[0m(dst ", dst_ds_cap,
		", src ", src_ds_cap, ", dst_offset=", Genode::Hex(dst_offset),
		", copy_size=", Genode::Hex(size), ")");
#endif
    
    char *dst_addr_start = state._env.rm().attach(dst_ds_cap);
    char *src_addr_start = state._env.rm().attach(src_ds_cap);

    Genode::memcpy(dst_addr_start + dst_offset, src_addr_start, size);

    state._env.rm().detach(src_addr_start);
    state._env.rm().detach(dst_addr_start);
}


