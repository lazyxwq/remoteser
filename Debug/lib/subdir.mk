################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../lib/files.cpp \
../lib/httpd.cpp \
../lib/inc.cpp 

C_SRCS += \
../lib/LOG.c \
../lib/LOGCONF.c \
../lib/LOGS.c \
../lib/LOGSCONF.c \
../lib/cJSON.c 

OBJS += \
./lib/LOG.o \
./lib/LOGCONF.o \
./lib/LOGS.o \
./lib/LOGSCONF.o \
./lib/cJSON.o \
./lib/files.o \
./lib/httpd.o \
./lib/inc.o 

CPP_DEPS += \
./lib/files.d \
./lib/httpd.d \
./lib/inc.d 

C_DEPS += \
./lib/LOG.d \
./lib/LOGCONF.d \
./lib/LOGS.d \
./lib/LOGSCONF.d \
./lib/cJSON.d 


# Each subdirectory must supply rules for building sources it contributes
lib/%.o: ../lib/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -std=c11 -I"/mnt/work/eclipse_work/logs" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

lib/%.o: ../lib/%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -std=c++1y -I"/mnt/work/eclipse_work/logs" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


