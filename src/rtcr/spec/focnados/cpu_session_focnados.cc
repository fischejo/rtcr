/*
 * \brief  Intercepting Cpu session
 * \author Denis Huber
 * \author Johannes Fischer
 * \date   2019-08-29
 */

#include <rtcr/cpu/cpu_session.h>

using namespace Rtcr;


void Cpu_session::deploy_queue(Genode::Dataspace_capability ds)
{
	_parent_cpu.deploy_queue(ds);
}


void Cpu_session::rq(Genode::Dataspace_capability ds)
{
	_parent_cpu.rq(ds);
}


void Cpu_session::dead(Genode::Dataspace_capability ds)
{
	_parent_cpu.dead(ds);
}


void Cpu_session::killed()
{
	_parent_cpu.killed();
}
