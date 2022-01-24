#AR 库文件维护程序的名称。默认值为ar
#AS 汇编程序的名称，默认值为as
#CPP C预编译器的名称，默认值为$(CC) -E
#CXX C++编译器的名称，默认值为g++
#FC   FORTRAN编译器的名称，默认值为f77
#ARFLAGS   库文件维护的程序选项，无默认值
#ASFLAGS   汇编程序的选项，无默认值
#CFLAGS     C编译器的选项，无默认值
#CPPFLAGS C预编译的选项，无默认值
#CXXFLAGS C++编译器的选项，无默认值
#FFLAGS      FORTRAN编译器的选项，无默认值

CC ?= gcc
CXX ?= g++

INC_CLUDE = -I$(CURDIR)/../local/include -I$(CURDIR)/inc
LIBS    := -lSDL2 -lSDL2_image
LIBS    += -lavdevice -lavformat -lavfilter -lavcodec -lswresample -lswscale -lavutil -pthread -lm
LDFLAGS := -L$(CURDIR)/../local/lib

//HI_CFLAGS = -mfloat-abi=softfp -mfpu=vfpv3-d16
HI_CFLAGS = 
SOURCES += $(wildcard $(CURDIR)/src/*.cpp)
objects =  $(patsubst %.cpp, %.o, $(SOURCES))

use_sdl : $(objects)
	$(CXX) $(HI_CFLAGS) -o  $@ -g $(objects) $(LIBS) $(LDFLAGS) $(INC_CLUDE)

$(objects) : %.o:%.cpp
	$(CXX) $(HI_CFLAGS) -c -g  $< -o $@ $(INC_CLUDE) $(LIBS) $(LDFLAGS)


clean :
	rm *.o use_sdl $(CURDIR)/src/*.o