SRC_CC = target_child.cc module_factory.cc base_module.cc checkpointable.cc
SRC_CC += cpu_session.cc cpu_thread.cc
SRC_CC += pd_session.cc
SRC_CC += rm_session.cc region_map.cc
SRC_CC += rom_session.cc
SRC_CC += ram_session.cc
SRC_CC += log_session.cc
SRC_CC += timer_session.cc
SRC_CC += capability_mapping.cc


vpath % $(REP_DIR)/src/rtcr


CC_OPT += -DVERBOSE
CC_OPT += -DDEBUG

INC_DIR += $(BASE_DIR)/../base-foc/src/include
LIBS += config base
