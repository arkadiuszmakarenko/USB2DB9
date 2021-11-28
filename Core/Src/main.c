/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2020 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_host.h"
#include "usbh_hid.h"
#include "usbh_hid_gamepad.h"
#include "usbh_hid_mouse.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;



#define MOUSEX	0
#define MOUSEY	1
#define Q_RATELIMIT 500
#define Q_BUFFERLIMIT 300
#define DPI_DIVIDER 2

volatile int8_t mouseDirectionX = 0;		// X direction (0 = decrement, 1 = increment)
volatile int8_t mouseEncoderPhaseX = 0;		// X Quadrature phase (0-3)
volatile int8_t mouseDirectionY = 0;		// Y direction (0 = decrement, 1 = increment)
volatile int8_t mouseEncoderPhaseY = 0;		// Y Quadrature phase (0-3)
volatile int16_t mouseDistanceX = 0;		// Distance left for mouse to move
volatile int16_t mouseDistanceY = 0;		// Distance left for mouse to move
volatile uint8_t xTimerTop = 1;				// X axis timer TOP value
volatile uint8_t yTimerTop = 1;				// Y axis timer TOP value

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
void MX_USB_HOST_Process(void);



/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint8_t processMouseMovement(int8_t movementUnits, uint8_t axis, int limitRate, int dpiDivide)
{
	uint16_t timerTopValue = 0;



	// Set the mouse movement direction and record the movement units
	if (movementUnits > 0) {
		// Moving in the positive direction

		// Apply DPI limiting if required
		if (dpiDivide) {
			movementUnits /= DPI_DIVIDER;
			if (movementUnits < 1) movementUnits = 1;
		}

		// Add the movement units to the quadrature output buffer
		if (axis == MOUSEX) mouseDistanceX += movementUnits;
		else mouseDistanceY += movementUnits;
	} else if (movementUnits < 0) {
		// Moving in the negative direction

		// Apply DPI limiting if required
		if (dpiDivide) {
			movementUnits /= DPI_DIVIDER;
			if (movementUnits > -1) movementUnits = -1;
		}

		// Add the movement units to the quadrature output buffer
		if (axis == MOUSEX) mouseDistanceX += -movementUnits;
		else mouseDistanceY += -movementUnits;
	} else {
		if (axis == MOUSEX) mouseDistanceX = 0;
		else mouseDistanceY = 0;
	}

	// Apply the quadrature output buffer limit
	if (axis == MOUSEX) {
		if (mouseDistanceX > Q_BUFFERLIMIT) mouseDistanceX = Q_BUFFERLIMIT;
	} else {
		if (mouseDistanceY > Q_BUFFERLIMIT) mouseDistanceY = Q_BUFFERLIMIT;
	}

	// Get the current value of the quadrature output buffer
	if (axis == MOUSEX) timerTopValue = mouseDistanceX;
	else timerTopValue = mouseDistanceY;

	// Range check the quadrature output buffer
	if (timerTopValue > 127) timerTopValue = 127;

	// Since the USB reports arrive at 100-125 Hz (even if there is only
	// a small amount of movement, we have to output the quadrature
	// at minimum rate to keep up with the reports (otherwise it creates
	// a slow lag).  If we assume 100 Hz of reports then the following
	// is true:
	//
	// 127 movements = 12,700 interrupts/sec
	// 100 movements = 10,000 interrupts/sec
	//  50 movements =  5,000 interrupts/sec
	//  10 movements =  1,000 interrupts/sec
	//   1 movement  =    100 interrupts/sec
	//
	// Timer speed is 15,625 ticks per second = 64 uS per tick
	//
	// Required timer TOP values (0 is fastest so all results are x-1):
	// 1,000,000 / 12,700 = 78.74 / 64 uS = 1.2 - 1
	// 1,000,000 / 10,000 = 100 / 64 uS = 1.56 - 1
	// 1,000,000 / 5,000 = 200 / 64 uS = 3.125 - 1
	// 1,000,000 / 1,000 = 1000 uS / 64 uS = 15.63 - 1
	// 1,000,000 / 100 = 10000 uS / 64 uS = 156.25 - 1
	//
	// So:
	//   timerTopValue = 10000 / timerTopValue; // i.e. 1,000,000 / (timerTopValue * 100)
	//   timerTopValue = timerTopValue / 64;
	//   timerTopValue = timerTopValue - 1;
    if (timerTopValue !=0)
    {
	timerTopValue = ((10000 / timerTopValue) / 64) - 1;
    }
    else
	{
    	timerTopValue = 255;
	}
	// If the 'Slow' configuration jumper is shorted; apply the quadrature rate limit
	if (limitRate) {
		// Rate limit is on

		// Rate limit is provided in hertz
		// Each timer tick is 64 uS
		//
		// Convert hertz into period in uS
		// 1500 Hz = 1,000,000 / 1500 = 666.67 uS
		//
		// Convert period into timer ticks (* 4 due to quadrature)
		// 666.67 us / (64 * 4) = 2.6 ticks
		//
		// Timer TOP is 0-255, so subtract 1
		// 10.42 ticks - 1 = 9.42 ticks

		uint32_t rateLimit = ((1000000 / Q_RATELIMIT) / 256) - 1;

		// If the timerTopValue is less than the rate limit, we output
		// at the maximum allowed rate.  This will cause addition lag that
		// is handled by the quadrature output buffer limit above.
		if (timerTopValue < (uint16_t)rateLimit) timerTopValue = (uint16_t)rateLimit;
	}

	// Return the timer TOP value
	return (uint8_t)timerTopValue;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
	USBH_HandleTypeDef * usbhost = NULL;
	ApplicationTypeDef aState = APPLICATION_DISCONNECT;
	uint8_t *joymap = 0U;
	HID_MOUSE_Info_TypeDef    *mousemap;

    uint8_t limitRate = 0U;
	uint8_t dpiDivide = 0U;



		// Get the state of the rate limiting (slow) header
	//	if ((RATESW_PIN & RATESW) == 0) limitRate = 1;

		// Get the state of the DPI divider header
		//if ((DPISW_PIN & DPISW) == 0) dpiDivide = 1;


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
  MX_USB_HOST_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
//HAL_TIM_Base_Start_IT(&htim2) ;
//HAL_TIM_Base_Start_IT(&htim3) ;
/* USER CODE END 2 */
 
	HAL_TIM_Base_Start_IT(&htim2) ;
  HAL_TIM_Base_Start_IT(&htim3) ;

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
while (1)
  {
    /* USER CODE END WHILE */
    MX_USB_HOST_Process();

    /* USER CODE BEGIN 3 */

    aState = USBH_ApplicationState();
    		if (aState == APPLICATION_READY)
    		{
    			usbhost = USBH_GetHost();
    			//HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET);

    			HID_TypeTypeDef device_type = USBH_HID_GetDeviceType(usbhost);
    			if (device_type == HID_MOUSE)
    			{
    				mousemap = USBH_HID_GetMouseInfo(usbhost);

    				if (mousemap!=NULL)
    				{
    					// +X = Mouse going right
    							// -X = Mouse going left
    							// +Y = Mouse going down
    							// -Y = Mouse going up
    							//
    							// X and Y have a range of -127 to +127

    							// If the mouse movement changes direction then disregard any remaining
    							// movement units in the previous direction.
    							if (mousemap->x > 0 && mouseDirectionX == 0) {
    								mouseDistanceX = 0;
    								mouseDirectionX = 1;
    							} else if (mousemap->x < 0 && mouseDirectionX == 1) {
    								mouseDistanceX = 0;
    								mouseDirectionX = 0;
    							} else if (mousemap->y > 0 && mouseDirectionY == 0) {
    								mouseDistanceY = 0;
    								mouseDirectionY = 1;
    							} else if (mousemap->y < 0 && mouseDirectionY == 1) {
    								mouseDistanceY = 0;
    								mouseDirectionY = 0;
    							}

    							// Process mouse X and Y movement -------------------------------------
    							//HAL_TIM_Base_Start_IT(&htim2) ;
    							//HAL_TIM_Base_Start_IT(&htim3) ;

    							int16_t x_val = mousemap->x;
    							int16_t y_val = mousemap->y;

    							if (x_val>0 && x_val<10){x_val=+ 10;}
    							if (x_val<0 && x_val<-10){x_val=- 10;}

    							if (y_val>0 && y_val<10){y_val=+ 10;}
    							if (y_val<0 && y_val<-10){y_val=- 10;}

    							xTimerTop = processMouseMovement(mousemap->x, MOUSEX, 0U,0U);
    							yTimerTop = processMouseMovement(mousemap->y, MOUSEY, 0U,0U);

    							// Process mouse buttons ----------------------------------------------


    		 					HAL_GPIO_WritePin(BTN1_GPIO_Port, BTN1_Pin, mousemap->buttons[2]);
    		    				HAL_GPIO_WritePin(BTN2_GPIO_Port, BTN2_Pin, mousemap->buttons[1]);
    		    				HAL_GPIO_WritePin(BTN3_GPIO_Port, BTN3_Pin, mousemap->buttons[0]);



    				}




    			}
    			else if (device_type == HID_UNKNOWN)
    			{
    				joymap = USBH_HID_GetGamepadInfo(usbhost);

    				if (joymap!=NULL)
    				{
    					//RIGHT = (*joymap&0x1);
    					//LEFT = (*joymap>>1&0x1);
    					//UP = (*joymap>>3&0x1);
    					//DOWN = (*joymap>>2&0x1);

    					//BTN1 =  (*joymap>>6&0x1);
    				    //BTN2 =  (*joymap>>5&0x1);
    					//BTN3 =  (*joymap>>4&0x1);
    					//HAL_GPIO_WritePin(LVQ_GPIO_Port, LVQ_Pin, ;

    					HAL_GPIO_WritePin(RHQ_GPIO_Port, RHQ_Pin, (*joymap&0x1));
    					HAL_GPIO_WritePin(LVQ_GPIO_Port, LVQ_Pin, (*joymap>>1&0x1));
    					HAL_GPIO_WritePin(BH_GPIO_Port, BH_Pin, (*joymap>>2&0x1));
    					HAL_GPIO_WritePin(FV_GPIO_Port, FV_Pin, (*joymap>>3&0x1));
    					HAL_GPIO_WritePin(BTN1_GPIO_Port, BTN1_Pin, (*joymap>>4&0x1));
    					HAL_GPIO_WritePin(BTN2_GPIO_Port, BTN2_Pin, (*joymap>>5&0x1));
    					HAL_GPIO_WritePin(BTN3_GPIO_Port, BTN3_Pin, (*joymap>>6&0x1));
    				}

    			};



    		}
    		else
    		{

    		//	HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
    		}



  }
  /* USER CODE END 3 */
}
  /* USER CODE END 3 */


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.Prediv1Source = RCC_PREDIV1_SOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL2.PLL2State = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB busses clocks 
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL_DIV3;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
  /** Configure the Systick interrupt time 
  */
  __HAL_RCC_PLLI2S_ENABLE();
}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 4608-1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1000;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 4608-1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 1000;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LED1_Pin|LED2_Pin|RHQ_Pin|LVQ_Pin 
                          |BH_Pin|FV_Pin|BTN3_Pin|BTN2_Pin 
                          |BTN1_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED1_Pin LED2_Pin RHQ_Pin LVQ_Pin 
                           BH_Pin FV_Pin BTN3_Pin BTN2_Pin 
                           BTN1_Pin */
  GPIO_InitStruct.Pin = LED1_Pin|LED2_Pin|RHQ_Pin|LVQ_Pin 
                          |BH_Pin|FV_Pin|BTN3_Pin|BTN2_Pin 
                          |BTN1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if (htim->Instance == TIM2)
	{

		// Process X output
		if (mouseDistanceX > 0) {
			// Set the output pins according to the current phase BH RHQ FV LVQ

			if (mouseEncoderPhaseX == 0) HAL_GPIO_WritePin(BH_GPIO_Port, BH_Pin, 1);	// Set X1 to 1
			if (mouseEncoderPhaseX == 1) HAL_GPIO_WritePin(RHQ_GPIO_Port, RHQ_Pin, 1);	// Set X2 to 1
			if (mouseEncoderPhaseX == 2) HAL_GPIO_WritePin(BH_GPIO_Port, BH_Pin, 0);	// Set X1 to 0
			if (mouseEncoderPhaseX == 3) HAL_GPIO_WritePin(RHQ_GPIO_Port, RHQ_Pin, 0);	// Set X2 to 0

			// Change phase
			if (mouseDirectionX == 0) mouseEncoderPhaseX--; else mouseEncoderPhaseX++;

			// Decrement the distance left to move
			mouseDistanceX--;

			// Range check the phase
			if ((mouseDirectionX == 1) && (mouseEncoderPhaseX > 3)) mouseEncoderPhaseX = 0;
			if ((mouseDirectionX == 0) && (mouseEncoderPhaseX < 0)) mouseEncoderPhaseX = 3;
		} else {
			// Reset the phase if the mouse isn't moving
			mouseEncoderPhaseX = 0;
		}

		// Set the timer top value for the next interrupt
		if (xTimerTop ==0)
		{
			htim->Instance->ARR = 1;
		}
		else
		{
			htim->Instance->ARR = xTimerTop;
		}

	}

	if (htim->Instance == TIM3)
	{

		// Process Y output
			if (mouseDistanceY > 0) {
				// Set the output pins according to the current phase
				if (mouseEncoderPhaseY == 3) HAL_GPIO_WritePin(FV_GPIO_Port, LVQ_Pin, 0);	// Set Y1 to 0
				if (mouseEncoderPhaseY == 2) HAL_GPIO_WritePin(LVQ_GPIO_Port, FV_Pin, 0);	// Set Y2 to 0
				if (mouseEncoderPhaseY == 1) HAL_GPIO_WritePin(FV_GPIO_Port, LVQ_Pin, 1);	// Set Y1 to 1
				if (mouseEncoderPhaseY == 0) HAL_GPIO_WritePin(LVQ_GPIO_Port, FV_Pin, 1);	// Set Y2 to 1

				// Change phase
				if (mouseDirectionY == 0) mouseEncoderPhaseY--; else mouseEncoderPhaseY++;

				// Decrement the distance left to move
				mouseDistanceY--;

				// Range check the phase
				if ((mouseDirectionY == 1) && (mouseEncoderPhaseY > 3)) mouseEncoderPhaseY = 0;
				if ((mouseDirectionY == 0) && (mouseEncoderPhaseY < 0)) mouseEncoderPhaseY = 3;
			} else {
				// Reset the phase if the mouse isn't moving
				mouseEncoderPhaseY = 0;
			}

			// Set the timer top value for the next interrupt
			if (yTimerTop == 0)
			{
				htim->Instance->ARR = 1;
			}else
			{
				htim->Instance->ARR = yTimerTop;
			}

	}


}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */

  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
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
     tex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
