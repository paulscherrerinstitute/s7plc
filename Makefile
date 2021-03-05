TOP=.

ifeq ($(wildcard $(TOP)/configure),)

# R3.13
include $(TOP)/config/CONFIG_APP
include $(TOP)/config/RULES_ARCHS

else

# R3.14+
include $(TOP)/configure/CONFIG

# Check for int64 support -> base >= 3.16.1
ifeq ($(shell expr $(EPICS_VERSION) \>= 3), 1)
	EPICS_INT64 = true
else
	ifeq ($(shell expr $(EPICS_REVISION) = 16), 1)
		ifeq ($(shell expr $(EPICS_MODIFICATION) \>= 1), 1)
		EPICS_INT64 = true
		endif
	endif
endif


# Want local functions non-static? Define DEBUG
#CFLAGS += -DDEBUG

# Suppress warning in EPICS 7
USR_CPPFLAGS += -DUSE_TYPED_RSET

# library

LIBRARY = s7plc
LIB_SRCS += drvS7plc.c
LIB_SRCS += devS7plc.c
ifdef EPICS_INT64
LIB_SRCS += devInt64S7plc.c
endif
HTMLS += s7plc.html
INSTALL_DBDS += $(INSTALL_DBD)/s7plc.dbd
# Uncomment this if you want a dynamically loadable module
#LIB_SRCS += s7plc_registerRecordDeviceDriver.cpp

# stand alone application program
PROD_DEFAULT = s7plcApp
PROD_vxWorks = -nil-
s7plcApp_SRCS += s7plcApp_registerRecordDeviceDriver.cpp
s7plcApp_SRCS += appMain.cc
s7plcApp_LIBS += s7plc
s7plcApp_LIBS += $(EPICS_BASE_IOC_LIBS)
s7plcApp_DBD += base.dbd
s7plcApp_DBD += s7plc.dbd
DBD += s7plcApp.dbd

include $(TOP)/configure/RULES

s7plcApp.dbd.d: s7plc.dbd

dbd_prerequisites = s7plcBase.dbd s7plcCalcout.dbd s7plcReg.dbd
ifdef EPICS_INT64
dbd_prerequisites := $(dbd_prerequisites) s7plcInt64.dbd
endif

vpath %.dbd ..
s7plc.dbd: $(dbd_prerequisites)
	cat $^ > $@
endif
