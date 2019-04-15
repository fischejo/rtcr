/*
 * \brief  Core module
 * \description This module provides a minimal implementation for a sucessful checkpoint/restore.
 * \author Johannes Fischer
 * \date   2019-04-04
 */

#include <rtcr_core/core_module.h>

using namespace Rtcr;

/* Create a static instance of the Core_module_factory. This registers the module */

Rtcr::Core_module_factory _core_module_factory_instance;


Core_module::Core_module(Genode::Env &env,
			 Genode::Allocator &md_alloc,
			 Genode::Entrypoint &ep,
			 const char* label,
			 bool &bootstrap,
			 Genode::Xml_node *config)
    :
    Core_module_pd(env, md_alloc, ep),
    Core_module_cpu(env, md_alloc, ep ), /* depends on Core_module_pd::Core_module_pd() */
    Core_module_rm(env, md_alloc, ep), /* depends on Core_module_pd::Core_module_pd() */
    Core_module_ram(env, md_alloc, ep), /* depends on Core_module_pd::Core_module_pd() */
    Core_module_rom(env, md_alloc, ep),
    Core_module_log(env, md_alloc, ep)    
{
    Core_module_pd::_init(label, bootstrap);
    Core_module_cpu::_init(label, bootstrap);
    Core_module_rm::_init(label, bootstrap);
    Core_module_ram::_init(label, 0, bootstrap);
    Core_module_rom::_init(label, bootstrap);
    Core_module_log::_init(label, bootstrap);      
}


void Core_module::initialize(Genode::List<Module> &modules)
{
  Module *module = modules.first();
  while (!_ds_module && module) {
    _ds_module = dynamic_cast<Dataspace_module*>(module);
    module = module->next();
  }

  if(!_ds_module)
    Genode::error("No Dataspace_module loaded! ");
}


void Core_module::checkpoint(Target_state &state)
{  
    /* initialize `kcap_mappings` variable. This step depends on Core_module_ram::Core_module_ram() */
    Core_module_pd::_create_kcap_mappings(state);

    /* initialize `region_map` variable. This step depends on Core_module_pd::Core_module_pd() */
    Core_module_rm::_create_region_map_dataspaces_list();

    /* Checkpointing */
    Core_module_pd::_checkpoint(state);  /* depends on Core_module_pd::_create_kcap_mappings */
    Core_module_cpu::_checkpoint(state);  /* depends on Core_module_pd::_create_kcap_mappings */
    Core_module_rm::_checkpoint(state);  /* depends on Core_module_pd::_create_kcap_mappings */

    /* depends on Core_module_pd::_create_kcap_mappings */
    /* depends on Core_module_rm::_create_region_map_dataspace_list */    
    Core_module_ram::_checkpoint(state);

    //    Core_module_ram::_checkpoint_temp_wrapper(state);
    Core_module_log::_checkpoint(state);
}


void Core_module::restore(Target_state &state)
{

}


void Core_module::pause()
{
    Core_module_cpu::_pause();
}


void Core_module::resume()
{
    Core_module_cpu::_resume();
}


Genode::Service *Core_module::resolve_session_request(const char *service_name,
						       const char *args)
{
    if(!Genode::strcmp(service_name, "PD")) {
	return &pd_service();
    } else if(!Genode::strcmp(service_name, "CPU")) {
	return &cpu_service();
    } else if(!Genode::strcmp(service_name, "RAM")) {
	return &ram_service();
    } else if(!Genode::strcmp(service_name, "LOG")) {
	return &log_service();	
    } else {
	Genode::Service *service = 0;	
	return service;
    }
}


