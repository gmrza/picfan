/*****************************************************
 * PI Fan Controller
 *****************************************************
 */

#include <bcm2835.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#define PIN RPI_GPIO_P1_12

#define PWM_CHANNEL 0
#define MAX_DUTY max_duty
#define MIN_DUTY min_duty
#define TARGET_TEMP target_temp
#define STATUS_FIFO "/run/fanspeed"
#define MAX_DELAY max_delay  // milliseconds
#define SCALE_FACTOR scale_factor

// Standard temperature settings
#define COOLER  0
#define COOL    1
#define NORMAL  2
#define QUIET   3
#define QUIETER 4
#define CUSTOM  5


float    temps[5] = {40.0, 45.0, 50.0, 55.0, 60.0};
int      setting = NORMAL;


int      max_duty = 400;     // Max duty cycle = RANGE on bcm2835
int      min_duty;
int      verbose = 0;
float    target_temp = 50.0;
float    speed = 2.0;
float    scale_factor;
unsigned stopped = 0;
unsigned max_delay = 5000;
float    attack = 2.0,
         decay = 1.0;


/*
 * Sleep function which takes signals into account
 */
void my_sleep(unsigned s, unsigned ms){
    struct timespec req,
                    rem;

    req.tv_sec = s;
    req.tv_nsec = 1000000 * ms;

    while ((nanosleep(&req, &rem) != 0) && (errno == EINTR)) {
       // We need to loop back to sleep the remainder
        req = rem;
    }
}


/*
 *  Measure CPU temperature
 */
float cpu_temp(void) {
    float systemp, millideg;
    FILE *thermal;
    int n;

    thermal = fopen("/sys/class/thermal/thermal_zone0/temp","r");
    n = fscanf(thermal,"%f",&millideg);
    fclose(thermal);
    systemp = millideg / 1000.0;

    return systemp;
}


/*
 *  Read Command Line Arguments
 */
void read_options(int argc, char ** argv) {
    int opt;

    // put ':' in the starting of the 
    // string so that program can  
    //distinguish between '?' and ':'  
    while((opt = getopt(argc, argv, ":x:t:d:m:s:a:D:vCQ")) != -1)  {  
        switch(opt)  {  
            case 'd':  
                sscanf(optarg, "%d", &max_delay);
                max_delay *= 1000; // we want it in milliseconds
                if (verbose) printf("max_delay: %s\n", optarg);  
                fflush(stdout);
                break;  
            case 'm':  
                sscanf(optarg, "%d", &min_duty);
                if (verbose) printf("min_duty: %d\n", min_duty);  
                fflush(stdout);
                break;  
            case 'x':  
                sscanf(optarg, "%d", &max_duty);
                if (verbose) printf("max_duty: %f\n", max_duty);  
                fflush(stdout);
                break;  
            case 'v':  
                (verbose) = 1;
                break;  
            case 'C':  
                if (setting == COOL)
                    setting = COOLER;
                else
                    setting = COOL;
                break;  
            case 'Q':  
                if (setting == QUIET)
                    setting = QUIETER;
                else
                    setting = QUIET;
                break;  
            case 't':
                sscanf(optarg, "%f", &target_temp);
                if (verbose) printf("target_temp: %f\n", target_temp);  
                fflush(stdout);
                setting = CUSTOM;
                break;  
            case 's':
                sscanf(optarg, "%f", &scale_factor);
                if (verbose) printf("scale_factor: %f\n", scale_factor);  
                fflush(stdout);
                break;  
            case 'a':
                sscanf(optarg, "%f", &attack);
                if (verbose) printf("attack: %f\n", attack);  
                fflush(stdout);
                break;  
            case 'D':
                sscanf(optarg, "%f", &decay);
                if (verbose) printf("decay: %f\n", decay);  
                fflush(stdout);
                break;  
            case ':':  
                printf("option needs a value\n");  
                fflush(stdout);
                exit(2);
                break;  
            case '?':  
                printf("Usage %s [OPTIONS]\n", argv[0]);
                printf("  -v          : verbose\n");
                // ":x:t:d:m:s:a:D:v"
                fflush(stdout);
                exit(2);
                break;  
        }  
    }  
}


/*
 *  Write out status to a named pipe
 */
void * write_status(void * arg) {
    FILE * outfifo;
    float value;

    mkfifo(STATUS_FIFO, 0644);

    while(1) {
        outfifo = fopen(STATUS_FIFO, "w");
        if (!stopped)
            value = speed;
        else
            value = 0.0;

        fprintf(outfifo, "%f\n", value/MAX_DUTY*100.0);
        fclose(outfifo);
        usleep(250000);
    }
}


int main(int argc, char **argv)
{
    float     temp,
              dif,
              oldtemp,
              distance,
              velocity,
              direction,
              sign;
    unsigned  min_count = 0,
              delay = MAX_DELAY,
              value,
              i;
    pthread_t child;

    min_duty = (MAX_DUTY / 56 );
    scale_factor = (MAX_DUTY * MAX_DELAY / 2000000.0);

    read_options(argc, argv);

    if (setting != CUSTOM)
        target_temp = temps[setting];
    if (verbose) fprintf(stderr, "Target temperature: %f\n", target_temp);

    // Adjust target temp down 0.5 degrees - tehen we tend to hit it!
    target_temp -= 0.5;

    if (pthread_create(&child, NULL, &write_status, NULL) != 0) {
        fprintf(stderr, "Cannot create thread");
        exit(2);
    } else {
        if (!bcm2835_init())
            return 1;
        // Set the output pin to Alt Fun 5, to allow PWM channel 0 to be output there
        bcm2835_gpio_fsel(PIN, BCM2835_GPIO_FSEL_ALT5);
    
        bcm2835_pwm_set_clock(2); // set divider to 2
        bcm2835_pwm_set_mode(PWM_CHANNEL, 1, 1); // mark-space
        bcm2835_pwm_set_range(PWM_CHANNEL, MAX_DUTY); // range 

        // For starters, get the fan going
        bcm2835_pwm_set_data(PWM_CHANNEL, MAX_DUTY / 2);
        my_sleep(0, 300);
        bcm2835_pwm_set_data(PWM_CHANNEL, MIN_DUTY);
        my_sleep(0, 500);
        if (verbose) printf("Scale factor : %f\n", SCALE_FACTOR);
        if (verbose) printf("starting\n");
        fflush(stdout);

        oldtemp = TARGET_TEMP;
        speed = (float)MIN_DUTY;
        stopped = 0;
        delay = MAX_DELAY;
        while(1) {
            if (delay >= 5000) {
                my_sleep((delay - 5000) / 1000, 0);
                temp = 0.0;
                for (i = 0 ; i < 5 ; i++) {
                    temp += cpu_temp();
                    my_sleep(1, 0);
                }
                temp /= 5.0;
            } else {
                my_sleep(delay /1000, 0);
                temp = cpu_temp();
            }

            velocity = (temp - oldtemp) / (delay / 1000 );
            distance = temp - TARGET_TEMP;
            oldtemp = temp;
            direction = velocity * distance;

            sign = (distance >= 0.0) ? 1.0 : -1.0;

            if (distance > 2.0 ) {
                delay = MAX_DELAY / 5;
            } else if ((distance < 0.5 ) && (distance > -1.5)) {
                delay = MAX_DELAY * 2;
            } else {
                delay = MAX_DELAY;
            }
            
            if (verbose) fprintf(stderr, "v: %f", velocity);
            if ((distance < 0.0) && stopped ) {
                speed = 0.0;
                delay = MAX_DELAY * 2;
            } else if ((distance > 0.0 ) && stopped) {
                speed = MIN_DUTY;
                delay = MAX_DELAY / 2;
            } else if (distance > 0.0) {
                if (velocity < -0.25)
                    speed += speed / 20.0  * decay / velocity;
                if (velocity < 0.0) {
                    speed -= 1.0;
                } else if (velocity > 0.25) {
                    speed += distance * distance * attack * velocity ;
                } else if (distance < 5.0){
                    if (distance > 2.0) speed += 1.0;
                    if (distance > 1.0) speed += 1.0;
                    speed += 1.0 ;
                } else
                    speed += distance * distance * attack;
            } else if ((distance < 0.0) && (velocity > 0.0)) {
                speed -= distance * velocity * decay;
            } else if ((distance < 0.0) && (velocity < 0.0)) {
                delay = MAX_DELAY / 2.5;
                if (distance < -1.5) speed -= 1.0;
                if (distance < -2.5) speed -= 1.0;
                if (velocity > -0.5)
                    speed -= 1.0;
                else {
                    // speed += speed / 20.0 * velocity * decay;
                    speed += velocity * decay * 2.0 ;
                    delay = MAX_DELAY / 2.5;
                }
            } else if ((distance < 0.0 )) {
                speed -= distance * distance * decay;
            }

            if (speed > (float)MAX_DUTY ) {
                speed = (float)MAX_DUTY;
            }
            if (speed < (float)MIN_DUTY) {
                min_count++;
                speed = (float)MIN_DUTY;
            } else {
                min_count = 0;
            }
            if (min_count >= 10){
                speed = 0.0;
                value = 0;
                stopped = 1;
            } else {
                value = (unsigned)speed;
                if(stopped) {
                   bcm2835_pwm_set_data(PWM_CHANNEL, MAX_DUTY / 2);
                   usleep(500000);
                   stopped = 0;
                }
            }

            if (verbose) printf("fan: %d, temp: %f\n", value, temp);
            fflush(stdout);
            bcm2835_pwm_set_data(PWM_CHANNEL, value);

        }
    }

    bcm2835_close();
    return 0;
}