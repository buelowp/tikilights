/*
 * torch.h
 *
 *  Created on: Apr 17, 2016
 *      Author: pete
 */

#ifndef USER_APPLICATIONS_TIKITORCH_TORCH_H_
#define USER_APPLICATIONS_TIKITORCH_TORCH_H_


#define LED_PIN     	D5
#define NUM_LEDS    	12

#define LATITUDE            42.058102
#define LONGITUDE           -87.984189
#define CST_OFFSET          -6
#define DST_OFFSET          (CST_OFFSET + 1)
#define TIME_BASE_YEAR		2019

#define BRIGHTNESS          250
#define WIFI_TIMEOUT        120000

#define ONE_HOUR            (1000 * 60 * 60)

typedef enum RUN_ANYWAY {
    NORMAL_OPERATION = 0,
    OOB_ON,
    OOB_OFF,
} e_runAnyway;


#endif /* USER_APPLICATIONS_TIKITORCH_TORCH_H_ */
