SRC_CC = module_factory.cc base_module.cc init_module.cc checkpointable.cc child_info.cc child.cc
SRC_CC += cpu_thread.cc
SRC_CC += pd_session.cc
SRC_CC += rm_session.cc region_map.cc
SRC_CC += rom_session.cc
SRC_CC += log_session.cc
SRC_CC += timer_session.cc
SRC_CC += cpu_session.cc
vpath % $(REP_DIR)/src/rtcr

ifeq ($(filter-out $(SPECS),focnados),)
vpath % $(REP_DIR)/src/rtcr/spec/focnados
SRC_CC += capability_mapping_focnados.cc cpu_session_focnados.cc
INC_DIR += $(BASE_DIR)/../base-focnados/src/include
LIBS += syscall-foc
endif

ifeq ($(filter-out $(SPECS),sel4),)
vpath % $(REP_DIR)/src/rtcr/spec/sel4
SRC_CC += capability_mapping_sel4.cc cpu_session_sel4.cc
endif

ifeq ($(filter-out $(SPECS),foc),)
vpath % $(REP_DIR)/src/rtcr/spec/foc
SRC_CC += capability_mapping_foc.cc cpu_session_foc.cc
endif


LIBS += base
CC_OPT += -w

LIBS += rtcr_inc rtcr_para rtcr_cdma
