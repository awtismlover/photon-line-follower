/*
 * BLDC_Motor.h
 *
 *  Created on: Jun 7, 2025
 *      Author: SNYDE
 */

#ifndef INC_BLDC_MOTOR_H_
#define INC_BLDC_MOTOR_H_

typedef struct
{
	TIM_HandleTypeDef *Timer;
	uint32_t counter_period;
	uint32_t Channel;
	uint32_t LastTime;
	float Speed;
	int iteration;
	//pid do rn if not then fuck you bitch

}BLDCmotor_t;

//Methods
void BLDC_ARM(BLDCmotor_t *motor, TIM_HandleTypeDef *Timer, uint32_t Channel);
void BLDC_Set_Speed(BLDCmotor_t *motor);


#endif /* INC_BLDC_MOTOR_H_ */
