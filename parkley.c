#pragma config(Sensor, S1,     sonarSensor,         sensorSONAR)
#pragma config(Sensor, S3,     bumperSensor,        sensorTouch)
#pragma config(Sensor, S4,     lightSensor,         sensorLightActive)
#pragma config(Motor,  motorA,          leftMotor,     tmotorNormal, openLoop, reversed, encoder)
#pragma config(Motor,  motorC,          rightMotor,    tmotorNormal, openLoop, reversed, encoder)
//*!!Code automatically generated by 'ROBOTC' configuration wizard               !!*//
/**********************************************************************************
* File: parkley.c
* Authors: Greg Nehus, Matt Odille
* Description:  Code for a self-parking car robot that uses a PID controller for line-following.  Written in RobotC.
**********************************************************************************/

/**********************************************************************************
* Tweakable macros
**********************************************************************************/
// light sensor
#define LIGHT_VALUE_CARPET 58
#define LIGHT_VALUE_TAPE 34

// PID tuning (using Ziegler�Nichols method)
#define PID_dT 0.002
#define PID_Pc 0.5
#define PID_Kc 2.5

// PID
#define OFFSET_TWEAK 0
#define PID_Tp 80.0
#define PID_Kp (0.6 * PID_Kc)
//#define PID_Ki 0.143
#define PID_Ki (0.5 * 2 * PID_Kp * PID_dT / PID_Pc)
#define PID_Kd (PID_Kp * PID_Pc / (8 * PID_dT))

#define PID_INTEGRAL_MAX 5500
#define PID_INTEGRAL_MIN -5500

// Car info
#define track 4.625
#define wheelbase 7.25
#define WHEEL_DIAMETER 5.5
#define turning_radius 6
//#define turning_radius 14.56

/**********************************************************************************
* Global variables
**********************************************************************************/
// PID
float Kp = PID_Kp;     //the Konstant for the proportional controller
float Tp = PID_Tp;     //the Target motor speed
float offset; //average of the tape and carpet readings
float Ki = PID_Ki;
float integral = 0.0;
float Kd = PID_Kd;

// Light sensor
float prevError = 0.0;
float error = 0.0;
bool isOffTape;

// Sonar sensor
int SonarValue;
int distanceFromWall = 0;

// Park
bool isParking = false;

/**********************************************************************************
* Function prototypes
**********************************************************************************/
float GetPID(float fError);
void park(float deltaX, float deltaY);
float inches_to_centimeters(float inches);
void drive(float distance, int leftSpeed, int rightSpeed);
float get_angle_between_circles(float deltaX, float deltaY);
float get_needed_park_y_coordinate(float deltaX);
void turn_around();

/**********************************************************************************
* Light sensor thread
**********************************************************************************/
task tLightSensor()
{
	nSchedulePriority = kDefaultTaskPriority;
	SensorMode[lightSensor] = modePercentage;
	int LightValue;

	while(!isParking)
	{
		// delay 3 milliseconds since that's about the fastest the light sensor can read
		wait1Msec(3);

		// take a sensor reading and calculate the error by subtracting the offset
		LightValue = SensorValue(lightSensor);
		error = LightValue - offset;
		//nxtDisplayTextLine(1, "S4=%d", LightValue);
		//nxtDisplayTextLine(3, "error=%f", error);
	}
	return;
}

/**********************************************************************************
* Sonar sensor thread
**********************************************************************************/
task tSonarSensor()
{
	nSchedulePriority = kDefaultTaskPriority;

	while(!isParking)
	{
	  // delay 15 milliseconds since that's about the fastest the sonar sensor can read
		wait1Msec(15);
		
		// take a sensor reading
		SonarValue = SensorValue[sonarSensor];
		
		if (distanceFromWall == 0) distanceFromWall = SonarValue;
		nMotorEncoder[motorA] = 0;
		//nxtDisplayCenteredTextLine(1,"Distance %d", distanceFromWall);
		while(SensorValue[sonarSensor] >= distanceFromWall + track * 2.54)
		{
		  //nxtDisplayCenteredBigTextLine(3, "boom=%d", abs(nMotorEncoder[motorA]));

		  if (abs(nMotorEncoder[motorA]) >= (long)(inches_to_centimeters(wheelbase)  / (PI * WHEEL_DIAMETER * 2/3 ) * 360.0 )   ){
		    isParking = true;
		    break;

		  };

		  //(long)abs(distance / (PI * WHEEL_DIAMETER) * 360.00)
		}
		if (!isParking)distanceFromWall = SonarValue;
		//nxtDisplayCenteredTextLine(1,"Distance %d", distanceFromWall);
	}
	
	return;
}


/**********************************************************************************
* Main thread
**********************************************************************************/
task main()
{
  nSchedulePriority = kDefaultTaskPriority;
  nVolume = 4;

	int controllerOutput;
	eraseDisplay();

	// initialize values of PID globals
	offset = ((LIGHT_VALUE_CARPET + LIGHT_VALUE_TAPE) / 2) + OFFSET_TWEAK;

	// start sonar sensor
	StartTask(tSonarSensor);
	wait1Msec(1000);

	// start light sensor
	StartTask(tLightSensor);
	wait1Msec(1000);

	// perform line-following
	motor[leftMotor] = 50;
	motor[rightMotor] = 50;
	while(!isParking)
	{
	  // get PID controller output
	  controllerOutput = GetPID(error);
	  
	  // if the robot ran off the end of the tape, then turn around to get back on
	  if (isOffTape)
	  {
	     turn_around();
	     distanceFromWall = 0;
	     integral = 0;
	     isOffTape = false;
	  }
	  //nxtDisplayTextLine(5, "L=%f", Tp + controllerOutput);
	  //nxtDisplayTextLine(7, "R=%f", Tp - controllerOutput);
	  
	  // set motor speeds based on PID controller
	  motor[leftMotor] = (int) (Tp + controllerOutput);
	  motor[rightMotor] = (int) (Tp - controllerOutput);
	}

	// perform parallel parking
	park((distanceFromWall/2.54)/2 , 1);

	// wait (in case sound needs to finish playing)
	wait1Msec(5000);
}





/**********************************************************************************
* Function: void turn_around()
* Parameters: none
* Return: None
* Description: Turns around once end of line is reached
**********************************************************************************/
void turn_around()
{
  motor[leftMotor] = 0;
  motor[rightMotor] = 0;
  wait10Msec(10);
  motor[leftMotor] = -40;
  motor[rightMotor] = 40;

  while (error < -5);

  motor[leftMotor] = 0;
  motor[rightMotor] = 0;
}


/**********************************************************************************
* Function: float GetPID()
* Parameters: takes the current error reading as its only argument
* Return: None
* Description: This method returns the PID output (a motor speed adjustment)
**********************************************************************************/
float GetPID(float fError)
{
  float pTerm = 0.0, dTerm = 0.0, iTerm = 0.0;

  // [p]roportional term calculations
  pTerm = Kp * fError;

  // [i]ntegral term calculations
  integral = integral + fError;
  if (integral <= PID_INTEGRAL_MIN) isOffTape = true;
  
  if (integral > PID_INTEGRAL_MAX)
  {
    integral = PID_INTEGRAL_MAX;
  }
  else
  {
    if (integral < PID_INTEGRAL_MIN)
	{
      integral = PID_INTEGRAL_MIN;
    }
  }
  iTerm = Ki * integral;

  nxtDisplayCenteredBigTextLine(3, "i=%f", integral);

  // [d]erivative term calculations
  dTerm = Kd * (fError - prevError);
  prevError = fError;

  return (pTerm + iTerm + dTerm);
}


/**********************************************************************************
* Function: void park()
* Parameters: float deltaX, float deltaY
* Return: None
* Description: This function performs parallel parking.
**********************************************************************************/
void park(float deltaX, float deltaY){
   float currentTurningCircleOrigin_X, currentTurningCircleOrigin_Y;
   float parkedTurningCircleOrigin_X, parkedTurningCircleOrigin_Y;
    eraseDisplay();
    //nxtDisplayCenteredTextLine(3, "%f, %f", deltaX, deltaY);
   deltaX = deltaX  ;
   currentTurningCircleOrigin_X = turning_radius - track/2;
   currentTurningCircleOrigin_Y = 0;

   parkedTurningCircleOrigin_X = deltaX - turning_radius + track/2;
   parkedTurningCircleOrigin_Y = deltaY;

   float circleDeltaX = parkedTurningCircleOrigin_X - currentTurningCircleOrigin_X;
   float nX = get_needed_park_y_coordinate(circleDeltaX);
    drive(inches_to_centimeters(nX-deltaY) * 2,50,50);

   //drive(inches_to_centimeters(turning_radius * PI * 2), -60,-20);
   //drive(inches_to_centimeters(turning_radius * PI * 2));

   //while(1);
   float angleBetweenCircle = get_angle_between_circles(circleDeltaX,deltaY + nX);

   //nxtDisplayCenteredTextLine(4, "angle= %f", angleBetweenCircle);
   //drive(inches_to_centimeters(turning_radius * angleBetweenCircle));


   //drive( inches_to_centimeters( turning_radius * (angleBetweenCircle) )+  wheelbase);
   drive(inches_to_centimeters(turning_radius * angleBetweenCircle), -60,-20);

   drive(inches_to_centimeters(turning_radius * angleBetweenCircle ) + 2, -20,-60);


   //drive(- turning_radius * get_angle_between_circles(deltaX, deltaY));
  //StartTask(we_are_the_champions);
}


/**********************************************************************************
* Function: float get_angle_between_circles()
* Parameters: float deltaX, float deltaY
* Return: None
* Description: This function finds the angle between 2 circles using the inverse tangent
**********************************************************************************/
float get_angle_between_circles(float deltaX, float deltaY){
    return atan(deltaY/deltaX);
}


/**********************************************************************************
* Function: void drive()
* Parameters: float distance, int leftSpeed, int rightSpeed
* Return: None
* Description: This function causes the robot to drive a specified distance at a specified speed
**********************************************************************************/
void drive(float distance, int leftSpeed, int rightSpeed)
{
  nMotorEncoder[leftMotor] = 0;
  nMotorEncoder[rightMotor] = 0;
  motor[leftMotor] =  leftSpeed;
  motor[rightMotor] = rightSpeed;

	while (1)
	{
	  // check bumper sensor
		if (SensorValue[bumperSensor] == 1)
		{
		  PlaySound(soundDownwardTones);
		  break;
		}

		// check motor encoder values and break if either reaches/exceeds the target
		if (leftSpeed > rightSpeed){
			if (abs(nMotorEncoder[leftMotor]) > (long)abs(distance / (PI * WHEEL_DIAMETER) * 360.00)) break;
		}else{
			if (abs(nMotorEncoder[rightMotor]) > (long)abs(distance / (PI * WHEEL_DIAMETER) * 360.00)) break;
		}
	}

  motor[leftMotor] = 0;
  motor[rightMotor] = 0;
}


/**********************************************************************************
* Function: float get_needed_park_y_coordinate()
* Parameters: float deltaX
* Return: None
* Description: This function calculates the needed y-coord, given a deltaX value
**********************************************************************************/
float get_needed_park_y_coordinate(float deltaX)
{
  float distance = ((turning_radius * 2) - track) * ((turning_radius * 2) - track);
  return sqrt( distance - (deltaX * deltaX) );
}


/**********************************************************************************
* Function: float inches_to_centimeters()
* Parameters: float inches
* Return: None
* Description: This function converts inches to centimeters
**********************************************************************************/
float inches_to_centimeters(float inches){
  return inches * 2.54;
}

//float get_distance(float deltaX, float deltaY){
//    return sqrt( deltaX * deltaX + deltaY * deltaY);
//}
