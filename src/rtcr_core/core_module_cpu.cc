/*
 * \brief  CPU implementation of core module
 * \author Johannes Fischer
 * \date   2019-03-21
 */

#include <rtcr_core/core_module_cpu.h>

#ifdef PROFILE
#include <util/profiler.h>
#define PROFILE_THIS_CALL PROFILE_FUNCTION("blue");
#else
#define PROFILE_THIS_CALL
#endif

#if DEBUG 
#define DEBUG_THIS_CALL Genode::log("\033[36m", __PRETTY_FUNCTION__, "\033[0m");
#else
#define DEBUG_THIS_CALL
#endif

using namespace Rtcr;


Core_module_cpu::Core_module_cpu(Genode::Env &env,
				 Genode::Allocator &alloc,
				 Genode::Entrypoint &ep,
				 Genode::Xml_node *config)
	:
	_env(env),
	_alloc(alloc),
	_ep(ep),
	_affinity_location(_affinity_location_from_config(config))
{}


Genode::Affinity::Location Core_module_cpu::_affinity_location_from_config(Genode::Xml_node *config)
{
	try {
		Genode::Xml_node node = config->sub_node("affinity");

		long const xpos = node.attribute_value<long>("xpos", 0);
		long const ypos = node.attribute_value<long>("ypos", 0);
		long const width = node.attribute_value<long>("width", 0);
		long const height = node.attribute_value<long>("height", 0);
		return Genode::Affinity::Location(xpos, ypos, width ,height);
	}
	catch (...) { return Genode::Affinity::Location(0, 0, 0, 0);}
}


void Core_module_cpu::_initialize_cpu_session(const char* label, bool &bootstrap)
{
	DEBUG_THIS_CALL PROFILE_THIS_CALL
		
	_cpu_root = new (_alloc) Cpu_root(_env, _alloc, _ep, pd_root(), bootstrap, _affinity_location);
	_cpu_service = new (_alloc) Genode::Local_service("CPU", _cpu_root);
	_cpu_session = _find_cpu_session(label, cpu_root());  
}


Cpu_session_component *Core_module_cpu::_find_cpu_session(const char *label, Cpu_root &cpu_root)
{
	DEBUG_THIS_CALL PROFILE_THIS_CALL
  
	/* Preparing argument string */
	char args_buf[160];
	Genode::snprintf(args_buf, sizeof(args_buf),
					 "priority=0x%x, ram_quota=%u, label=\"%s\"",
					 Genode::Cpu_session::DEFAULT_PRIORITY, 128*1024, label);
	
	/* Issuing session method of Cpu_root */
	Genode::Session_capability cpu_cap = cpu_root.session(args_buf, Genode::Affinity());

	/* Find created RPC object in Cpu_root's list */
	Cpu_session_component *cpu_session = cpu_root.session_infos().first();
	if(cpu_session) cpu_session = cpu_session->find_by_badge(cpu_cap.local_name());
	if(!cpu_session) {
		Genode::error("Creating custom PD session failed: "
					  "Could not find PD session in PD root");
		throw Genode::Exception();
	}

	return cpu_session;
}


Core_module_cpu::~Core_module_cpu() {
	DEBUG_THIS_CALL
	Genode::destroy(_alloc, _cpu_root);
	Genode::destroy(_alloc, _cpu_service);
}


void Core_module_cpu::_checkpoint()
{
	DEBUG_THIS_CALL PROFILE_THIS_CALL
		
	Genode::List<Cpu_session_component> &child_infos =  cpu_root().session_infos();
	Genode::List<Stored_cpu_session_info> &stored_infos = state()._stored_cpu_sessions;
	Cpu_session_component *child_info = nullptr;
	Stored_cpu_session_info *stored_info = nullptr;

	/* Update state_info from child_info
	 * If a child_info has no corresponding state_info, create it
	 */
	child_info = child_infos.first();
	while(child_info) {
		/* Find corresponding state_info */
		stored_info = stored_infos.first();
		if(stored_info) stored_info = stored_info->find_by_badge(child_info->cap().local_name());
	
		/* No corresponding stored_info => create it */
		if(!stored_info) {
			Genode::addr_t childs_kcap = find_kcap_by_badge(child_info->cap().local_name());
			stored_info = new (_alloc) Stored_cpu_session_info(*child_info, childs_kcap);
			stored_infos.insert(stored_info);
		}

		/* Update stored_info */
		stored_info->sigh_badge = child_info->parent_state().sigh.local_name();
		_prepare_cpu_threads(stored_info->stored_cpu_thread_infos,
							 child_info->parent_state().cpu_threads);

		child_info = child_info->next();
	}

	/* Delete old stored_infos, if the child misses corresponding infos in its list */
	stored_info = stored_infos.first();
	while(stored_info) {
		Stored_cpu_session_info *next_info = stored_info->next();

		/* Find corresponding child_info */
		child_info = child_infos.first();
		if(child_info) child_info = child_info->find_by_badge(stored_info->badge);

		/* No corresponding child_info => delete it */
		if(!child_info) {
			stored_infos.remove(stored_info);
			_destroy_stored_cpu_session(*stored_info);
		}

		stored_info = next_info;
	}
}


void Core_module_cpu::_destroy_stored_cpu_session(Stored_cpu_session_info &stored_info)
{
	DEBUG_THIS_CALL PROFILE_THIS_CALL
		
	while(Stored_cpu_thread_info *info = stored_info.stored_cpu_thread_infos.first()) {
		stored_info.stored_cpu_thread_infos.remove(info);
		_destroy_stored_cpu_thread(*info);
	}
	Genode::destroy(_alloc, &stored_info);
}


void Core_module_cpu::_prepare_cpu_threads(Genode::List<Stored_cpu_thread_info> &stored_infos,
										   Genode::List<Cpu_thread_component> &child_infos)
{
	DEBUG_THIS_CALL PROFILE_THIS_CALL
		
	Cpu_thread_component *child_info = nullptr;
	Stored_cpu_thread_info *stored_info = nullptr;

	/* Update state_info from child_info
	 * If a child_info has no corresponding state_info, create it
	 */

	child_info = child_infos.first();
	while(child_info) {
		/* Find corresponding state_info */
		stored_info = stored_infos.first();
		if(stored_info) stored_info = stored_info->find_by_badge(child_info->cap().local_name());

		/* No corresponding stored_info => create it */
		if(!stored_info) {
			Genode::addr_t childs_kcap = find_kcap_by_badge(child_info->cap().local_name());
			stored_info = new (_alloc) Stored_cpu_thread_info(*child_info, childs_kcap);
			stored_infos.insert(stored_info);
		}

		/* Update stored_info */
		stored_info->started = child_info->parent_state().started;
		stored_info->paused = child_info->parent_state().paused;
		stored_info->single_step = child_info->parent_state().single_step;
		stored_info->affinity = child_info->parent_state().affinity;
		stored_info->sigh_badge = child_info->parent_state().sigh.local_name();
		/* XXX does not guarantee to return the current thread registers */
		stored_info->ts = Genode::Cpu_thread_client(child_info->parent_cap()).state();

		child_info = child_info->next();
	}

	/* Delete old stored_infos, if the child misses corresponding infos in its list */
	stored_info = stored_infos.first();
	while(stored_info) {
		Stored_cpu_thread_info *next_info = stored_info->next();

		/* Find corresponding child_info */
		child_info = child_infos.first();
		if(child_info) child_info = child_info->find_by_badge(stored_info->badge);

		/* No corresponding child_info => delete it */
		if(!child_info) {
			stored_infos.remove(stored_info);
			_destroy_stored_cpu_thread(*stored_info);
		}

		stored_info = next_info;
	}
}


void Core_module_cpu::_destroy_stored_cpu_thread(Stored_cpu_thread_info &stored_info)
{
	DEBUG_THIS_CALL
	Genode::destroy(_alloc, &stored_info);
}


void Core_module_cpu::pause()
{
	DEBUG_THIS_CALL PROFILE_THIS_CALL
	/* Iterate through every session */
	Cpu_session_component *cpu_session = _cpu_root->session_infos().first();
	while(cpu_session) {
		/* Iterate through every CPU thread */
		Cpu_thread_component *cpu_thread = _cpu_session->parent_state().cpu_threads.first();
		while(cpu_thread) {
			/* Pause the CPU thread */
			Genode::Cpu_thread_client client{cpu_thread->parent_cap()};
			client.pause();

			cpu_thread = cpu_thread->next();
		}

		cpu_session = cpu_session->next();
	}
}


void Core_module_cpu::resume()
{
	DEBUG_THIS_CALL PROFILE_THIS_CALL
		
	/* Iterate through every session */
	Cpu_session_component *cpu_session = _cpu_root->session_infos().first();
	while(cpu_session) {
		/* Iterate through every CPU thread */
		Cpu_thread_component *cpu_thread = _cpu_session->parent_state().cpu_threads.first();
		while(cpu_thread) {
			/* Pause the CPU thread */
			Genode::Cpu_thread_client client{cpu_thread->parent_cap()};
			client.resume();

			cpu_thread = cpu_thread->next();
#ifdef DEBUG			
			Genode::log("\033[36m", __PRETTY_FUNCTION__, " cpu_thread=",cpu_thread, "\033[0m");
#endif			
		}

		cpu_session = cpu_session->next();
#ifdef DEBUG		
		Genode::log("\033[36m", __PRETTY_FUNCTION__, " cpu_session=",cpu_session, "\033[0m");
#endif		
	}
}
