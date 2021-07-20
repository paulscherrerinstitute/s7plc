ifeq ($(wildcard /ioc/tools/driver.makefile),)
$(info If you are not using the PSI build environment, GNUmakefile can be removed.)
include Makefile
else
include /ioc/tools/driver.makefile

BUILDCLASSES+=vxWorks Linux WIN32
DBDS += S7plcApp/src/s7plcBase.dbd
DBDS_3.14 += S7plcApp/src/s7plcCalcout.dbd
DBDS_3.14 += S7plcApp/src/s7plcReg.dbd
SOURCES += S7plcApp/src/drvS7plc.c
SOURCES += S7plcApp/src/devS7plc.c
ifneq ($(wildcard $(EPICS_BASE)/include/int64*Record.h),)
DBDS += S7plcApp/src/s7plcInt64.dbd
SOURCES += S7plcApp/src/devInt64S7plc.c
endif
TEMPLATES=$(wildcard S7plcApp/Db/*.template)
USR_CPPFLAGS+=-DDEBUG
endif
