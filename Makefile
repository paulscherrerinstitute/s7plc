TOP=..

ifeq ($(wildcard $(TOP)/configure),)

# R3.13
include $(TOP)/config/CONFIG_APP
include $(TOP)/config/RULES_ARCHS

else

# R3.14
include $(TOP)/configure/CONFIG

# Allow for debugging
HOST_OPT = NO

# Want local functions non-static? Define DEBUG 
#CFLAGS += -DDEBUG

# library

LIBRARY = s7plc
LIB_SRCS += s7plc_registerRecordDeviceDriver.cpp
LIB_SRCS += drvS7plc.c
LIB_SRCS += devS7plc.c
LIB_LIBS += $(EPICS_BASE_IOC_LIBS)
HTMLS += s7plc.html
INSTALL_DBDS += $(INSTALL_DBD)/s7plc.dbd
# stand alone application program

PROD_DEFAULT = s7plcApp
PROD_vxWorks = -nil-
s7plcApp_SRCS += s7plcApp_registerRecordDeviceDriver.cpp
s7plcApp_SRCS += appMain.cc
s7plcApp_LIBS += s7plc
s7plcApp_LIBS += $(EPICS_BASE_IOC_LIBS)
s7plcApp_DBD += base.dbd
s7plcApp_DBD += s7plcBase.dbd
s7plcApp_DBD += s7plcCalcout.dbd
s7plcApp_DBD += s7plcReg.dbd
DBD += s7plcApp.dbd

include $(TOP)/configure/RULES

s7plc.dbd: s7plcBase.dbd s7plcCalcout.dbd s7plcReg.dbd
	cat $^ > $@
endif
