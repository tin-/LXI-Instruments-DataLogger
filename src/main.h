#define ERROR_CREATE_THREAD -11
#define ERROR_JOIN_THREAD   -12
#define SUCCESS        0

#define MAX_CHANNELS   16
#define RESPONSE_LEN   64

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <libconfig.h>
#include <string.h>
#include <time.h>
#include <lxi.h>
#include <pthread.h>
#include <ncurses.h>
#include <malloc.h>
#include <locale.h>
#include <linux/i2c-dev.h>
#include <unistd.h>


static int i2c_fd;

typedef struct
{
    int device[MAX_CHANNELS];
    int Timeout[MAX_CHANNELS];
    const char *Device_name[MAX_CHANNELS];
    const char *IP[MAX_CHANNELS];
    const char *Read_command[MAX_CHANNELS];
    const char *Display_on_command[MAX_CHANNELS];
    const char *Display_off_command[MAX_CHANNELS];
    const char *Exit_command[MAX_CHANNELS];
    const char *Instance[MAX_CHANNELS];
    const char *Init_commands[MAX_CHANNELS][255];
    int Protocol[MAX_CHANNELS];
    int Port[MAX_CHANNELS];
    pthread_t tid[MAX_CHANNELS];
} ChannelsDef;


typedef struct
{
    int screen_timeout;
    int lxi_connect_timeout;
    const char *csv_dots;
    const char *csv_delimeter;
    int syncfs;
    int display_state;
} SettingsDef;

typedef struct
{   int i2c_address;
    int config_word;
    double delay;
    double tmp117_last_read_time;
    float temperature;
} temp_sensorsDef;

uint64_t sample_num = 0;
int term_x,term_y;
char response_massive[MAX_CHANNELS][RESPONSE_LEN];

WINDOW *log_win;
WINDOW *channels_win;
WINDOW *help_win;

void* measurement_thread(void *arg);

