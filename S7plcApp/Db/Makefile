TOP=../..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS BELOW HERE

# Install databases, templates & substitutions like this
DB += example.db
DB += example_int64.db
DB += s7plcAnalogIn.template
DB += s7plcAnalogOut.template
DB += s7plcBitIn.template
DB += s7plcBitOut.template
DB += s7plcTime.template

# If <anyname>.db template is not named <anyname>*.template add
# <anyname>_TEMPLATE = <templatename>

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD EXTRA GNUMAKE RULES BELOW HERE

