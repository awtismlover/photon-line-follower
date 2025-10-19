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

	float Turbine_Speed;
	uint32_t Turbine_Prep_Time;

	float Base_speed_R;
	float Base_speed_L;
	float Max_speed_R;
	float Max_speed_L;

	float Sharp_bend_speed_right;
	float Sharp_bend_speed_left;
	float Bend_speed_right;
	float Bend_speed_left;

	float battery_voltage;
	uint16_t Adc1_Values[9];
	uint16_t Adc2_Values[4];
	float Speed_level;
	float Speed_offset;

	BLDCmotor_t MotorR;
	BLDCmotor_t MotorL;
	BLDCmotor_t MotorT;
	float MaxSpeed;
	float AverageSpeed;
	float AverageSpeedSum;
	float AverageSpeedNum;
	float SmallestValueR;
	float SmallestValueL;

	/*SimpleMap*/
	float ChangePoint[32][2];
	int ChangePointsCount;
	uint8_t SimpleMapState;


	//Sensor
	uint8_t SensorValues[12];
	uint16_t treshold;
	uint32_t LastEndTimer;
	int Last_end;
	int Last_idle;
	int SensorPosition;
	int actives;


	//PID
	float P,D;
	int Errors[10];
	float Last_error;

	uint8_t LineFollowing;

	//DickShot
	uint32_t DSHOTTimer;
	uint32_t PreviousTimeDshot;
	uint32_t HighestDelayDshot;


} LineFollower_t;

//Functions
void PID_control(LineFollower_t *LF);
float GetAverageSpeed(LineFollower_t *LF);
void motor_control(LineFollower_t* LF, float pos_right, float pos_left);

#endif /* INC_LINE_FOLLOWER_H_ */
