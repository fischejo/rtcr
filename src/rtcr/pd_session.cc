/*
 * \brief  Intercepting Pd session
 * \author Denis Huber
 * \author Johannes Fischer
 * \date   2019-08-29
 */

#include <rtcr/pd/pd_session.h>

#ifdef PROFILE
#include <util/profiler.h>
#define PROFILE_THIS_CALL PROFILE_FUNCTION("red");
#else
#define PROFILE_THIS_CALL
#endif

#if DEBUG 
#define DEBUG_THIS_CALL Genode::log("\e[38;5;196m", __PRETTY_FUNCTION__, "\033[0m");
#else
#define DEBUG_THIS_CALL
#endif


using namespace Rtcr;

Pd_session::Pd_session(Genode::Env &env,
                       Genode::Allocator &md_alloc,
                       Genode::Entrypoint &ep,
                       const char *creation_args,
                       Child_info *child_info)
	:
	Checkpointable(env, "pd_session"),
	Pd_session_info(creation_args, cap().local_name()),
	_env (env),
	_md_alloc (md_alloc),
	_ep (ep),
	_child_info (child_info),
	_parent_pd (env, child_info->name.string()),
	_address_space (_md_alloc,
	                _parent_pd.address_space(),
	                0,
	                "address_space",
	                child_info->bootstrapped),
	_stack_area (_md_alloc,
	             _parent_pd.stack_area(),
	             0,
	             "stack_area",
	             child_info->bootstrapped),
	_linker_area (_md_alloc,
	              _parent_pd.linker_area(),
	              0,
	              "linker_area",
	              child_info->bootstrapped)
{
	DEBUG_THIS_CALL;

	_ep.rpc_ep().manage(this);

	i_address_space = &_address_space;
	i_stack_area = &_stack_area;
	i_linker_area = &_linker_area;

	_ep.rpc_ep().manage(&_address_space);
	_ep.rpc_ep().manage(&_stack_area);
	_ep.rpc_ep().manage(&_linker_area);

	Genode::log("pd_session::creation_args=", creation_args);
}


Pd_session::~Pd_session()
{
	_ep.dissolve(_linker_area);
	_ep.dissolve(_stack_area);
	_ep.dissolve(_address_space);

	while(Signal_context_info *sc = _signal_contexts.first()) {
		_signal_contexts.remove(sc);
		Genode::destroy(_md_alloc, sc);
	}


	while(Signal_source_info *ss = _signal_sources.first()) {
		_signal_sources.remove(ss);
		Genode::destroy(_md_alloc, ss);
	}


	while(Native_capability_info *nc = _native_caps.first()) {
		_native_caps.remove(nc);
		Genode::destroy(_md_alloc, nc);
	}
}



void Pd_session::_checkpoint_signal_contexts()
{
	DEBUG_THIS_CALL PROFILE_THIS_CALL;

	Signal_context_info *sc = nullptr;
	while(sc = _destroyed_signal_contexts.dequeue()) {
		_signal_contexts.remove(sc);
		Genode::destroy(_md_alloc, &sc);
	}

	sc = _signal_contexts.first();
	while(sc) {
		static_cast<Signal_context*>(sc)->checkpoint();
		sc = sc->next();
	}

	i_signal_contexts = _signal_contexts.first();
}


void Pd_session::_checkpoint_signal_sources()
{
	DEBUG_THIS_CALL PROFILE_THIS_CALL;

	Signal_source_info *ss = nullptr;
	while(ss = _destroyed_signal_sources.dequeue()) {
		_signal_sources.remove(ss);
		Genode::destroy(_md_alloc, &ss);
	}

	/* Signal_source only stores const values. No need for checkpoint() */

	i_signal_sources = _signal_sources.first();
}


void Pd_session::_checkpoint_native_capabilities()
{
	DEBUG_THIS_CALL PROFILE_THIS_CALL;

	Native_capability_info *nc = nullptr;
	while(nc = _destroyed_native_caps.dequeue()) {
		_native_caps.remove(nc);
		Genode::destroy(_md_alloc, &nc);
	}

	/* Native_capability only stores const values. No need for
	   checkpoint() */

	i_native_caps = _native_caps.first();
}


void Pd_session::checkpoint()
{
	DEBUG_THIS_CALL PROFILE_THIS_CALL;
	i_upgrade_args = _upgrade_args;

	_address_space.checkpoint();
	_stack_area.checkpoint();
	_linker_area.checkpoint();

	_checkpoint_native_capabilities();
	_checkpoint_signal_sources();
	_checkpoint_signal_contexts();
}


void Pd_session::assign_parent(Genode::Capability<Genode::Parent> parent)
{
	DEBUG_THIS_CALL;
	_parent_pd.assign_parent(parent);
}


bool Pd_session::assign_pci(Genode::addr_t addr, Genode::uint16_t bdf)
{
	DEBUG_THIS_CALL;
	return _parent_pd.assign_pci(addr, bdf);
}


Genode::Capability<Genode::Signal_source> Pd_session::alloc_signal_source()
{
	DEBUG_THIS_CALL;
	auto result_cap = _parent_pd.alloc_signal_source();

	/* Create and insert list element to monitor this signal source */
	Signal_source *new_ss = new (_md_alloc) Signal_source(result_cap,
	                                                      _child_info->bootstrapped);

	Genode::Lock::Guard guard(_signal_sources_lock);
	_signal_sources.insert(new_ss);

	return result_cap;
}


void Pd_session::free_signal_source(Genode::Capability<Genode::Signal_source> cap)
{
	DEBUG_THIS_CALL;
	/* Find list element */
	Genode::Lock::Guard guard(_signal_sources_lock);
	Signal_source_info *ss = _signal_sources.first();
	if(ss) ss = ss->find_by_badge(cap.local_name());
	if(ss) {
		/* Free signal source */
		_parent_pd.free_signal_source(cap);
		_destroyed_signal_sources.enqueue(ss);
	} else {
		Genode::error("No list element found!");
	}
}


Genode::Signal_context_capability Pd_session::alloc_context(Signal_source_capability source,
                                                            unsigned long imprint)
{
	DEBUG_THIS_CALL;
	auto result_cap = _parent_pd.alloc_context(source, imprint);

	/* Create and insert list element to monitor this signal context */
	Signal_context *new_sc = new (_md_alloc) Signal_context(result_cap,
	                                                        source,
	                                                        imprint,
	                                                        _child_info->bootstrapped);

	Genode::Lock::Guard guard(_signal_contexts_lock);
	_signal_contexts.insert(new_sc);
	return result_cap;
}


void Pd_session::free_context(Genode::Signal_context_capability cap)
{
	/* Find list element */
	Genode::Lock::Guard guard(_signal_contexts_lock);
	Signal_context_info *sc = _signal_contexts.first();
	if(sc) sc = sc->find_by_badge(cap.local_name());
	if(sc) {
		/* Free signal context */
		_parent_pd.free_context(cap);
		_destroyed_signal_contexts.enqueue(sc);
	} else {
		Genode::error("No list element found!");
	}
}


void Pd_session::submit(Genode::Signal_context_capability context, unsigned cnt)
{
	_parent_pd.submit(context, cnt);
}


Genode::Native_capability Pd_session::alloc_rpc_cap(Genode::Native_capability ep)
{
	auto result_cap = _parent_pd.alloc_rpc_cap(ep);

	/* Create and insert list element to monitor this native_capability */
	Native_capability *new_nc = new (_md_alloc) Native_capability(result_cap, ep,
	                                                              _child_info->bootstrapped);

	Genode::Lock::Guard guard(_native_caps_lock);
	_native_caps.insert(new_nc);
	return result_cap;
}


void Pd_session::free_rpc_cap(Genode::Native_capability cap)
{
	/* Find list element */
	Genode::Lock::Guard guard(_native_caps_lock);
	Native_capability_info *nc = _native_caps.first();
	if(nc) nc = nc->find_by_native_badge(cap.local_name());
	if(nc) {
		/* Free native capability */
		_parent_pd.free_rpc_cap(cap);
		_destroyed_native_caps.enqueue(nc);
	} else {
		Genode::error("No list element found!");
	}
}


Genode::Capability<Genode::Region_map> Pd_session::address_space()
{
	return _address_space.Rpc_object<Genode::Region_map>::cap();
}


Genode::Capability<Genode::Region_map> Pd_session::stack_area()
{
	return _stack_area.Rpc_object<Genode::Region_map>::cap();
}


Genode::Capability<Genode::Region_map> Pd_session::linker_area()
{
	return _linker_area.Rpc_object<Genode::Region_map>::cap();
}


Genode::Capability<Genode::Pd_session::Native_pd> Pd_session::native_pd()
{
	return _parent_pd.native_pd();
}


void Pd_session::map(Genode::addr_t _addr, Genode::addr_t __addr)
{
	_parent_pd.map(_addr, __addr);
}


void Pd_session::ref_account(Genode::Capability<Genode::Pd_session> cap)
{
	_parent_pd.ref_account(cap);
}


void Pd_session::transfer_quota(Genode::Capability<Genode::Pd_session> cap,
                                Genode::Cap_quota quota)
{
	_parent_pd.transfer_quota(cap, quota);
}


void Pd_session::transfer_quota(Genode::Capability<Genode::Pd_session> cap,
                                Genode::Ram_quota quota)
{
	_parent_pd.transfer_quota(cap, quota);
}


Genode::Cap_quota Pd_session::cap_quota() const
{
	return _parent_pd.cap_quota();
}


Genode::Cap_quota Pd_session::used_caps() const
{
	return _parent_pd.used_caps();
}


Genode::Ram_quota Pd_session::ram_quota() const
{
	return _parent_pd.ram_quota();
}


Genode::Ram_quota Pd_session::used_ram() const
{
	return _parent_pd.used_ram();
}


Genode::Ram_dataspace_capability Pd_session::alloc(Genode::size_t size,
                                                   Genode::Cache_attribute cached)
{
	DEBUG_THIS_CALL;
	return _parent_pd.alloc(size, cached);
}


void Pd_session::free(Genode::Ram_dataspace_capability ram_cap)
{
	DEBUG_THIS_CALL;
	_parent_pd.free(ram_cap);
}


Genode::size_t Pd_session::dataspace_size(Genode::Ram_dataspace_capability cap) const
{
	return _parent_pd.dataspace_size(cap);
}


Pd_factory::Pd_factory(Genode::Env &env,
                       Genode::Allocator &md_alloc,
                       Genode::Entrypoint &ep,
                       Genode::Lock &childs_lock,
                       Genode::List<Child_info> &childs)
	:
	_env              (env),
	_md_alloc         (md_alloc),
	_ep               (ep),
	_childs_lock(childs_lock),
	_childs(childs),
	_service(*this)
{
	DEBUG_THIS_CALL PROFILE_THIS_CALL;
}


Pd_session *Pd_factory::_create(Child_info *info, const char *args)
{
	return new (_md_alloc) Pd_session(_env, _md_alloc, _ep, args, info);
}


Pd_session &Pd_factory::create(Genode::Session_state::Args const &args,
                               Genode::Affinity)
{
	DEBUG_THIS_CALL;

	char label_buf[160];
	Genode::Arg label_arg = Genode::Arg_string::find_arg(args.string(), "label");
	label_arg.string(label_buf, sizeof(label_buf), "");

	_childs_lock.lock();
	Child_info *info = _childs.first();
	if(info) info = info->find_by_name(label_buf);
	if(!info) {
		info = new(_md_alloc) Child_info(label_buf);
		_childs.insert(info);
	}
	_childs_lock.unlock();

	/* Create custom Pd_session */
	Pd_session *new_session = _create(info, args.string());
	Capability_mapping *cap_mapping = new(_md_alloc) Capability_mapping(_env,
	                                                                    _md_alloc,
	                                                                    new_session);

	info->pd_session = new_session;
	info->capability_mapping = cap_mapping;
	return *new_session;
}


void Pd_factory::upgrade(Pd_session&, Genode::Session_state::Args const &)
{
	DEBUG_THIS_CALL;
	// char ram_quota_buf[32];
	// char new_upgrade_args[160];

	// Genode::strncpy(new_upgrade_args, session->upgrade_args(), sizeof(new_upgrade_args));

	// Genode::size_t ram_quota = Genode::Arg_string::find_arg(new_upgrade_args, "ram_quota").ulong_value(0);
	// Genode::size_t extra_ram_quota = Genode::Arg_string::find_arg(upgrade_args, "ram_quota").ulong_value(0);
	// ram_quota += extra_ram_quota;

	// Genode::snprintf(ram_quota_buf, sizeof(ram_quota_buf), "%zu", ram_quota);
	// Genode::Arg_string::set_arg(new_upgrade_args, sizeof(new_upgrade_args), "ram_quota", ram_quota_buf);

	// _env.parent().upgrade(Genode::Parent::Env::pd(), upgrade_args);
	// session->upgrade(upgrade_args);
}


void Pd_factory::destroy(Pd_session&)
{
	DEBUG_THIS_CALL;
	// Genode::Lock::Guard lock(_childs_lock);
	// Child_info *info = _childs.first();
	// while(info) {
	// 	Genode::destroy(_md_alloc, info->pd_session);		
	// 	info->pd_session = nullptr;
	// 	if(info->child_destroyed()) _childs.remove(info);
	// 	info = info->next();
	// }	  
}

