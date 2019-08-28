/*
 * \brief  Monitoring PD::alloc_context and PD::free_context
 * \author Denis Huber
 * \date   2016-10-06
 */

#ifndef _RTCR_SIGNAL_CONTEXT_INFO_H_
#define _RTCR_SIGNAL_CONTEXT_INFO_H_

/* Genode includes */
#include <util/list.h>
#include <base/signal.h>

/* Rtcr includes */
#include <rtcr/info_structs.h>

namespace Rtcr {
	struct Signal_context_info;
}

/**
 * List element to store Signal_context_capabilities created by the pd session
 */
struct Rtcr::Signal_context_info : Genode::List<Signal_context_info>::Element
{
  	Genode::uint16_t ck_signal_source_badge;
	unsigned long ck_imprint;

        bool ck_bootstrapped;
        Genode::addr_t ck_kcap;
	Genode::uint16_t ck_badge;
  
  
	// Creation arguments and result
	Genode::Signal_context_capability         const cap;
	Genode::Capability<Genode::Signal_source> const ss_cap;
	unsigned long                             const imprint;
  bool bootstrapped;
  
	Signal_context_info(Genode::Signal_context_capability sc_cap,
			    Genode::Capability<Genode::Signal_source> ss_cap,
			    unsigned long imprint,
			    bool bootstrapped)
		:
		bootstrapped(bootstrapped),
		cap     (sc_cap),
		ss_cap  (ss_cap),
		imprint (imprint)
	{ }

  void checkpoint()
  {
    ck_badge = cap.local_name();
    ck_bootstrapped = bootstrapped;
    ck_imprint = imprint;
    ck_signal_source_badge = cap.local_name();
  }
  
	Signal_context_info *find_by_badge(Genode::uint16_t badge)
	{
		if(badge == cap.local_name())
			return this;
		Signal_context_info *info = next();
		return info ? info->find_by_badge(badge) : 0;
	}

	void print(Genode::Output &output) const
	{
		using Genode::Hex;

		Genode::print(output, "sc ", cap, ", ss ", ss_cap, ", imprint=", Hex(imprint), ", ");
	}
};


#endif /* _RTCR_SIGNAL_CONTEXT_INFO_H_ */