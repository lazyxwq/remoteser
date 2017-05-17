################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../files.cpp \
../httpd.cpp \
../inc.cpp \
../main.cpp 

C_SRCS += \
../LOG.c \
../LOGCONF.c \
../LOGS.c \
../LOGSCONF.c \
../cJSON.c 

OBJS += \
./LOG.o \
./LOGCONF.o \
./LOGS.o \
./LOGSCONF.o \
./cJSON.o \
./files.o \
./httpd.o \
./inc.o \
./main.o 

CPP_DEPS += \
./files.d \
./httpd.d \
./inc.d \
./main.d 

C_DEPS += \
./LOG.d \
./LOGCONF.d \
./LOGS.d \
./LOGSCONF.d \
./cJSON.d 


# Each subdirectory must supply rules for building sources it contributes
%.o: ../%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '

%.o: ../%.cpp
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C++ Compiler'
	g++ -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


