#Makefile at top of application tree
TOP = .
include $(TOP)/configure/CONFIG

DIRS := configure coreApp  demoApp  sigApp  testApp  udpApp  xbpmApp iocBoot

define DIR_template
 $(1)_DEPEND_DIRS = configure
endef
$(foreach dir, $(filter-out configure,$(DIRS)),$(eval $(call DIR_template,$(dir))))

iocBoot_DEPEND_DIRS += $(filter %App,$(DIRS))
testApp_DEPEND_DIRS += coreApp
sigApp_DEPEND_DIRS += coreApp
udpApp_DEPEND_DIRS += coreApp
demoApp_DEPEND_DIRS += coreApp udpApp

include $(TOP)/configure/RULES_TOP


