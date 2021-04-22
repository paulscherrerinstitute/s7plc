ifeq ($(wildcard /ioc/tools/driver.makefile),)
$(info If you are not using the PSI build environment, GNUmakefile can be removed.)
include Makefile
else
include /ioc/tools/driver.makefile

BUILDCLASSES+=Linux
DBDS += s7plcBase.dbd
DBDS_3.14 += s7plcCalcout.dbd
DBDS_3.14 += s7plcReg.dbd
SOURCES += drvS7plc.c
SOURCES += devS7plc.c
endif
