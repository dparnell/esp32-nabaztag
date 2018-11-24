#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

COMPONENT_EXTRA_CLEAN := $(PROJECT_PATH)/build/weaken.out

vnet.o: $(PROJECT_PATH)/build/weaken.out

$(PROJECT_PATH)/build/weaken.out: $(PROJECT_PATH)/build/esp32/event_default_handlers.o
	echo --------------------------------------------------------------------------
	echo Weakening $(PROJECT_PATH)/build/esp32/event_default_handlers.o
	echo --------------------------------------------------------------------------
	xtensa-esp32-elf-objcopy --verbose --weaken $(PROJECT_PATH)/build/esp32/event_default_handlers.o > $(PROJECT_PATH)/build/weaken.out
