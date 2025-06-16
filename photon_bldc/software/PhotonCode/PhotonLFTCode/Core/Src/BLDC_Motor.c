/*
 * BLDC_Motor.c
 *
 *  Created on: Jun 7, 2025
 *      Author: Szymon Nyderek
 */

#include "main.h"
#include "tim.h"
#include "BLDC_motor.h"

void BLDC_ARM(BLDCmotor_t *motor, TIM_HandleTypeDef *Timer, uint32_t Channel)
{
	motor->Channel = Channel;
	motor->Timer = Timer;
//	HAL_TIM_PWM_Start(motor->Timer, motor->Channel);
//	__HAL_TIM_SET_COMPARE(motor->Timer, motor->Channel, 1600);
//	HAL_Delay(2000);
//	__HAL_TIM_SET_COMPARE(motor->Timer, motor->Channel, 1488);
//	HAL_Delay(2000);

	if(motor->iteration == 0)
	{
		motor->LastTime = HAL_GetTick();
		HAL_TIM_PWM_Start(motor->Timer, motor->Channel);
		__HAL_TIM_SET_COMPARE(motor->Timer, motor->Channel, 1600);
		motor->iteration = 2;
	}
	if((motor->iteration == 2) && (HAL_GetTick() > (motor->LastTime + 2000)))
	{
		motor->LastTime = HAL_GetTick();
		__HAL_TIM_SET_COMPARE(motor->Timer, motor->Channel, 1488);
		motor->iteration = 3;
	}
	if((motor->iteration == 3) && (HAL_GetTick() > (motor->LastTime + 2000)))
	{
	}

}

void BLDC_Set_Speed(BLDCmotor_t *motor)
{
	__HAL_TIM_SET_COMPARE(motor->Timer, motor->Channel, motor->Speed);
}
