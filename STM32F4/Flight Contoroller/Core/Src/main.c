/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
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
#include "dma.h"
#include "i2c.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bno080.h"
#include "quaternion.h"
#include "icm20602.h"
#include "FS-iA6B.h"
#include "pid control.h"
#include <string.h>
#include "AT24C08.h"
#include "lps22hh.h"
#include "M8N.h"
#include "XAVIER.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
int _write(int file, char* p, int len)
{
	for(int i=0;i<len;i++)
	{
		while(!LL_USART_IsActiveFlag_TXE(USART6));
		LL_USART_TransmitData8(USART6, *(p+i));
		HAL_Delay(1);
	}
}

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern uint8_t uart6_rx_flag;
extern uint8_t uart6_rx_data;

extern uint8_t m8n_rx_buf[36];
extern uint8_t m8n_rx_cplt_flag;

extern uint8_t ibus_rx_buf[32];
extern uint8_t ibus_rx_cplt_flag;

extern uint8_t uart1_rx_data;

extern uint8_t tim7_1ms_flag;
extern uint8_t tim7_20ms_flag;
extern uint8_t tim7_100ms_flag;
extern uint8_t tim7_1000ms_flag;

unsigned char failsafe_flag = 0;
unsigned char low_bat_flag = 0;

uint8_t telemetry_tx_buf[40];
uint8_t telemetry_rx_buf[20];
uint8_t telemetry_rx_cplt_flag;

extern uint8_t nx_rx_cplt_flag;
extern uint8_t nx_rx_buf;

float last_altitude;
float altitude_filt;
float baro_offset = 0;
signed int gps_height_offset = 0;
float batVolt;

float pressure_total_average = 0;
float pressure_rotating_mem[20] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
int pressure_rotating_mem_location = 0;
float actual_pressure_diff;
float actual_pressure_fast = 0, actual_pressure_slow = 0;

uint8_t ccr[18];
unsigned int ccr1 ,ccr2, ccr3, ccr4;

float theta, theta_radian;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
int Is_iBus_Throttle_min(void);
void ESC_Calibration(void);
int Is_iBus_Received(void);
void BNO080_Calibration(void);
float Sensor_fusion(float, float, float);

void Encode_Msg_AHRS(unsigned char* telemetry_tx_buf);
void Encode_Msg_GPS(unsigned char* telemetry_tx_buf);
void Encode_Msg_Altitude(unsigned char* telemetry_tx_buf);
void Encode_Msg_PID_Gain(unsigned char* telemetry_tx_buf, unsigned char id, float p, float i, float d);

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
float q[4];
float quatRadianAccuracy;
short gyro_x_offset = -6, gyro_y_offset = -19, gyro_z_offset = 4;
unsigned char motor_arming_flag = 0;
unsigned short iBus_SwA_Prev = 0;
unsigned char iBus_rx_cnt = 0;
float yaw_heading_reference;

unsigned int landing_throttle = 38640;
int manual_throttle;
int gps_cnt = 0;
int baro_cnt = 0;

unsigned int last_lon;
unsigned int last_lat;
uint8_t mode = 0;
float BNO080_Pitch_Offset = 1.1f;
float BNO080_Roll_Offset = 0;

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
  MX_DMA_Init();
  MX_TIM3_Init();
  MX_USART6_UART_Init();
  MX_SPI2_Init();
  MX_SPI1_Init();
  MX_UART5_Init();
  MX_TIM5_Init();
  MX_TIM7_Init();
  MX_USART1_UART_Init();
  MX_SPI3_Init();
  MX_I2C1_Init();
  MX_UART4_Init();
  /* USER CODE BEGIN 2 */
  LL_TIM_EnableCounter(TIM3); //Buzzer

  LL_USART_EnableIT_RXNE(UART4); //GPS
  LL_USART_EnableIT_RXNE(UART5); //FS-iA6B;
  LL_USART_EnableIT_RXNE(USART6); //Debug UART

  HAL_UART_Receive_IT(&huart1, &uart1_rx_data, 1); // Telemetry

  LL_TIM_EnableCounter(TIM5); //Motor PWM
  LL_TIM_CC_EnableChannel(TIM5, LL_TIM_CHANNEL_CH1); //Enable Timer Counting
  LL_TIM_CC_EnableChannel(TIM5, LL_TIM_CHANNEL_CH2); //Enable Timer Counting
  LL_TIM_CC_EnableChannel(TIM5, LL_TIM_CHANNEL_CH3); //Enable Timer Counting
  LL_TIM_CC_EnableChannel(TIM5, LL_TIM_CHANNEL_CH4); //Enable Timer Counting

  LL_TIM_EnableCounter(TIM7); //10Hz, 50Hz, 1kHz loop
  LL_TIM_EnableIT_UPDATE(TIM7);


  TIM3->PSC = 1000;
  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
  HAL_Delay(60);
  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
  HAL_Delay(60);
  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
  HAL_Delay(60);
  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
  HAL_Delay(60);


  printf("Checking sensor connection!\n");

  if(BNO080_Initialization() != 0)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  TIM3->PSC = 1000;
	  HAL_Delay(100);
	  TIM3->PSC = 1500;
	  HAL_Delay(100);
	  TIM3->PSC = 2000;
	  HAL_Delay(100);

	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  printf("\nBNO080 failed. Program shutting down...");
	  while(1)
	  {
		  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_0);
		  HAL_Delay(200);
		  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_0);
		  HAL_Delay(200);
	  }
  }
  BNO080_enableRotationVector(2500);

  if(ICM20602_Initialization() !=0 )
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4); //Enable Timer Counting

	  	  TIM3->PSC = 1000;
	  	  HAL_Delay(100);
	  	  TIM3->PSC = 1500;
	  	  HAL_Delay(100);
	  	  TIM3->PSC = 2000;
	  	  HAL_Delay(100);

	  	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  	  printf("\nICM20602 failed. Program shutting down...");
	  	  while(1)
	  	  {
	  		  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
	  		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_1);
	  		  HAL_Delay(200);
	  		  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
	  		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_1);
	  		  HAL_Delay(200);
	  	  }
  }

  /*LPS22HH Initialization*/
  if(LPS22HH_Initialization() != 0)
    {
  	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

  	  TIM3->PSC = 1000;
  	  HAL_Delay(100);
  	  TIM3->PSC = 1500;
  	  HAL_Delay(100);
  	  TIM3->PSC = 2000;
  	  HAL_Delay(100);

  	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

  	  printf("\nLPS22HH failed. Program shutting down...");
  	  while(1)
  	  {
  		  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
  		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_2);
  		  HAL_Delay(200);
  		  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
  		  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_2);
  		  HAL_Delay(200);
  	  }
    }



  /*GNSS Initialization*/
  M8N_Initialization();


  /*ICM20602 Initial Bias Correction*/
//  for(int i=0; i<250; i++)
//  {
//	  ICM20602_Get3AxisGyroRawData(&ICM20602.gyro_x_raw);
//
//	  ICM20602.gyro_x = ICM20602.gyro_x_raw * 2000.f / 32768.f;
//	  ICM20602.gyro_y = ICM20602.gyro_y_raw * 2000.f / 32768.f;
//	  ICM20602.gyro_z = ICM20602.gyro_z_raw * 2000.f / 32768.f;
//
//	  ICM20602.gyro_x = -ICM20602.gyro_x;
//	  ICM20602.gyro_z = -ICM20602.gyro_z;
//
//	  gyro_x_offset += ICM20602.gyro_x;
//	  gyro_y_offset += ICM20602.gyro_y;
//	  gyro_z_offset += ICM20602.gyro_z;
//
//	  HAL_Delay(2);
//  }
//  gyro_x_offset = gyro_x_offset/250.f;
//  gyro_y_offset = gyro_y_offset/250.f;
//  gyro_z_offset = gyro_z_offset/250.f;
//
//  HAL_Delay(5);

  ICM20602_Writebyte(0x13, (gyro_x_offset*-2)>>8);
  ICM20602_Writebyte(0x14, (gyro_x_offset*-2));

  ICM20602_Writebyte(0x15, (gyro_y_offset*-2)>>8);
  ICM20602_Writebyte(0x16, (gyro_y_offset*-2));

  ICM20602_Writebyte(0x17, (gyro_z_offset*-2)>>8);
  ICM20602_Writebyte(0x18, (gyro_z_offset*-2));

  printf("All sensor OK!\n\n");

  /*************Save Initial Gain into EEPROM**************/

EP_PIDGain_Read(0, &roll.in.kp, &roll.in.ki, &roll.in.kd);
Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 0, roll.in.kp, roll.in.ki, roll.in.kd);
HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);

EP_PIDGain_Read(1, &roll.out.kp, &roll.out.ki, &roll.out.kd);
Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 1, roll.out.kp, roll.out.ki, roll.out.kd);
HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);

EP_PIDGain_Read(2, &pitch.in.kp, &pitch.in.ki, &pitch.in.kd);
Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 2, pitch.in.kp, pitch.in.ki, pitch.in.kd);
HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);

EP_PIDGain_Read(3, &pitch.out.kp, &pitch.out.ki, &pitch.out.kd);
Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 3, pitch.out.kp, pitch.out.ki, pitch.out.kd);
HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);

EP_PIDGain_Read(4, &yaw_heading.kp, &yaw_heading.ki, &yaw_heading.kd);
Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 4, yaw_heading.kp, yaw_heading.ki, yaw_heading.kd);
HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);

EP_PIDGain_Read(5, &yaw_rate.kp, &yaw_rate.ki, &yaw_rate.kd);
Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 5, yaw_rate.kp, yaw_rate.ki, yaw_rate.kd);
HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);


altitude.out.kp = 0.3;
altitude.out.ki = 0;
altitude.out.kd = 0;
altitude.in.kp = 150;
altitude.in.ki = 0.1;
altitude.in.kd = 0.1;

gps_lon.out.kp = 50;
gps_lon.out.ki = 0;
gps_lon.out.kd = 0;
gps_lon.in.kp = 2;
gps_lon.in.ki = 0;
gps_lon.in.kd = 0;

gps_lat.out.kp = 50;
gps_lat.out.ki = 0;
gps_lat.out.kd = 0;
gps_lat.in.kp = 2;
gps_lat.in.ki = 0;
gps_lat.in.kd = 0;

/*Receiver Detection*/
  while(Is_iBus_Received() == 0)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4); //Enable Timer Counting
	  TIM3->PSC = 3000;
	  HAL_Delay(200);
	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  HAL_Delay(200);
  }

  /**************************ESC Calibration***********************************/
  if(iBus.SwC == 2000)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4); //Enable Timer Counting
	  TIM3->PSC = 1500;
	  HAL_Delay(500);
	  TIM3->PSC = 2000;
	  HAL_Delay(500);
	  TIM3->PSC = 1500;
	  HAL_Delay(500);
	  TIM3->PSC = 2000;
	  HAL_Delay(500);
	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  ESC_Calibration();
	  while(iBus.SwC != 1000)
	  {
		  Is_iBus_Received();

		  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		  TIM3->PSC = 1500;
		  HAL_Delay(200);
		  TIM3->PSC = 2000;
		  HAL_Delay(200);
		  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
	  }
  }


  /**************************BNO080 Calibration********************************/
  else if(iBus.SwC == 1500)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4); //Enable Timer Counting
	  TIM3->PSC = 1500;
	  HAL_Delay(500);
	  TIM3->PSC = 2000;
	  HAL_Delay(500);
	  TIM3->PSC = 1500;
	  HAL_Delay(500);
	  TIM3->PSC = 2000;
	  HAL_Delay(500);
	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  BNO080_Calibration();
	  while(iBus.SwC != 1000)
	  	  {
	  		  Is_iBus_Received();

	  		  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
	  		  TIM3->PSC = 1500;
	  		  HAL_Delay(200);
	  		  TIM3->PSC = 2000;
	  		  HAL_Delay(200);
	  		  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
	  	  }
  }

  /*********************Check Throttle value is minimum************************/
  while(Is_iBus_Throttle_min() == 0 || iBus.SwA == 2000)
  {
	  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4); //Enable Timer Counting
	  TIM3->PSC = 1000;
	  HAL_Delay(70);
	  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
	  HAL_Delay(70);
  }

  /*LPS22HH Initial Offset*/
  for(int i=0; i<20; i++)
  {
	  if(LPS22HH_DataReady() == 1)
	  {
		  LPS22HH_GetPressure(&LPS22HH.pressure_raw);
		  LPS22HH_GetTemperature(&LPS22HH.temperature_raw);

		  //Default Unit = 1m
		  LPS22HH.baroAlt = getAltitude2(LPS22HH.pressure_raw/4096.f, LPS22HH.temperature_raw/100.f);
		  baro_offset += LPS22HH.baroAlt;
		  HAL_Delay(100);

		  baro_cnt++;
	  }
  }

  baro_offset = baro_offset / baro_cnt;



//  for(int i=0; i<50; i++)
//  {
//	  if(m8n_rx_cplt_flag == 1) // GPS receive checking
//	  {
//		  m8n_rx_cplt_flag == 0;
//
//		  if(M8N_UBX_CHKSUM_Check(&m8n_rx_buf[0], 36) == 1)
//		  {
//			  //LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_2);
//			  M8N_UBX_NAV_POSLLH_Parsing(&m8n_rx_buf[0], &posllh);
//			  gps_cnt++;
//			  //		  printf("%d %ld %ld %d %d %d %ld\n", mode, posllh.lat, posllh.lon, (int)LPS22HH.baroAltFilt, (int)BNO080_Yaw, (int)XAVIER.mode, XAVIER.lat)
//		  }
//	  }
//	  HAL_Delay(200);
//	  gps_height_offset += posllh.height;
//  }
//
//  gps_height_offset /= gps_cnt;

  /********************* FC Ready to Fly ************************/

  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4); //Enable Timer Counting

  TIM3->PSC = 2000;
  HAL_Delay(100);
  TIM3->PSC = 1500;
  HAL_Delay(100);
  TIM3->PSC = 1000;
  HAL_Delay(100);

  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

  printf("Start\n");

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
//	  printf("%2f \n", actual_pressure_fast);
	  /********************* NX Message Parsing ************************/
//	  if(nx_rx_cplt_flag==1)
//	  {
//		  nx_rx_cplt_flag=0;
//
//		  XAVIER_Parsing(&nx_rx_buf, &XAVIER);
//	  }

	  /********************* GPS Data Parsing ************************/
	  if(m8n_rx_cplt_flag == 1) // GPS receive checking
	  {
		  m8n_rx_cplt_flag == 0;

		  if(M8N_UBX_CHKSUM_Check(&m8n_rx_buf[0], 36) == 1)
		  {
			  LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_2);
			  M8N_UBX_NAV_POSLLH_Parsing(&m8n_rx_buf[0], &posllh);
			  posllh.height -= gps_height_offset;

			  if((posllh.lon - posllh.lon_prev > 500) || (posllh.lon - posllh.lon_prev < -500)) posllh.lon = posllh.lon_prev;
			  if((posllh.lat - posllh.lat_prev > 500) || (posllh.lat - posllh.lat_prev < -500)) posllh.lat = posllh.lat_prev;

			  posllh.lon_prev = posllh.lon;
			  posllh.lat_prev = posllh.lat;
		  }
	  }

	  /********************* Telemetry Communication ************************/
	  if(telemetry_rx_cplt_flag == 1) //Receive GCS Message
	  	  {
	  		  telemetry_rx_cplt_flag = 0;

	  		  if(iBus.SwA == 1000) //Check FS-i6 Switch A
	  		  {
	  			  unsigned char chksum = 0xff;
	  			  for(int i=0;i<19;i++) chksum = chksum - telemetry_rx_buf[i];

	  			  if(chksum == telemetry_rx_buf[19]) //Check checksum of GCS Message
	  			  {
	  				  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  				  TIM3->PSC = 1000;
	  				  HAL_Delay(10);

	  				  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	  				  switch(telemetry_rx_buf[2]) //Check ID of GCS Message
	  				  {
	  				  case 0:
	  					  roll.in.kp = *(float*)&telemetry_rx_buf[3];
	  					  roll.in.ki = *(float*)&telemetry_rx_buf[7];
	  					  roll.in.kd = *(float*)&telemetry_rx_buf[11];
	  					  EP_PIDGain_Write(telemetry_rx_buf[2], roll.in.kp, roll.in.ki, roll.in.kd);
	  					  EP_PIDGain_Read(telemetry_rx_buf[2], &roll.in.kp, &roll.in.ki, &roll.in.kd);
	  					  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[2], roll.in.kp, roll.in.ki, roll.in.kd);
	  					  HAL_UART_Transmit_IT(&huart1, &telemetry_tx_buf[0], 20);
	  					  break;
	  				  case 1:
	  					  roll.out.kp = *(float*)&telemetry_rx_buf[3];
	  					  roll.out.ki = *(float*)&telemetry_rx_buf[7];
	  					  roll.out.kd = *(float*)&telemetry_rx_buf[11];
	  					  EP_PIDGain_Write(telemetry_rx_buf[2], roll.out.kp, roll.out.ki, roll.out.kd);
	  					  EP_PIDGain_Read(telemetry_rx_buf[2], &roll.out.kp, &roll.out.ki, &roll.out.kd);
	  					  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[2], roll.out.kp, roll.out.ki, roll.out.kd);
	  					  HAL_UART_Transmit_IT(&huart1, &telemetry_tx_buf[0], 20);
	  					  break;
	  				  case 2:
	  					  pitch.in.kp = *(float*)&telemetry_rx_buf[3];
	  					  pitch.in.ki = *(float*)&telemetry_rx_buf[7];
	  					  pitch.in.kd = *(float*)&telemetry_rx_buf[11];
	  					  EP_PIDGain_Write(telemetry_rx_buf[2], pitch.in.kp, pitch.in.ki, pitch.in.kd);
	  					  EP_PIDGain_Read(telemetry_rx_buf[2], &pitch.in.kp, &pitch.in.ki, &pitch.in.kd);
	  					  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[2], pitch.in.kp, pitch.in.ki, pitch.in.kd);
	  					  HAL_UART_Transmit_IT(&huart1, &telemetry_tx_buf[0], 20);
	  					  break;
	  				  case 3:
	  					  pitch.out.kp = *(float*)&telemetry_rx_buf[3];
	  					  pitch.out.ki = *(float*)&telemetry_rx_buf[7];
	  					  pitch.out.kd = *(float*)&telemetry_rx_buf[11];
	  					  EP_PIDGain_Write(telemetry_rx_buf[2], pitch.out.kp, pitch.out.ki, pitch.out.kd);
	  					  EP_PIDGain_Read(telemetry_rx_buf[2], &pitch.out.kp, &pitch.out.ki, &pitch.out.kd);
	  					  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[2], pitch.out.kp, pitch.out.ki, pitch.out.kd);
	  					  HAL_UART_Transmit_IT(&huart1, &telemetry_tx_buf[0], 20);
	  					  break;
	  				  case 4:
	  					  yaw_heading.kp = *(float*)&telemetry_rx_buf[3];
	  					  yaw_heading.ki = *(float*)&telemetry_rx_buf[7];
	  					  yaw_heading.kd = *(float*)&telemetry_rx_buf[11];
	  					  EP_PIDGain_Write(telemetry_rx_buf[2], yaw_heading.kp, yaw_heading.ki, yaw_heading.kd);
	  					  EP_PIDGain_Read(telemetry_rx_buf[2], &yaw_heading.kp, &yaw_heading.ki, &yaw_heading.kd);
	  					  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[2], yaw_heading.kp, yaw_heading.ki, yaw_heading.kd);
	  					  HAL_UART_Transmit_IT(&huart1, &telemetry_tx_buf[0], 20);
	  					  break;
	  				  case 5:
	  					  yaw_rate.kp = *(float*)&telemetry_rx_buf[3];
	  					  yaw_rate.ki = *(float*)&telemetry_rx_buf[7];
	  					  yaw_rate.kd = *(float*)&telemetry_rx_buf[11];
	  					  EP_PIDGain_Write(telemetry_rx_buf[2], yaw_rate.kp, yaw_rate.ki, yaw_rate.kd);
	  					  EP_PIDGain_Read(telemetry_rx_buf[2], &yaw_rate.kp, &yaw_rate.ki, &yaw_rate.kd);
	  					  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[2], yaw_rate.kp, yaw_rate.ki, yaw_rate.kd);
	  					  HAL_UART_Transmit_IT(&huart1, &telemetry_tx_buf[0], 20);
	  					  break;
	  				  case 0x10:
	  					  switch(telemetry_rx_buf[3]) //Check PID Gain ID of GCS PID Gain Request Message
	  					  {
	  					  case 0:
	  						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[3], roll.in.kp, roll.in.ki, roll.in.kd);
	  						  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  						  break;
	  					  case 1:
	  						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[3], roll.out.kp, roll.out.ki, roll.out.kd);
	  						  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  						  break;
	  					  case 2:
	  						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[3], pitch.in.kp, pitch.in.ki, pitch.in.kd);
	  						  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  						  break;
	  					  case 3:
	  						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[3], pitch.out.kp, pitch.out.ki, pitch.out.kd);
	  						  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  						  break;
	  					  case 4:
	  						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[3], yaw_heading.kp, yaw_heading.ki, yaw_heading.kd);
	  						  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  						  break;
	  					  case 5:
	  						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], telemetry_rx_buf[3], yaw_rate.kp, yaw_rate.ki, yaw_rate.kd);
	  						  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  						  break;
	  					  case 6:
	  						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 0, roll.in.kp, roll.in.ki, roll.in.kd);
	  						  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 1, roll.out.kp, roll.out.ki, roll.out.kd);
	  						  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 2, pitch.in.kp, pitch.in.ki, pitch.in.kd);
	  						  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 3, pitch.out.kp, pitch.out.ki, pitch.out.kd);
	  						  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 4, yaw_heading.kp, yaw_heading.ki, yaw_heading.kd);
	  						  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  						  Encode_Msg_PID_Gain(&telemetry_tx_buf[0], 5, yaw_rate.kp, yaw_rate.ki, yaw_rate.kd);
	  						  HAL_UART_Transmit(&huart1, &telemetry_tx_buf[0], 20, 10);
	  						  break;
	  					  }
	  					  break;
	  				  }
	  			  }
	  		  }
	  	  }


	  /********************* Flight Mode Detection / ESC Control / PID Calculation ************************/
	  if(tim7_1ms_flag==1)
	  {
		  tim7_1ms_flag = 0;

		  Double_Roll_Pitch_PID_Calculation(&pitch, (iBus.RV - 1500)*0.07f, BNO080_Pitch, ICM20602.gyro_x);
		  Double_Roll_Pitch_PID_Calculation(&roll, (iBus.RH - 1500)*0.07f, BNO080_Roll, ICM20602.gyro_y);

		  if(iBus.SwA == 2000 && iBus.SwB == 1000 && iBus.SwD == 2000 && iBus.LV < 1550 && iBus.LV > 1450) //Altitude Holding Mode
		  {
			  Double_Altitude_PID_Calculation(&altitude, last_altitude, actual_pressure_fast);

			  if(iBus.LH < 1485 || iBus.LH > 1515)
			  {
				  yaw_heading_reference = BNO080_Yaw;
				  Single_Yaw_Rate_PID_Calculation(&yaw_rate, (iBus.LH-1500), ICM20602.gyro_z);

				  ccr1 = 84000 + landing_throttle - pitch.in.pid_result + roll.in.pid_result -yaw_rate.pid_result+altitude.in.pid_result;
				  ccr2 = 84000 + landing_throttle + pitch.in.pid_result + roll.in.pid_result +yaw_rate.pid_result+altitude.in.pid_result;
				  ccr2 = (unsigned int)((float)ccr2 * 0.91f);
				  ccr3 = 84000 + landing_throttle + pitch.in.pid_result - roll.in.pid_result -yaw_rate.pid_result+altitude.in.pid_result;
				  ccr4 = 84000 + landing_throttle - pitch.in.pid_result - roll.in.pid_result +yaw_rate.pid_result+altitude.in.pid_result;
				  ccr4 = (unsigned int)((float)ccr4 * 0.91f);
			  }
			  else
			  {
				  Single_Yaw_Heading_PID_Calculation(&yaw_heading, yaw_heading_reference, BNO080_Yaw, ICM20602.gyro_z);
				  ccr1 = 84000 + landing_throttle - pitch.in.pid_result + roll.in.pid_result - yaw_heading.pid_result + altitude.in.pid_result;
				  ccr2 = 84000 + landing_throttle + pitch.in.pid_result + roll.in.pid_result + yaw_heading.pid_result + altitude.in.pid_result;
				  ccr2 = (unsigned int)((float)ccr2 * 0.91f);
				  ccr3 = 84000 + landing_throttle + pitch.in.pid_result - roll.in.pid_result - yaw_heading.pid_result + altitude.in.pid_result;
				  ccr4 = 84000 + landing_throttle - pitch.in.pid_result - roll.in.pid_result + yaw_heading.pid_result + altitude.in.pid_result;
				  ccr4 = (unsigned int)((float)ccr4 * 0.91f);
			  }
		  }


		  else if(iBus.SwA == 2000 && iBus.SwB == 2000 && iBus.LV < 1550 && iBus.LV > 1450) //GPS holding Mode
		  {
			  Double_GPS_PID_Calculation(&gps_lon, last_lon, posllh.lon);
			  Double_GPS_PID_Calculation(&gps_lat, last_lat, posllh.lat);
			  Double_Altitude_PID_Calculation(&altitude, last_altitude, actual_pressure_fast);

			  if ( (abs(iBus.RH-1500) < 50) && (abs(iBus.RV-1500) <50))
			  {
				  Single_Yaw_Heading_PID_Calculation(&yaw_heading, 0 , BNO080_Yaw, ICM20602.gyro_z);
				  ccr1 = 84000 + landing_throttle - gps_lon.in.pid_result * (-sin(theta_radian)) + gps_lat.in.pid_result * cos(theta_radian) + gps_lon.in.pid_result * cos(theta_radian) + gps_lat.in.pid_result * sin(theta_radian) -yaw_heading.pid_result  + altitude.in.pid_result;
				  ccr2 = 84000 + landing_throttle + gps_lon.in.pid_result * (-sin(theta_radian)) + gps_lat.in.pid_result * cos(theta_radian) + gps_lon.in.pid_result * cos(theta_radian) + gps_lat.in.pid_result * sin(theta_radian) +yaw_heading.pid_result  + altitude.in.pid_result;
				  ccr2 = (unsigned int)((float)ccr2 * 0.91f);
				  ccr3 = 84000 + landing_throttle + gps_lon.in.pid_result * (-sin(theta_radian)) + gps_lat.in.pid_result * cos(theta_radian) - gps_lon.in.pid_result * cos(theta_radian) + gps_lat.in.pid_result * sin(theta_radian) -yaw_heading.pid_result  + altitude.in.pid_result;
				  ccr4 = 84000 + landing_throttle - gps_lon.in.pid_result * (-sin(theta_radian)) + gps_lat.in.pid_result * cos(theta_radian) - gps_lon.in.pid_result * cos(theta_radian) + gps_lat.in.pid_result * sin(theta_radian) +yaw_heading.pid_result  + altitude.in.pid_result;
				  ccr4 = (unsigned int)((float)ccr4 * 0.91f);
			  }
			  else
			  {
				  ccr1 = 84000 + landing_throttle - pitch.in.pid_result + roll.in.pid_result - yaw_heading.pid_result + altitude.in.pid_result;
				  ccr2 = 84000 + landing_throttle + pitch.in.pid_result + roll.in.pid_result + yaw_heading.pid_result + altitude.in.pid_result;
				  ccr2 = (unsigned int)((float)ccr2 * 0.91f);
				  ccr3 = 84000 + landing_throttle + pitch.in.pid_result - roll.in.pid_result - yaw_heading.pid_result + altitude.in.pid_result;
				  ccr4 = 84000 + landing_throttle - pitch.in.pid_result - roll.in.pid_result + yaw_heading.pid_result + altitude.in.pid_result;
				  ccr4 = (unsigned int)((float)ccr4 * 0.91f);
			  }
		  }


		  else // Default Angle Mode
		  {
			  //			  if (iBus.LV < 1050)
			  //			  {
			  //				  ccr1 = 84000 + (iBus.LV - 1000) * 83.9;
			  //				  ccr2 = 84000 + (iBus.LV - 1000) * 83.9;
			  //				  ccr3 = 84000 + (iBus.LV - 1000) * 83.9;
			  //				  ccr4 = 84000 + (iBus.LV - 1000) * 83.9;
			  //			  }
			  //			  else
			  //			  {
			  if(iBus.LH < 1485 || iBus.LH > 1515)
			  {
				  yaw_heading_reference = BNO080_Yaw;
				  Single_Yaw_Rate_PID_Calculation(&yaw_rate, (iBus.LH-1500), ICM20602.gyro_z);

				  ccr1 = 84000 + (iBus.LV - 1000) * 83.9 - pitch.in.pid_result + roll.in.pid_result -yaw_rate.pid_result;
				  ccr2 = 84000 + (iBus.LV - 1000) * 83.9 + pitch.in.pid_result + roll.in.pid_result +yaw_rate.pid_result;
				  ccr2 = (unsigned int)((float)ccr2 * 0.91f);
				  ccr3 = 84000 + (iBus.LV - 1000) * 83.9 + pitch.in.pid_result - roll.in.pid_result -yaw_rate.pid_result;
				  ccr4 = 84000 + (iBus.LV - 1000) * 83.9 - pitch.in.pid_result - roll.in.pid_result +yaw_rate.pid_result;
				  ccr4 = (unsigned int)((float)ccr4 * 0.91f);
			  }
			  else
			  {
				  Single_Yaw_Heading_PID_Calculation(&yaw_heading, yaw_heading_reference, BNO080_Yaw, ICM20602.gyro_z);
				  ccr1 = 84000 + (iBus.LV - 1000) * 83.9 - pitch.in.pid_result + roll.in.pid_result -yaw_heading.pid_result;
				  ccr2 = 84000 + (iBus.LV - 1000) * 83.9 + pitch.in.pid_result + roll.in.pid_result +yaw_heading.pid_result;
				  ccr2 = (unsigned int)((float)ccr2 * 0.91f);
				  ccr3 = 84000 + (iBus.LV - 1000) * 83.9 + pitch.in.pid_result - roll.in.pid_result -yaw_heading.pid_result;
				  ccr4 = 84000 + (iBus.LV - 1000) * 83.9 - pitch.in.pid_result - roll.in.pid_result +yaw_heading.pid_result;
				  ccr4 = (unsigned int)((float)ccr4 * 0.91f);
			  }

			  last_lat = posllh.lat;
			  last_lon = posllh.lon;
			  last_altitude = actual_pressure_fast;
		  }
	  }


	  if(iBus.LV < 1030 || motor_arming_flag == 0)
	  {
		  Reset_All_PID_Integrator();
	  }


	  /********************* Motor Arming State ************************/
	  if(iBus.SwA == 2000 && iBus_SwA_Prev != 2000)
	  {
		  if(iBus.LV < 1010)
		  {
			  motor_arming_flag = 1;
			  yaw_heading_reference = BNO080_Yaw;

		  }
		  else
		  {
			  while(Is_iBus_Throttle_min() == 0 || iBus.SwA == 2000)
			  {
				  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4); //Enable Timer Counting
				  TIM3->PSC = 1000;
				  HAL_Delay(70);
				  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
				  HAL_Delay(70);
			  }
		  }
	  }
	  iBus_SwA_Prev = iBus.SwA;
	  if(iBus.SwA != 2000)
	  {
		  motor_arming_flag = 0;
	  }

	  if(motor_arming_flag == 1)
	  {
		  if(failsafe_flag == 0)
		  {
			  if(iBus.LV > 1050)
			  {
				  //			  printf("%d\t%d\t%d\t%d\n", ccr1, ccr2, ccr3, ccr4);
				  TIM5->CCR1 = ccr1 > 167999 ? 167999 : ccr1 < 84000 ? 84000 : ccr1;
				  TIM5->CCR2 = ccr2 > 167999 ? 167999 : ccr2 < 84000 ? 84000 : ccr2;
				  TIM5->CCR3 = ccr3 > 167999 ? 167999 : ccr3 < 84000 ? 84000 : ccr3;
				  TIM5->CCR4 = ccr4 > 167999 ? 167999 : ccr4 < 84000 ? 84000 : ccr4;
			  }
			  else
			  {
				  TIM5->CCR1 = 84000;
				  TIM5->CCR2 = 84000;
				  TIM5->CCR3 = 84000;
				  TIM5->CCR4 = 84000;
			  }
		  }
		  else
		  {
			  TIM5->CCR1 = 84000;
			  TIM5->CCR2 = 84000;
			  TIM5->CCR3 = 84000;
			  TIM5->CCR4 = 84000;
		  }
	  }

	  else
	  {
		  TIM5->CCR1 = 84000;
		  TIM5->CCR2 = 84000;
		  TIM5->CCR3 = 84000;
		  TIM5->CCR4 = 84000;
	  }


	  /********************* Telemetry Communication ************************/
	  if(tim7_20ms_flag == 1 && tim7_100ms_flag == 0)
	  {
		  tim7_20ms_flag = 0;
//		  Encode_Msg_AHRS(&telemetry_tx_buf[0]);
//		  HAL_UART_Transmit_IT(&huart1, &telemetry_tx_buf[0], 20);
	  }

	  else if(tim7_20ms_flag == 1 && tim7_100ms_flag == 1)
	  {
		  tim7_20ms_flag = 0;
		  tim7_100ms_flag = 0;
//		  Encode_Msg_AHRS(&telemetry_tx_buf[0]);
//		  Encode_Msg_GPS(&telemetry_tx_buf[20]);
//		  HAL_UART_Transmit_IT(&huart1, &telemetry_tx_buf[0], 40);
		  Encode_Msg_Altitude(&telemetry_tx_buf[0]);
		  HAL_UART_Transmit_DMA(&huart1, &telemetry_tx_buf[0], 14);
	  }


	  /***********************************************************************************************
	----------------------------Check BNO080 Sensor Value(current Angle Data)-----------------------
	   ***********************************************************************************************/
	  if(BNO080_dataAvailable() == 1)
	  {
		  q[0] = BNO080_getQuatI();
		  q[1] = BNO080_getQuatJ();
		  q[2] = BNO080_getQuatK();
		  q[3] = BNO080_getQuatReal();
		  quatRadianAccuracy = BNO080_getQuatRadianAccuracy();

		  Quaternion_Update(&q[0]);

		  BNO080_Roll = -BNO080_Roll;
		  BNO080_Roll -= BNO080_Roll_Offset;
		  BNO080_Pitch = -BNO080_Pitch;
		  BNO080_Pitch -= BNO080_Pitch_Offset;

		  float theta = 360.f - BNO080_Yaw;
		  float theta_radian = theta * 0.01745329252f;

//		  Check BNO080 Calibration value
//		  printf("%f\t%f\n", BNO080_Roll, BNO080_Pitch);
//		  printf("%.2f\n",BNO080_Yaw);
//		  printf("%d, %d, %d \n", (int)(BNO080_Roll*100), (int)(BNO080_Pitch*100), (int)(BNO080_Yaw*100));
	  }

	  /***********************************************************************************************
	----------------------Check ICM20602 Sensor Value(current Angular Velocity Data)------------------
	   ***********************************************************************************************/
	  if(ICM20602_DataReady() == 1)
	  {
		  ICM20602_Get3AxisGyroRawData(&ICM20602.gyro_x_raw);

		  // Gyro FS=2 (+500dps max)
		  ICM20602.gyro_x = ICM20602.gyro_x_raw / 65.5f;
		  ICM20602.gyro_y = ICM20602.gyro_y_raw / 65.5f;
		  ICM20602.gyro_z = ICM20602.gyro_z_raw / 65.5f;

		  ICM20602.gyro_x = -ICM20602.gyro_x;
		  ICM20602.gyro_z = -ICM20602.gyro_z;

//		  printf("%d, %d, %d \n", (int)ICM20602.gyro_x, (int)ICM20602.gyro_y, (int)ICM20602.gyro_z);
	  }

	  if(LPS22HH_DataReady() == 1)
	  {
		  LPS22HH_GetPressure(&LPS22HH.pressure_raw);
		  LPS22HH_GetTemperature(&LPS22HH.temperature_raw);

		  LPS22HH.baroAlt = getAltitude2(LPS22HH.pressure_raw/4096.f, LPS22HH.temperature_raw/100.f); //Default Unit = 1m

		  pressure_total_average -= pressure_rotating_mem[pressure_rotating_mem_location];
		  pressure_rotating_mem[pressure_rotating_mem_location] = LPS22HH.baroAlt - baro_offset;
		  pressure_total_average += pressure_rotating_mem[pressure_rotating_mem_location];
		  pressure_rotating_mem_location++;
		  if(pressure_rotating_mem_location ==20) pressure_rotating_mem_location = 0;
		  actual_pressure_fast = pressure_total_average / 20.0f;
		  actual_pressure_slow = actual_pressure_slow * 0.985f + actual_pressure_fast * 0.015f;

//		  actual_pressure_diff = actual_pressure_slow - actual_pressure_fast;
//		  if (actual_pressure_diff > 0.08f || actual_pressure_diff < -0.08f) actual_pressure_slow -= actual_pressure_fast / 6.0f;

//		  printf("%f, %f, %f\n", LPS22HH.baroAlt, baro_offset, actual_pressure_slow);
	  }


	  /***********************************************************************************************
	------------------------------Toggle Led if Checksum Data is right------------------------------
	   ***********************************************************************************************/
	  if(ibus_rx_cplt_flag==1)
	  {
		  ibus_rx_cplt_flag=0;
		  if(iBus_Check_CHKSUM(&ibus_rx_buf[0],32)==1)
		  {
			  LL_GPIO_TogglePin(GPIOC,LL_GPIO_PIN_2);

			  iBus_Parsing(&ibus_rx_buf, &iBus);
			  iBus_rx_cnt++;

			  if(iBus_isActiveFailSafe(&iBus) == 1)
			  {
				  failsafe_flag = 1;
			  }
			  else
			  {
				  failsafe_flag = 0;
			  }
		  }
	  }

	  if(tim7_1000ms_flag == 1)
	  {
		  tim7_1000ms_flag = 0;
		  if(iBus_rx_cnt == 0)
		  {
			  failsafe_flag = 2;
		  }
		  iBus_rx_cnt = 0;
	  }

	  if(failsafe_flag == 1 || failsafe_flag ==2 || low_bat_flag == 1)
	  {
		  LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4); //Buzzer On
	  }
	  else
	  {
		  LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4); //Buzzer Off
	  }
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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
int Is_iBus_Throttle_min(void)
{
	if(ibus_rx_cplt_flag==1)
	{
		ibus_rx_cplt_flag=0;
		if(iBus_Check_CHKSUM(&ibus_rx_buf[0],32)==1)
		{
			iBus_Parsing(&ibus_rx_buf, &iBus);
			if(iBus.LV < 1010) return 1;
		}
	}

	return 0;
}

void ESC_Calibration(void)
{
	  TIM5->CCR1 = 167999;
	  TIM5->CCR2 = 167999;
	  TIM5->CCR3 = 167999;
	  TIM5->CCR4 = 167999;
	  HAL_Delay(7000);

	  TIM5->CCR1 = 84000;
	  TIM5->CCR2 = 84000;
	  TIM5->CCR3 = 84000;
	  TIM5->CCR4 = 84000;
	  HAL_Delay(8000);
}

int Is_iBus_Received(void)
{
	if(ibus_rx_cplt_flag==1)
		{
			ibus_rx_cplt_flag=0;
			if(iBus_Check_CHKSUM(&ibus_rx_buf[0],32)==1)
			{
				iBus_Parsing(&ibus_rx_buf, &iBus);
				return 1;
			}
		}
		return 0;
}

void BNO080_Calibration(void)
{
	//Resets BNO080 to disable All output
	BNO080_Initialization();

	//BNO080/BNO085 Configuration
	//Enable dynamic calibration for accelerometer, gyroscope, and magnetometer
	//Enable Game Rotation Vector output
	//Enable Magnetic Field output
	BNO080_calibrateAll(); //Turn on cal for Accel, Gyro, and Mag
	BNO080_enableGameRotationVector(20000); //Send data update every 20ms (50Hz)
	BNO080_enableMagnetometer(20000); //Send data update every 20ms (50Hz)

	//Once magnetic field is 2 or 3, run the Save DCD Now command
  	printf("Calibrating BNO080. Pull up FS-i6 SWC to end calibration and save to flash\n");
  	printf("Output in form x, y, z, in uTesla\n\n");

	//while loop for calibration procedure
	//Iterates until iBus.SwC is mid point (1500)
	//Calibration procedure should be done while this loop is in iteration.
	while(iBus.SwC == 1500)
	{
		if(BNO080_dataAvailable() == 1)
		{
			//Observing the status bit of the magnetic field output
			float x = BNO080_getMagX();
			float y = BNO080_getMagY();
			float z = BNO080_getMagZ();
			unsigned char accuracy = BNO080_getMagAccuracy();

			float quatI = BNO080_getQuatI();
			float quatJ = BNO080_getQuatJ();
			float quatK = BNO080_getQuatK();
			float quatReal = BNO080_getQuatReal();
			unsigned char sensorAccuracy = BNO080_getQuatAccuracy();

			printf("%f,%f,%f,", x, y, z);
			if (accuracy == 0) printf("Unreliable\t");
			else if (accuracy == 1) printf("Low\t");
			else if (accuracy == 2) printf("Medium\t");
			else if (accuracy == 3) printf("High\t");

			printf("\t%f,%f,%f,%f,", quatI, quatI, quatI, quatReal);
			if (sensorAccuracy == 0) printf("Unreliable\n");
			else if (sensorAccuracy == 1) printf("Low\n");
			else if (sensorAccuracy == 2) printf("Medium\n");
			else if (sensorAccuracy == 3) printf("High\n");

			//Turn the LED and buzzer on when both accuracy and sensorAccuracy is high
			if(accuracy == 3 && sensorAccuracy == 3)
			{
				LL_GPIO_SetOutputPin(GPIOC, LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2);
				TIM3->PSC = 65000; //Very low frequency
				LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
			}
			else
			{
				LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2);
				LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
			}
		}

		Is_iBus_Received(); //Refreshes iBus Data for iBus.SwC
		HAL_Delay(100);
	}

	//Ends the loop when iBus.SwC is not mid point
	//Turn the LED and buzzer off
	LL_GPIO_ResetOutputPin(GPIOC, LL_GPIO_PIN_0 | LL_GPIO_PIN_1 | LL_GPIO_PIN_2);
	LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);

	//Saves the current dynamic calibration data (DCD) to memory
	//Sends command to get the latest calibration status
	BNO080_saveCalibration();
	BNO080_requestCalibrationStatus();

	//Wait for calibration response, timeout if no response
	int counter = 100;
	while(1)
	{
		if(--counter == 0) break;
		if(BNO080_dataAvailable())
		{
			//The IMU can report many different things. We must wait
			//for the ME Calibration Response Status byte to go to zero
			if(BNO080_calibrationComplete() == 1)
			{
				printf("\nCalibration data successfully stored\n");
				LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
				TIM3->PSC = 2000;
				HAL_Delay(300);
				TIM3->PSC = 1500;
				HAL_Delay(300);
				LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
				HAL_Delay(1000);
				break;
			}
		}
		HAL_Delay(10);
	}
	if(counter == 0)
	{
		printf("\nCalibration data failed to store. Please try again.\n");
		LL_TIM_CC_EnableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		TIM3->PSC = 1500;
		HAL_Delay(300);
		TIM3->PSC = 2000;
		HAL_Delay(300);
		LL_TIM_CC_DisableChannel(TIM3, LL_TIM_CHANNEL_CH4);
		HAL_Delay(1000);
	}

	//BNO080_endCalibration(); //Turns off all calibration
	//In general, calibration should be left on at all times. The BNO080
	//auto-calibrates and auto-records cal data roughly every 5 minutes

	//Resets BNO080 to disable Game Rotation Vector and Magnetometer
	//Enables Rotation Vector
	BNO080_Initialization();
	BNO080_enableRotationVector(2500); //Send data update every 2.5ms (400Hz)
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	static unsigned char cnt = 0;
	if(huart->Instance = USART1)
	{
		HAL_UART_Receive_IT(&huart1, &uart1_rx_data, 1);

		switch(cnt)
				{
				case 0:
					if(uart1_rx_data==0x47)
					{
						telemetry_rx_buf[cnt]=uart1_rx_data;
						cnt++;
					}
					break;
				case 1:
					if(uart1_rx_data==0x53)
					{
						telemetry_rx_buf[cnt]=uart1_rx_data;
						cnt++;
					}
					else
						cnt=0;
					break;

				case 19:
					telemetry_rx_buf[cnt]=uart1_rx_data;
					cnt=0;
					telemetry_rx_cplt_flag = 1;
					break;

				default:
					telemetry_rx_buf[cnt]=uart1_rx_data;
					cnt++;
					break;
				}
	}

}

void Encode_Msg_AHRS(unsigned char* telemetry_tx_buf)
{
	  telemetry_tx_buf[0] = 0x46;
	  telemetry_tx_buf[1] = 0x43;
	  telemetry_tx_buf[2] = 0x10;

	  telemetry_tx_buf[3] = (short)(BNO080_Roll*100);
	  telemetry_tx_buf[4] = ((short)(BNO080_Roll*100))>>8;

	  telemetry_tx_buf[5] = (short)(BNO080_Pitch*100);
	  telemetry_tx_buf[6] = ((short)(BNO080_Pitch*100))>>8;

	  telemetry_tx_buf[7] = (unsigned short)(BNO080_Yaw*100);
	  telemetry_tx_buf[8] = ((unsigned short)(BNO080_Yaw*100))>>8;

	  telemetry_tx_buf[9] = (short)(LPS22HH.baroAltFilt*10);
	  telemetry_tx_buf[10] = ((short)(LPS22HH.baroAltFilt*10))>>8;

	  telemetry_tx_buf[11] = (short)((iBus.RH-1500)*0.07f*100);
	  telemetry_tx_buf[12] = ((short)((iBus.RH-1500)*0.07f*100))>>8;

	  telemetry_tx_buf[13] = (short)((iBus.RV-1500)*0.07f*100);
	  telemetry_tx_buf[14] = ((short)((iBus.RV-1500)*0.07f*100))>>8;

	  telemetry_tx_buf[15] = (unsigned short)((iBus.LH-1000)*0.36f*100);
	  telemetry_tx_buf[16] = ((unsigned short)((iBus.LH-1000)*0.36f*100))>>8;

	  telemetry_tx_buf[17] = 0x00;
	  telemetry_tx_buf[18] = 0x00;

	  telemetry_tx_buf[19] = 0xff;

	  for(int i=0; i<19; i++)
	  {
		  telemetry_tx_buf[19] = telemetry_tx_buf[19] - telemetry_tx_buf[i];
	  }
}

void Encode_Msg_GPS(unsigned char* telemetry_tx_buf)
{
	  telemetry_tx_buf[0] = 0x46;
	  telemetry_tx_buf[1] = 0x43;
	  telemetry_tx_buf[2] = 0x10;

	  telemetry_tx_buf[3] = posllh.lat;
	  telemetry_tx_buf[4] = posllh.lat>>8;
	  telemetry_tx_buf[5] = posllh.lat>>16;
	  telemetry_tx_buf[6] = posllh.lat>>24;

	  telemetry_tx_buf[7] = posllh.lon;
	  telemetry_tx_buf[8] = posllh.lon>>8;
	  telemetry_tx_buf[9] = posllh.lon>>16;
	  telemetry_tx_buf[10] = posllh.lon>>24;

//	  telemetry_tx_buf[11] = (unsigned short)(bat.Volt*100);
//	  telemetry_tx_buf[12] = ((unsigned short)(bat.Volt*100))>>8;

	  telemetry_tx_buf[13] = iBus.SwA ==1000 ? 0 : 1;
	  telemetry_tx_buf[14] = iBus.SwC ==1000 ? 0 : iBus.SwC == 1500 ? 1 : 2;

	  telemetry_tx_buf[15] = iBus_isActiveFailSafe(&iBus);

	  telemetry_tx_buf[16] = 0x00;
	  telemetry_tx_buf[17] = 0x00;
	  telemetry_tx_buf[18] = 0x00;

	  telemetry_tx_buf[19] = 0xff;

	  for(int i=0; i<19; i++)
	  {
		  telemetry_tx_buf[19] = telemetry_tx_buf[19] - telemetry_tx_buf[i];
	  }
}

void Encode_Msg_PID_Gain(unsigned char* telemetry_tx_buf, unsigned char id, float p, float i, float d)
{
	  telemetry_tx_buf[0] = 0x46;
	  telemetry_tx_buf[1] = 0x43;
	  telemetry_tx_buf[2] = id;

//	  memcpy(telemetry_tx_buf[3], &p, 4);
//	  memcpy(telemetry_tx_buf[7], &i, 4);
//	  memcpy(telemetry_tx_buf[11], &d, 4);

	  *(float*)&telemetry_tx_buf[3] = p;
	  *(float*)&telemetry_tx_buf[7] = i;
	  *(float*)&telemetry_tx_buf[11] = d;

	  telemetry_tx_buf[15] = 0x00;
	  telemetry_tx_buf[16] = 0x00;
	  telemetry_tx_buf[17] = 0x00;
	  telemetry_tx_buf[18] = 0x00;

	  telemetry_tx_buf[19] = 0xff;

	  for(int i=0; i<19; i++)
	  {
		  telemetry_tx_buf[19] = telemetry_tx_buf[19] - telemetry_tx_buf[i];
	  }
}

float Sensor_fusion(float sensor1, float sensor2,float ratio)
{
	return sensor1 * ratio + sensor2 * (1.f - ratio);
}

void Encode_Msg_Motor(unsigned char* telemetry_tx_buf)
{
     telemetry_tx_buf[0] = 0x88;
     telemetry_tx_buf[1] = 0x18;

     telemetry_tx_buf[2] = ccr1 >> 24;
     telemetry_tx_buf[3] = ccr1 >> 16;
     telemetry_tx_buf[4] = ccr1 >> 8;
     telemetry_tx_buf[5] = ccr1;

     telemetry_tx_buf[6] = ccr2 >> 24;
     telemetry_tx_buf[7] = ccr2 >> 16;
     telemetry_tx_buf[8] = ccr2 >> 8;
     telemetry_tx_buf[9] = ccr2;

     telemetry_tx_buf[10] = ccr3 >> 24;
     telemetry_tx_buf[11] = ccr3 >> 16;
     telemetry_tx_buf[12] = ccr3 >> 8;
     telemetry_tx_buf[13] = ccr3;

     telemetry_tx_buf[14] = ccr4 >> 24;
     telemetry_tx_buf[15] = ccr4 >> 16;
     telemetry_tx_buf[16] = ccr4 >> 8;
     telemetry_tx_buf[17] = ccr4;

     telemetry_tx_buf[18] = iBus.LV >> 8;
     telemetry_tx_buf[19] = iBus.LV;

     telemetry_tx_buf[20] = ((int)pitch.in.pid_result) >> 24;
     telemetry_tx_buf[21] = ((int)pitch.in.pid_result) >> 16;
     telemetry_tx_buf[22] = ((int)pitch.in.pid_result) >> 8;
     telemetry_tx_buf[23] = ((int)pitch.in.pid_result);

     telemetry_tx_buf[24] = ((int)roll.in.pid_result) >> 24;
     telemetry_tx_buf[25] = ((int)roll.in.pid_result) >> 16;
     telemetry_tx_buf[26] = ((int)roll.in.pid_result) >> 8;
     telemetry_tx_buf[27] = ((int)roll.in.pid_result);

     telemetry_tx_buf[28] = ((int)yaw_heading.pid_result) >> 24;
     telemetry_tx_buf[29] = ((int)yaw_heading.pid_result) >> 16;
     telemetry_tx_buf[30] = ((int)yaw_heading.pid_result) >> 8;
     telemetry_tx_buf[31] = ((int)yaw_heading.pid_result);

//     telemetry_tx_buf[32] = ((int)altitude_filt) >> 24;
//     telemetry_tx_buf[33] = ((int)altitude_filt) >> 16;
//     telemetry_tx_buf[34] = ((int)altitude_filt) >> 8;
//     telemetry_tx_buf[35] = ((int)altitude_filt);

     telemetry_tx_buf[32] = ((int)actual_pressure_fast) >> 24;
     telemetry_tx_buf[33] = ((int)actual_pressure_fast) >> 16;
     telemetry_tx_buf[34] = ((int)actual_pressure_fast) >> 8;
     telemetry_tx_buf[35] = ((int)actual_pressure_fast);
}

void Encode_Msg_Altitude(unsigned char* telemetry_tx_buf)
{
	telemetry_tx_buf[0] = 0x88;
	telemetry_tx_buf[1] = 0x18;

	telemetry_tx_buf[2] = ((int)actual_pressure_fast) >> 24;
	telemetry_tx_buf[3] = ((int)actual_pressure_fast) >> 16;
	telemetry_tx_buf[4] = ((int)actual_pressure_fast) >> 8;
	telemetry_tx_buf[5] = ((int)actual_pressure_fast);

	telemetry_tx_buf[6] = ((int)last_altitude) >> 24;
	telemetry_tx_buf[7] = ((int)last_altitude) >> 16;
	telemetry_tx_buf[8] = ((int)last_altitude) >> 8;
	telemetry_tx_buf[9] = ((int)last_altitude);

	telemetry_tx_buf[10] = ((int)(altitude.out.error)) >> 24;
	telemetry_tx_buf[11] = ((int)(altitude.out.error)) >> 16;
	telemetry_tx_buf[12] = ((int)(altitude.out.error)) >> 8;
	telemetry_tx_buf[13] = ((int)(altitude.out.error));

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
  __disable_irq();
  while (1)
  {
  }
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
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
