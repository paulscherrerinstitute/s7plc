TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS BELOW HERE

# Use typed rset structure (see 3.16.1 release notes)
USR_CPPFLAGS += -DUSE_TYPED_RSET

# Check for int64 support
ifneq ($(wildcard $(EPICS_BASE)/include/int64*Record.h),)
EPICS_INT64 = true
endif

# Documentation
HTMLS += s7plc.html

# Install xxxSupport.dbd into <top>/dbd
DBD += s7plc.dbd
s7plc_DBD += base.dbd
s7plc_DBD += s7plcBase.dbd
s7plc_DBD += s7plcCalcout.dbd
s7plc_DBD += s7plcReg.dbd
ifdef EPICS_INT64
	s7plc_DBD += s7plcInt64.dbd
endif

# Build an IOC support library
LIBRARY_IOC += s7plc

# Compile and add code to the support library
s7plc_SRCS += drvS7plc.c
s7plc_SRCS += devS7plc.c
ifdef EPICS_INT64
	s7plc_SRCS += devInt64S7plc.c
endif

s7plc_LIBS += $(EPICS_BASE_IOC_LIBS)

# Build the IOC application
PROD_IOC = S7plcApp

# S7plcApp.dbd will be created and installed
#DBD += S7plcApp.dbd
#
## S7plcApp.dbd will include these files:
#S7plcApp_DBD += base.dbd
#S7plcApp_DBD += s7plcBase.dbd
#S7plcApp_DBD += s7plcCalcout.dbd
#S7plcApp_DBD += s7plcReg.dbd
#ifdef EPICS_INT64
#	S7plcApp_DBD += s7plcInt64.dbd
#endif

# S7plcApp_registerRecordDeviceDriver.cpp derives from S7plcApp.dbd
S7plcApp_SRCS += s7plc_registerRecordDeviceDriver.cpp

# Build the main IOC entry point where needed
S7plcApp_SRCS_DEFAULT += S7plcMain.cpp
S7plcApp_SRCS_vxWorks += -nil-

# Link in the code from our support library
S7plcApp_LIBS += s7plc

# Finally link IOC to the EPICS Base libraries
S7plcApp_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD EXTRA GNUMAKE RULES BELOW HERE
