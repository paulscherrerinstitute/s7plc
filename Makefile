TOP=.

ifeq ($(wildcard $(TOP)/configure),)

# R3.13
include $(TOP)/config/CONFIG_APP
include $(TOP)/config/RULES_ARCHS

else

# R3.14+
include $(TOP)/configure/CONFIG

# Want local functions non-static? Define DEBUG 
#CFLAGS += -DDEBUG

# Suppress warning in EPICS 7
USR_CPPFLAGS += -DUSE_TYPED_RSET

# library

LIBRARY = s7plc
LIB_SRCS += drvS7plc.c
LIB_SRCS += devS7plc.c
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

vpath %.dbd ..
s7plc.dbd: s7plcBase.dbd s7plcCalcout.dbd s7plcReg.dbd
	cat $^ > $@
endif
