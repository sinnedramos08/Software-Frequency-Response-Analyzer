################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Engine/Src/dds.c \
../Core/Engine/Src/iq.c \
../Core/Engine/Src/sfra_engine.c 

OBJS += \
./Core/Engine/Src/dds.o \
./Core/Engine/Src/iq.o \
./Core/Engine/Src/sfra_engine.o 

C_DEPS += \
./Core/Engine/Src/dds.d \
./Core/Engine/Src/iq.d \
./Core/Engine/Src/sfra_engine.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Engine/Src/%.o Core/Engine/Src/%.su Core/Engine/Src/%.cyclo: ../Core/Engine/Src/%.c Core/Engine/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -I"C:/Users/denni/Desktop/compensator_freqdomain/Software-Frequency-Response-Analyzer/SFRA/Core/Alg/Inc" -I"C:/Users/denni/Desktop/compensator_freqdomain/Software-Frequency-Response-Analyzer/SFRA/Core/Engine/Inc" -I"C:/Users/denni/Desktop/compensator_freqdomain/Software-Frequency-Response-Analyzer/SFRA/Core/Strategies/Inc" -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/BSP/STM32G4xx_Nucleo -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Engine-2f-Src

clean-Core-2f-Engine-2f-Src:
	-$(RM) ./Core/Engine/Src/dds.cyclo ./Core/Engine/Src/dds.d ./Core/Engine/Src/dds.o ./Core/Engine/Src/dds.su ./Core/Engine/Src/iq.cyclo ./Core/Engine/Src/iq.d ./Core/Engine/Src/iq.o ./Core/Engine/Src/iq.su ./Core/Engine/Src/sfra_engine.cyclo ./Core/Engine/Src/sfra_engine.d ./Core/Engine/Src/sfra_engine.o ./Core/Engine/Src/sfra_engine.su

.PHONY: clean-Core-2f-Engine-2f-Src

