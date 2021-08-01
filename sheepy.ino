/*************************************************************************************************/
/*  Project : Sheepy                                                                             */
/*  Author  : PEB <pebdev@lavache.com>                                                           */
/*  Date    : 31.08.2021                                                                         */
/*************************************************************************************************/
/*  This program is free software: you can redistribute it and/or modify                         */
/*  it under the terms of the GNU General Public License as published by                         */
/*  the Free Software Foundation, either version 3 of the License, or                            */
/*  (at your option) any later version.                                                          */
/*                                                                                               */
/*  This program is distributed in the hope that it will be useful,                              */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of                               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                                */
/*  GNU General Public License for more details.                                                 */
/*                                                                                               */
/*  You should have received a copy of the GNU General Public License                            */
/*  along with this program.  If not, see <https://www.gnu.org/licenses/>.                       */
/*************************************************************************************************/


/* I N C L U D E S *******************************************************************************/


/* D E F I N E S *********************************************************************************/
/* Mode */
#define NB_MAX_MODE           (3)

/* Action */
#define ACTION_SLEEP          (0)
#define ACTION_POWEROFF       (1)

/* Direction */
#define DIR_FALLING           (-1)
#define DIR_RAISING           (1)

/* GPIO */
#define IO_IN_MODE            (2)
#define IO_OUT_POWEROFF       (2)
#define IO_OUT_LED1           (5)


/* S T R U C T U R E S ****************************************************************************/
typedef struct strSleepMode {
  int action;
  double sleepTime_ms;
  double periodStart_ms;
  double periodStop_ms;
  double inspirationRation;
} tstrSleepMode;


/* D E C L A R A T I O N S ************************************************************************/
int sleepMode;                  /* 0=poweroff | 1=8min | 2=20min                                  */
double periodDuration_ms;       /* Time of the current period in ms                               */
double periodStep_ms;           /* Step time to increase period from start to stop period         */
double timePwmStep_ms;          /* Time of the pwm step for the current period in ms              */
double timePwmCounter_ms;       /* Counter to sum each pwm step for the current period in ms      */
double timeCounter_ms;          /* Time counter since sheepy is started                           */
int pwmDirection;               /* RISING | FALLING                                               */
int pwmValue;                   /* value of the 'pwm' [0:255]                                     */
tstrSleepMode sleepModeList[NB_MAX_MODE]; /* List of available modes                              */


/* I O  F U N C T I O N S *************************************************************************/
void io_led_update (void)
{
  analogWrite(IO_OUT_LED1, pwmValue);
}


/* M A I N  F U N C T I O N S *********************************************************************/
void sheepy_find_next_mode (void)
{
  sleepMode++;
  
  if (sleepMode >= NB_MAX_MODE)
    sleepMode = 0;
}

/*------------------------------------------------------------------------------------------------*/
void sheepy_apply_mode (int mode)
{
  double sum_period = 0.0;
  double previous_period_ms = 0.0;
  
  /* The period is dynamic (we start and finish not with the same rpm) */
  periodStep_ms = (sleepModeList[mode].periodStop_ms - sleepModeList[mode].periodStart_ms) / (sleepModeList[mode].sleepTime_ms);
  periodDuration_ms = sleepModeList[mode].periodStop_ms;

  /* We have two parameters that can match exactly : the total sleep time and the start/stop period.
   *  To solve this problem, we will :
   *  - compute the sum of period and stop the loop when the sum period will be greater than the wanted sleep time
   *  - save latest computed period, it will be our first period
   */
  while (periodDuration_ms > sleepModeList[mode].periodStart_ms)
  {
    previous_period_ms  = periodDuration_ms;
    periodDuration_ms   = previous_period_ms - (previous_period_ms * periodStep_ms);
    sum_period          += periodDuration_ms;
  }
  
  /* Reset variables */
  timePwmStep_ms    = 0.0;
  timePwmCounter_ms = 0.0;
  timeCounter_ms    = 0.0;
  pwmDirection      = DIR_RAISING;
  pwmValue          = -1;

  Serial.println("=========================================================================================================");
  Serial.print("periodDuration_ms=");
  Serial.print(periodDuration_ms);

  Serial.print(" | sleepTime_ms=");
  Serial.print(sleepModeList[mode].sleepTime_ms);
  
  Serial.print(" | periodStart_ms=");
  Serial.print(sleepModeList[mode].periodStart_ms);
  
  Serial.print(" | periodStop_ms=");
  Serial.print(sleepModeList[mode].periodStop_ms);
  
  Serial.print(" | periodStep_ms=");
  Serial.println(periodStep_ms);
  Serial.println("=========================================================================================================");
}

/*------------------------------------------------------------------------------------------------*/
void sheepy_state_update (void)
{
  double step_ms = 0.0;
  double previousPeriod_ms = 0.0;
  
  /* Finally, we update or time counter and pwm value */
  if (timePwmCounter_ms >= timePwmStep_ms)
  {
    timePwmCounter_ms = 0.0;
    pwmValue += pwmDirection;

    if (pwmValue <= 0)
    {
      pwmValue          = 0;
      pwmDirection      = DIR_RAISING;
      timePwmStep_ms    = (sleepModeList[sleepMode].inspirationRation * periodDuration_ms) / 255.0;

      /* If we have reached end of the latest period, we stop sheepy */
      if (periodDuration_ms >= sleepModeList[sleepMode].periodStop_ms)
      {
        sleepMode = 0;
      }
      
      /* Debug */
      Serial.print("timeCounter_min : ");
      Serial.print(timeCounter_ms/1000/60);
      Serial.print(" | periodDuration_ms : ");
      Serial.print(periodDuration_ms);
      Serial.print(" | DIR_RAISING : ");
      Serial.print(timePwmStep_ms);
    }

    if (pwmValue >= 255)
    {
      pwmValue          = 255;
      pwmDirection      = DIR_FALLING;
      timePwmStep_ms    = ((1.0-sleepModeList[sleepMode].inspirationRation) * periodDuration_ms) / 255.0;

      /* If prepare new period */
      previousPeriod_ms = periodDuration_ms;
      periodDuration_ms = previousPeriod_ms + (previousPeriod_ms * periodStep_ms);

      /* Debug */
      Serial.print(" | DIR_FALLING : ");
      Serial.println(timePwmStep_ms);
    }
  }
  else
  {
    /* We have a delay of 1ms between each call */
    timePwmCounter_ms += 1.0;
  }

  /* Count time since sheepy is started */
  timeCounter_ms += 1.0;
}

/*------------------------------------------------------------------------------------------------*/
void setup (void)
{
  Serial.begin (115200);
  Serial.println("=========================================================================================================");
  Serial.println("============================================  SHEEPY v1.0.0  ============================================");
  Serial.println("=========================================================================================================");
  Serial.println(" ");

  /* Hardware initilization */
  pinMode(IO_IN_MODE, INPUT_PULLUP);
  pinMode(IO_OUT_POWEROFF, OUTPUT);
  pinMode(IO_OUT_LED1, OUTPUT);
  digitalWrite(IO_OUT_POWEROFF, LOW);
  digitalWrite(IO_OUT_LED1, LOW);

  /* Variable initialization */
  sleepMode = 1;
  
  /* mode power off, user has to run around his house because Sheepy can't do anything fir him */
  sleepModeList[0].action             = ACTION_POWEROFF;
  
  /* mode to sleep in 8 minutes, user just need some help to sleep */
  sleepModeList[1].action             = ACTION_SLEEP;
  sleepModeList[1].sleepTime_ms       = 8.0 * 60000;
  sleepModeList[1].periodStart_ms     = 60000.0 / 10.0;  /* 10 RPM */
  sleepModeList[1].periodStop_ms      = 60000.0 / 6.0;   /* 6 RPM  */
  sleepModeList[1].inspirationRation  = 0.4;

  /* mode to sleep in 20 minutes, user must stop to drink coffee before going to bed */
  sleepModeList[2].action             = ACTION_SLEEP;
  sleepModeList[2].sleepTime_ms       = 20.0 * 60000;
  sleepModeList[2].periodStart_ms     = 60000.0 / 10.0;  /* 10 RPM */
  sleepModeList[2].periodStop_ms      = 60000.0 / 6.0;   /* 6 RPM  */
  sleepModeList[2].inspirationRation  = 0.4;

  /* Apply the default mode */
  sheepy_apply_mode (sleepMode);
}

/*------------------------------------------------------------------------------------------------*/
void loop(void)
{  
  /* We update the sheepy state */
  sheepy_state_update ();

  /* Update LED status */
  io_led_update ();
  
  /* If user would like to change sleep mode we reset sheepy status and settings */
  if (digitalRead(IO_IN_MODE) == true)
  {
    /* Switch to the next mode */
    sheepy_find_next_mode ();
    sheepy_apply_mode (sleepMode);
  }

  /* If the system or user ask to poweroff, do it */
  if (sleepModeList[sleepMode].action == ACTION_POWEROFF)
  {
    digitalWrite(IO_OUT_POWEROFF, HIGH);
    while (1)
    {
      delay(10);
    }
  }

  /* In ms */
  delay(1);
}
