#makefile文件编译指令
#如果make文件名是makefile，直接使用make就可以编译
#如果make文件名不是makefile，比如test.txt，那么使用make -f test.txt

#------------------------------------------检查操作系统32位64位--------------------------------------------------------
#SYS_BIT=$(shell getconf LONG_BIT)
#SYS_BIT=$(shell getconf WORD_BIT)
SYS_BIT=$(shell getconf LONG_BIT)
ifeq ($(SYS_BIT),32)
	CPU =  -march=i686 
else 
	CPU = 
endif

#------------------------------------------编辑器--------------------------------------------------------

#c++编译工具
CC = g++ 

#------------------------------------------编辑器End--------------------------------------------------------

#------------------------------------------目录--------------------------------------------------------

#依赖目标/文件查找目录
VPATH = $(OBJ_OUTPUT_DIR) 

#输出目录
OBJ_OUTPUT_DIR=./output
OBJ_OUTPUT=./output
$(shell mkdir $(OBJ_OUTPUT_DIR))
$(shell mkdir $(OUTPUT_DIR))

#.cpp目录
CPP_DIR=

#mdk安装目录
MDK_HOME=..

#.h目录
H_DIR=$(MDK_HOME)/include

#------------------------------------------目录End--------------------------------------------------------

#------------------------------------------编译选项--------------------------------------------------------

#SO文件编译选项
CFLAGS= -O -g -fPIC -Wall -D_REENTRANT -DUSE_APACHE -DNO_STRING_CIPHER $(CPU) 

#警告级别
WARNING_LEVEL += -O3 

#头文件目录：-I 目录
INCLUDE = -I. -I../include -I$(H_DIR) 

#库目录，库文件:-L 目录 -库名
#SYSLIB = -lnsl -lc -lm -lpthread -lstdc++ 
LIB = -lnsl -lc -lm -lpthread -lrt -lstdc++ 

#静态库：.a文件路径名
LIB += $(MDK_HOME)/lib/mdk.a 

#------------------------------------------编译选项End--------------------------------------------------------

#项目入口主文件
MAIN =



#------------------------------------------输出--------------------------------------------------------
#编译产生的程序文件名
OUTPUT = test

#目标文件
OBJ_PRO = $(notdir $(patsubst %.cpp,%.o,$(wildcard *.cpp))) 

#依赖的目标文件
DEPENDENCE = $(OBJ_PRO) 

#连接项目，需要的所有其它源文件或目标文件(带目录)
OBJ = $(addprefix $(OBJ_OUTPUT_DIR)/, $(OBJ_PRO)) 

#------------------------------------------输出End--------------------------------------------------------

#-------------------------------------------编译指令-----------------------------------------------------
#生成EXE
#范例：
#$(OBJ_OUTPUT)/$(OUTPUT):$(MAIN)$(DEPENDENCE)
#	@echo "Complie $(OBJ_OUTPUT)/$(OUTPUT)"
#	@echo ""
#	$(CC) -o $@ $(CFLAGS)$(WARNING_LEVEL)$(INCLUDE)$(MAIN)$(OBJ)$(LIB)
#	@echo ""
#	@echo "$(OBJ_OUTPUT)/$(OUTPUT) complie finished"
#	@echo ""
#	@echo ""
#	@echo ""
#	@echo ""

$(OBJ_OUTPUT)/$(OUTPUT):$(MAIN)$(DEPENDENCE)
	@echo "Complie $(OBJ_OUTPUT)/$(OUTPUT)"
	@echo ""
	$(CC) -o $@ $(CFLAGS)$(WARNING_LEVEL)$(INCLUDE)$(MAIN)$(OBJ)$(LIB)
	@echo ""
	@echo "$(OBJ_OUTPUT)/$(OUTPUT) complie finished"
	@echo ""
	@echo ""
	@echo ""
	@echo ""

#-----------------------------------------编译生成.A静态库---------------------------------------------------
#$(OBJ_OUTPUT)/$(OUTPUT):$(MAIN)$(DEPENDENCE)
#	@echo "Complie $(OBJ_OUTPUT)/$(OUTPUT)"
#	@echo ""
#	ar -r $@ $(OBJ)
#	@echo ""
#	@echo "$(OBJ_OUTPUT)/$(OUTPUT) complie finished"
#	@echo ""
#	@echo ""
#	@echo ""
#	@echo ""

#-----------------------------------------编译生成.SO动态库---------------------------------------------------
#范例：
#$(OBJ_OUTPUT)/$(OUTPUT): $(DEPENDENCE)											依赖关系
#	@echo "Complie $(OBJ_OUTPUT)/$(OUTPUT)"
#	$(CC) -o $@ -shared $(CFLAGS)$(WARNING_LEVEL)$(INCLUDE)$(OBJ)$(LIB)		gcc编译指令
#	@echo ""
#	@echo "$(OBJ_OUTPUT)/$(OUTPUT) complie finished"
#	@echo ""
#	@echo ""
#	@echo ""
#	@echo ""


#------------------------------------------编译Object----------------------------------------------------
#范例：单个object编译
#$(OBJ_OUTPUT_DIR)/GameSerFrm.o: main/GameSerFrm.cpp main/GameSerFrm.h main/GameSerCPU.h com/ComDef.h com/XXSocket.h tool/DBTool.h	依赖关系
#	@echo "Complie GameSerFrm.o"
#	$(CC) -c -o $*.o $(CFLAGS)$(WARNING_LEVEL)$(INCLUDE)main/GameSerFrm.cpp											gcc编译指令
#	@echo ""
#	@echo "$(OBJ_OUTPUT_DIR)/GameSerFrm.o complie finished"
#	@echo ""
#	@echo ""

#范例：批量object编译
#$(OBJ):%.o:%.cpp %.h
#	@echo "Complie $(OBJ_OUTPUT_DIR)/$*.o"
#	@echo ""
#	$(CC) -c -o $(OBJ_OUTPUT_DIR)/$*.o $(CFLAGS)$(WARNING_LEVEL)$(INCLUDE) $(CPP_DIR)/$*.cpp
#	@echo ""
#	@echo "$(OBJ_OUTPUT_DIR)/$*.o complie finished"
#	@echo ""
#	@echo ""
#	@echo ""
#	@echo ""


$(OBJ_MDK):%.o:%.cpp %.h
	@echo "Complie $(OBJ_OUTPUT_DIR)/$*.o"
	@echo ""
	$(CC) -c -o $(OBJ_OUTPUT_DIR)/$*.o $(CFLAGS)$(WARNING_LEVEL)$(INCLUDE) $(CPP_DIR)/mdk/$*.cpp
	@echo ""
	@echo "$(OBJ_OUTPUT_DIR)/$*.o complie finished"
	@echo ""
	@echo ""
	@echo ""
	@echo ""

$(OBJ_FRAME_NETSERVER):%.o:%.cpp %.h
	@echo "Complie $(OBJ_OUTPUT_DIR)/$*.o"
	@echo ""
	$(CC) -c -o $(OBJ_OUTPUT_DIR)/$*.o $(CFLAGS)$(WARNING_LEVEL)$(INCLUDE) $(CPP_DIR)/frame/netserver/$*.cpp
	@echo ""
	@echo "$(OBJ_OUTPUT_DIR)/$*.o complie finished"
	@echo ""
	@echo ""
	@echo ""
	@echo ""

$(OBJ_PRO):%.o:%.cpp %.h
	@echo "Complie $(OBJ_OUTPUT_DIR)/$*.o"
	@echo ""
	$(CC) -c -o $(OBJ_OUTPUT_DIR)/$*.o $(CFLAGS)$(WARNING_LEVEL)$(INCLUDE) $*.cpp
	@echo ""
	@echo "$(OBJ_OUTPUT_DIR)/$*.o complie finished"
	@echo ""
	@echo ""
	@echo ""
	@echo ""



#------------------------------------------清理重新编译----------------------------------------------------
clean:
	-rm -f $(OBJ_OUTPUT)/$(OUTPUT) $(OBJ_OUTPUT_DIR)/*.o
	
.PHONY: clean

