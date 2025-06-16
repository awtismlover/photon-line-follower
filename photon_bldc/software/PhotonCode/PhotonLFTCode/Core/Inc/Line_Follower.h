/*
 * Line_Follower.h
 *
 *  Created on: 18 paź 2024
 *      Author: Szymon
 */

#include "BLDC_Motor.h"
#ifndef INC_LINE_FOLLOWER_H_
#define INC_LINE_FOLLOWER_H_

typedef struct
{
	float Kp;
	float Kd;

	int Turbine_Speed;
	uint32_t Turbine_Prep_Time;
	int Base_speed_R;
	int Base_speed_L;
	int Max_speed_R;
	int Max_speed_L;

	int Sharp_bend_speed_right;
	int Sharp_bend_speed_left;
	int Bend_speed_right;
	int Bend_speed_left;

	float battery_voltage;
	uint16_t Adc1_Values[9];
	uint16_t Adc2_Values[4];
	float Speed_level;
	float Speed_offset;

	BLDCmotor_t MotorR;
	BLDCmotor_t MotorL;
	BLDCmotor_t MotorT;


	//Sensor
	uint8_t SensorValues[12];
	uint16_t treshold;
	uint32_t LastEndTimer;
	int Last_end;
	int Last_idle;
	int SensorPosition;
	int actives;


	//PID
	int P,D;
	int Errors[10];
	int Last_error;

	uint8_t LineFollowing;


} LineFollower_t;

//Functions
void PID_control(LineFollower_t *LF);

#endif /* INC_LINE_FOLLOWER_H_ */
