/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "hrtim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "compensator.h"
#include "math.h"
#include "sfra.h"
#include "stdio.h"
#include "stdint.h"
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define KI_LOOP						(float)(1.0f / (G2 * GADC))
#define GADC						(float)(1240.909091f)		//(1240.909091f)					//(1105)				//1240.909091f	//ADC Gain = 4095/3.3
#define G2							(float)(0.066f) // (0.250f)				//Hall Sensor Gain = 250mV/A
#define G3							(float)(0.00298923138f)//(0.002609997838f)//(0.00309512f)		//VIN Gain
#define G3_RMS						(float)(G3 * 100.0f)

#define SWITCHING_FREQ				(100000U)
#define SYSTEM_CLOCK				(170000000U)
#define HRTIM_CLOCK					(5440000000U)
#define FREQ_HZ_TO_PWM_TICKS(f)		(uint16_t)((uint64_t)HRTIM_CLOCK / (uint64_t)f)
#define HRTIM_PERIOD				FREQ_HZ_TO_PWM_TICKS(SWITCHING_FREQ)
#define PI_F       (3.14159265359f)




/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

/* USER CODE BEGIN PV */
compensator_2p2z_t 		comp2p2z_iloop;
compensator_2p2z_t		comp2p2z_vloop;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_HRTIM1_Init();
  MX_DAC1_Init();
  MX_DAC2_Init();
  MX_ADC2_Init();
  /* USER CODE BEGIN 2 */
  compensator_2P2Z_Init(&comp2p2z_iloop, 0.0f, A1_I, A2_I, B0_I, B1_I, B2_I, 1);
  compensator_2P2Z_Init(&comp2p2z_vloop, 0.0f, A1_V, A2_V, B0_V, B1_V, B2_V, 1);

  SFRA_Init();
  LUT_Init();

  HAL_DAC_Start(&hdac1, DAC_CHANNEL_1);
  HAL_DAC_Start(&hdac2, DAC_CHANNEL_1);


#if TOGGLE_SWEEP_ILOOP_FS_100KHZ
  HAL_HRTIM_WaveformCountStart_IT(&hhrtim1, HRTIM_TIMERID_TIMER_A);
  HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1|HRTIM_OUTPUT_TA2);	// For Debugging Purposes
#endif

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
  HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED);

  HAL_ADCEx_InjectedStart_IT(&hadc2);

  HAL_HRTIM_WaveformCountStart_IT(&hhrtim1, HRTIM_TIMERID_TIMER_A);
  HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TA1);	// For Boost PWM

#endif

#if TOGGLE_SWEEP_VLOOP_FS_6KHZ
  HAL_HRTIM_WaveformCountStart_IT(&hhrtim1, HRTIM_TIMERID_TIMER_D);
  HAL_HRTIM_WaveformOutputStart(&hhrtim1, HRTIM_OUTPUT_TD1|HRTIM_OUTPUT_TD2);	// For Debugging Purposes
#endif

  /* USER CODE END 2 */

  /* Initialize led */
  BSP_LED_Init(LED_GREEN);

  /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
  BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	if(g_sfra.b_start_flag){
		g_sfra.b_start_flag = false;
		printf("\r\nStarting Frequency Sweep: %s\r\n", STRING_MESSAGE_SWEEP_NAME);
		printf("Operating Condition: %s\r\n", STRING_OPERATION);
#if TOGGLE_SWEEP_ILOOP_FS_100KHZ
		printf("ILOOP Parameters: %s, %s, %s\r\n", STRING_ILOOP_FX, STRING_ILOOP_PM, STRING_ILOOP_GM);
		printf("ILOOP Coefficients: B0:%f B1:%f B2:%f A1:%f A2:%f\r\n\r\n", B0_I, B1_I, B2_I, A1_I, A2_I);
#elif TOGGLE_SWEEP_VLOOP_FS_6KHZ
		printf("VLOOP Parameters: %s, %s, %s\r\n", STRING_VLOOP_FX, STRING_VLOOP_PM, STRING_VLOOP_GM);
		printf("VLOOP Coefficients: B0:%f B1:%f B2:%f A1:%f A2:%f\r\n\r\n", B0_V, B1_V, B2_V, A1_V, A2_V);
#endif

		printf("frequency,magnitude_db,phase_deg\r\n");
	}




	if(g_sfra.b_result_ready_flag)
	{
#if TOGGLE_SWEEP_ILOOP_FS_100KHZ
		g_sfra.b_result_ready_flag = false;
		printf("%f,%f,%f\r\n", g_sfra.freq_table[g_sfra.freq_index-1], g_sfra.gain_db[g_sfra.freq_index-1], g_sfra.phase_deg[g_sfra.freq_index-1]);
#elif TOGGLE_SWEEP_VLOOP_FS_6KHZ
		g_sfra.b_result_ready_flag = false;
		if (g_sfra.current_freq ==(float)(FREQ_START_HZ))
		{

		}
		else
		{
			printf("%f,%f,%f\r\n", g_sfra.freq_table[g_sfra.freq_index-1], g_sfra.gain_db[g_sfra.freq_index-1], g_sfra.phase_deg[g_sfra.freq_index-1]);
		}
#endif
	}



	if(g_sfra.b_end_flag){
		g_sfra.b_end_flag = false;
		printf("%f,%f,%f\r\n", g_sfra.freq_table[g_sfra.freq_index-1], g_sfra.gain_db[g_sfra.freq_index-1], g_sfra.phase_deg[g_sfra.freq_index-1]);
	    printf("\r\nElapsed Time: %.2f sec\r\n", g_sfra.elapsed_time_ms / 1000.0f);
		printf("\r\nEnd of Frequency Sweep\r\n");

	}

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

#if 1
void HAL_ADCEx_InjectedConvCpltCallback(ADC_HandleTypeDef *hadc)
{
	GPIOC->BSRR = GPIO_PIN_7;
	g_sfra.u32_isense_ave_adc = HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
	GPIOC->BRR = GPIO_PIN_7;

}
#endif

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ


#endif
#if 0
void HAL_HRTIM_CounterResetCallback(HRTIM_HandleTypeDef * hhrtim, uint32_t TimerIdx)
{

#if TOGGLE_SWEEP_IPLANT_FS_100KHZ
	// Current Loop for Plant Sweep
	if(HRTIM_TIMERINDEX_TIMER_A == TimerIdx)
	{

		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);
		HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);

		SFRA_Run();

		// Create Sine Wave
		// LUT index (top 13 bits)
		g_sfra.index = g_sfra.phase_acc >> DDS_LUT_SHIFT;
		g_sfra.phase_acc += g_sfra.phase_inc;

		// Create the Injected Signal Sine Wave
		g_sfra.sine_out = g_sfra.amplitude * g_sfra.sine_lut[g_sfra.index];						// Generated sine, injected to 2p2z
		g_sfra.cosine_out = g_sfra.amplitude * g_sfra.sine_lut[(g_sfra.index + 2048) & 0x1FFF];	// Generated for testing only, not to be processed

		// Create a reference signal sine and cosine
		g_sfra.sine_ref = g_sfra.sine_lut[g_sfra.index];
		g_sfra.cosine_ref = g_sfra.sine_lut[(g_sfra.index + 2048) & 0x1FFF];

#endif
	}
#endif

#if TOGGLE_SWEEP_ILOOP_FS_100KHZ
	// Current Loop 100kHz
	if(HRTIM_TIMERINDEX_TIMER_A == TimerIdx)
	{
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);

		SFRA_Run();

		// Create Sine Wave
		// LUT index (top 13 bits)
		g_sfra.index = g_sfra.phase_acc >> DDS_LUT_SHIFT;
		g_sfra.phase_acc += g_sfra.phase_inc;

		// Create the Injected Signal Sine Wave
		g_sfra.sine_out = g_sfra.amplitude * g_sfra.sine_lut[g_sfra.index];						// Generated sine, injected to 2p2z
		g_sfra.cosine_out = g_sfra.amplitude * g_sfra.sine_lut[(g_sfra.index + 2048) & 0x1FFF];	// Generated for testing only, not to be processed

		// Create a reference signal sine and cosine
		g_sfra.sine_ref = g_sfra.sine_lut[g_sfra.index];
		g_sfra.cosine_ref = g_sfra.sine_lut[(g_sfra.index + 2048) & 0x1FFF];

		// Run Compensator 2p2z
		comp2p2z_iloop.f_ref = g_sfra.sine_out;
		comp2p2z_iloop.f_fdbk = 0.0f;
		compensator_2P2Z_Update(&comp2p2z_iloop);

		// Accumulator During FSM Measuring
		if(g_sfra.state == SFRA_STATE_MEASURING)
		{
		    g_sfra.input_I_acc += g_sfra.sine_out * g_sfra.sine_ref;

		    g_sfra.input_Q_acc +=g_sfra.sine_out * g_sfra.cosine_ref;

		    g_sfra.output_I_acc += comp2p2z_iloop.f_out * g_sfra.sine_ref;

		    g_sfra.output_Q_acc += comp2p2z_iloop.f_out *g_sfra.cosine_ref;
		}

		// Output DAC Signals
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, (uint16_t)(comp2p2z_iloop.f_ref+2048.0f));
		HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R,(uint16_t)(comp2p2z_iloop.f_out+2048.0f));
		//HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R,(uint16_t)(g_sfra.cosine_out+2048.0f));
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

	}
#endif
#if TOGGLE_SWEEP_VLOOP_FS_6KHZ
	// Voltage Loop 6kHz
	if(HRTIM_TIMERINDEX_TIMER_D == TimerIdx)
	{
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
		SFRA_Run();

		// Create Sine Wave
		// LUT index (top 13 bits)
		g_sfra.index = g_sfra.phase_acc >> DDS_LUT_SHIFT;
		g_sfra.phase_acc += g_sfra.phase_inc;

		// Create the Injected Signal Sine Wave
		g_sfra.sine_out = g_sfra.amplitude * g_sfra.sine_lut[g_sfra.index];						// Generated sine, injected to 2p2z
		g_sfra.cosine_out = g_sfra.amplitude * g_sfra.sine_lut[(g_sfra.index + 2048) & 0x1FFF];	// Generated for testing only, not to be processed

		// Create a reference signal sine and cosine
		g_sfra.sine_ref = g_sfra.sine_lut[g_sfra.index];
		g_sfra.cosine_ref = g_sfra.sine_lut[(g_sfra.index + 2048) & 0x1FFF];

		// Run Compensator 2p2z
		comp2p2z_vloop.f_ref = g_sfra.sine_out;
		comp2p2z_vloop.f_fdbk = 0.0f;
		compensator_2P2Z_Update(&comp2p2z_vloop);

		// Accumulator During FSM Measuring
		if(g_sfra.state == SFRA_STATE_MEASURING)
		{
		    g_sfra.input_I_acc += g_sfra.sine_out * g_sfra.sine_ref;

		    g_sfra.input_Q_acc +=g_sfra.sine_out * g_sfra.cosine_ref;

		    g_sfra.output_I_acc += comp2p2z_vloop.f_out * g_sfra.sine_ref;

		    g_sfra.output_Q_acc += comp2p2z_vloop.f_out *g_sfra.cosine_ref;
		}

		// Output DAC Signals
		HAL_DAC_SetValue(&hdac1, DAC_CHANNEL_1, DAC_ALIGN_12B_R, (uint16_t)(comp2p2z_vloop.f_ref+2048.0f));
		HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R,(uint16_t)(comp2p2z_vloop.f_out+2048.0f));
		//HAL_DAC_SetValue(&hdac2, DAC_CHANNEL_1, DAC_ALIGN_12B_R,(uint16_t)(g_sfra.cosine_out+2048.0f));
		HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

	}

}
#endif


/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
