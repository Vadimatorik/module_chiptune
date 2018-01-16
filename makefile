ifndef MODULE_MOD_CHIP_OPTIMIZATION
	MODULE_MOD_CHIP_OPTIMIZATION = -g3 -O0
endif
#**********************************************************************
# module_chiptune
#**********************************************************************
MOD_CHIP_H_FILE		:= $(shell find module_chiptune/ -maxdepth 3 -type f -name "*.h" )
MOD_CHIP_CPP_FILE	:= $(shell find module_chiptune/ -maxdepth 3 -type f -name "*.cpp" )
MOD_CHIP_DIR		:= $(shell find module_chiptune/ -maxdepth 3 -type d -name "*" )
MOD_CHIP_PATH		:= $(addprefix -I, $(MOD_CHIP_DIR))
MOD_CHIP_OBJ_FILE	:= $(addprefix build/obj/, $(MOD_CHIP_CPP_FILE))
MOD_CHIP_OBJ_FILE	:= $(patsubst %.cpp, %.o, $(MOD_CHIP_OBJ_FILE))

build/obj/module_chiptune/%.o:	module_chiptune/%.cpp
	@echo [CPP] $<
	@mkdir -p $(dir $@)
	@$(CPP) $(CPP_FLAGS) $(MK_INTER_PATH) $(FAT_FS_PATH) $(MOD_CHIP_PATH) $(USER_CFG_PATH) $(STM32_F2_API_PATH) $(FREE_RTOS_PATH) $(SH_PATH) $(MODULE_MOD_CHIP_OPTIMIZATION) -c $< -o $@
	
# Добавляем к общим переменным проекта.
PROJECT_PATH			+= $(MOD_CHIP_PATH)
PROJECT_OBJ_FILE		+= $(MOD_CHIP_OBJ_FILE)