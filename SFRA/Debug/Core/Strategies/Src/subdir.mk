################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Core/Strategies/Src/compensator_strategy.c \
../Core/Strategies/Src/plant_strategy.c 

OBJS += \
./Core/Strategies/Src/compensator_strategy.o \
./Core/Strategies/Src/plant_strategy.o 

C_DEPS += \
./Core/Strategies/Src/compensator_strategy.d \
./Core/Strategies/Src/plant_strategy.d 


# Each subdirectory must supply rules for building sources it contributes
Core/Strategies/Src/%.o Core/Strategies/Src/%.su Core/Strategies/Src/%.cyclo: ../Core/Strategies/Src/%.c Core/Strategies/Src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DDEBUG -DUSE_NUCLEO_64 -DUSE_HAL_DRIVER -DSTM32G474xx -c -I../Core/Inc -I"C:/Users/denni/Desktop/compensator_freqdomain/Software-Frequency-Response-Analyzer/SFRA/Core/Alg/Inc" -I"C:/Users/denni/Desktop/compensator_freqdomain/Software-Frequency-Response-Analyzer/SFRA/Core/Engine/Inc" -I"C:/Users/denni/Desktop/compensator_freqdomain/Software-Frequency-Response-Analyzer/SFRA/Core/Strategies/Inc" -I../Drivers/STM32G4xx_HAL_Driver/Inc -I../Drivers/STM32G4xx_HAL_Driver/Inc/Legacy -I../Drivers/BSP/STM32G4xx_Nucleo -I../Drivers/CMSIS/Device/ST/STM32G4xx/Include -I../Drivers/CMSIS/Include -O0 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Core-2f-Strategies-2f-Src

clean-Core-2f-Strategies-2f-Src:
	-$(RM) ./Core/Strategies/Src/compensator_strategy.cyclo ./Core/Strategies/Src/compensator_strategy.d ./Core/Strategies/Src/compensator_strategy.o ./Core/Strategies/Src/compensator_strategy.su ./Core/Strategies/Src/plant_strategy.cyclo ./Core/Strategies/Src/plant_strategy.d ./Core/Strategies/Src/plant_strategy.o ./Core/Strategies/Src/plant_strategy.su

.PHONY: clean-Core-2f-Strategies-2f-Src

