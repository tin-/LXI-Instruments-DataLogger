#include "main.h"

SettingsDef Settings;

void* measurement_thread(void *arg)
{
    extern SettingsDef Settings;
    int myid = (int)((intptr_t)arg); 
    unsigned long i = 0;
    char response[RESPONSE_LEN];


	lxi_send(Settings.device[myid], Settings.Read_command[myid], strlen(Settings.Read_command[myid]), Settings.Timeout[myid]);  // Send SCPI commnd
	lxi_receive(Settings.device[myid], response, sizeof(response), Settings.Timeout[myid]);  // Wait for response

        while (strchr("\t\n\v\f\r ", response[i]) == NULL)
	{
	    if(strchr(".", response[i]) != NULL)response[i] = Settings.csv_dots[0];
	    response_massive[myid][i] = response[i];
	    i++;
	}
        response[i] = '\0';
	response_massive[myid][i] = '\0';


    return NULL;
}


// ---------------------------------------------------------------------------------------------------


int16_t i2c_read_temp(char i2c_dev_addr, char addr)
{
    int ret;
    char data[2]= {0, 0};
    struct i2c_msg msg[2];
    struct i2c_rdwr_ioctl_data xfer = {
	.msgs = msg,
	.nmsgs = 2,
    };


    if (i2c_dev_addr < 0 || i2c_dev_addr > 255) {
	fprintf(stderr, "i2c: Invalid I2C address. \n");
	return 0;
    }

    ret = ioctl(i2c_fd, I2C_SLAVE_FORCE, i2c_dev_addr);
    if (ret < 0) {
	perror("i2c: Failed to set i2c device address");
	return 0;
    }



    msg[0].addr = i2c_dev_addr;
    msg[0].flags = 0;
    msg[0].buf = &addr;
    msg[0].len = 1;

    msg[1].addr = i2c_dev_addr;
    msg[1].flags = I2C_M_RD;
    msg[1].buf = data;
    msg[1].len = 2;

    ioctl(i2c_fd, I2C_RDWR, &xfer);

    printf( "First: %d Senond %d ", data[0] ,data[1] );

    return (data[0]<<8) + data[1];
}


// ---------------------------------------------------------------------------------------------------


void read_temp()
{

    float result;


    i2c_fd = open( "/dev/i2c-1", O_RDWR );

    if (i2c_fd < 0) {
	perror("i2c: Failed to open i2c device");
	return;
    }

    result = i2c_read_temp(0x48, 0x00);

    close(i2c_fd);
    
    result*=0.0078125;

    if ( result > -256 ) 
    {
	printf( "Temperature is: %.3f\n", result );
    }

}


// ---------------------------------------------------------------------------------------------------

int main(int argc, char **argv)
{
  config_t cfg; 
  config_setting_t *setting, *init_commands;
  int i,k,exit_code=0;
  int status_addr,status;
  struct timespec start, stop;
  double accum;
  time_t tdate = time(NULL);
  const char *csv_dir;
  char time_in_char[32];
  char csv_file_name[512];
  int time_in_char_pos=0;


  lxi_init(); // Initialize LXI library

  // Initialize Libconfig library -------------------------------
  config_init(&cfg); // Initialize Libconfig library

  if(! config_read_file(&cfg, "lxiidl.cfg"))
  {
    fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg),
            config_error_line(&cfg), config_error_text(&cfg));
    config_destroy(&cfg);
    return(EXIT_FAILURE);
  }

  // Open CSV File
  if(!config_lookup_string(&cfg, "csv_save_dir", &csv_dir))csv_dir="./";

  strftime(time_in_char, sizeof(time_in_char), "%c", localtime(&tdate));
  csv_file_name[0]='\0';
  strcat(csv_file_name, csv_dir);
  strcat(csv_file_name, "/");
  strcat(csv_file_name, time_in_char);
  strcat(csv_file_name, ".csv");


  FILE *csv_file_descriptor = fopen(csv_file_name,"w");
  if(csv_file_descriptor == NULL) {
    printf("\n Error trying to open 'csv' file. %s",csv_file_name);
    return 1;
  }

  if(!config_lookup_string(&cfg, "csv_dots", &Settings.csv_dots))Settings.csv_dots=".";

  if(!config_lookup_int(&cfg, "screen_refresh_div", &Settings.screen_timeout))Settings.screen_timeout=1;
  setting = config_lookup(&cfg, "inventory.channels");
  int channel_count = config_setting_length(setting);
  // -------------------------------------------------------------


  // Initialize Ncurses ------------------------------------------
  setlocale(LC_ALL, "");
  initscr();
  cbreak();
  noecho();
  nonl();
  intrflush(stdscr, FALSE);
  keypad(stdscr, TRUE);
  curs_set(0);
  scrollok(stdscr, TRUE);
  nodelay(stdscr, TRUE);
  refresh();

  start_color();
  init_pair(1, COLOR_BLACK, COLOR_GREEN);
  init_pair(2, COLOR_YELLOW, COLOR_BLACK);

  getmaxyx(stdscr, term_y, term_x);

  channels_win = newwin(channel_count+3, 65, 0, 0);
  wattron(channels_win,COLOR_PAIR(2));
  box(channels_win, 0, 0);

  log_win = newwin(term_y-(channel_count+3)-1, term_x, channel_count+3, 0);
  scrollok(log_win, TRUE);

  help_win = newwin(1, term_x, term_y-1, 0);
  wattron(help_win,COLOR_PAIR(1));


  wprintw(help_win,"  SPACE - pause, q - quit, r - refresh window  ");

  wrefresh(log_win);
  wrefresh(channels_win);
  wrefresh(help_win);

  wmove(log_win, 0, 0);
  // -------------------------------------------------------------


  // Read settings -----------------------------------------------
  if(setting != NULL)
  {

    wmove(channels_win, 1, 1);
    wprintw(channels_win,"%-7s %-20s %-15s %-15s", "Channel", "Device", "IP", "Data");
    fprintf(csv_file_descriptor,"%-7s %-20s %-15s\n", "Channel", "Device", "IP");

    for(i = 0; i < channel_count; ++i)
    {
      config_setting_t *channels = config_setting_get_elem(setting, i);

      /* Выводим только те записи, если они имеют все нужные поля. */
      const char *init_single, *device_name, *IP, *Instance;
      int Port, Protocol;
      if(!(config_setting_lookup_string(channels, "device_name", &device_name)
           && config_setting_lookup_string(channels, "IP", &IP)
           && config_setting_lookup_int(channels, "Protocol", &Protocol)
           && config_setting_lookup_string(channels, "Instance", &Instance)
           && config_setting_lookup_int(channels, "Port", &Port)
           && config_setting_lookup_int(channels, "Timeout", &Settings.Timeout[i])
           && config_setting_lookup_string(channels, "Read_command", &Settings.Read_command[i])
         ))
        continue;

      wmove(channels_win, i+2, 1);
      wprintw(channels_win,"%-7i %-20s %-15s", i, device_name, IP);
      fprintf(csv_file_descriptor,"%-7i %-20s %-15s\n",  i, device_name, IP);
      wrefresh(channels_win);

      if(Protocol==1)
      {
         Settings.device[i] = lxi_connect(IP, Port, Instance, Settings.Timeout[i], VXI11);       // Try connect to LXI

      } else {
         Settings.device[i] = lxi_connect(IP, Port, Instance, Settings.Timeout[i], RAW);         // Try connect to LXI
      }

      if (Settings.device[i]<0)
      {

        wmove(channels_win, i+2, 46);
	wprintw(channels_win,"Connection failed!");
        wrefresh(channels_win);
        wprintw(log_win,"Can't connect to %s\n", device_name);
      } else 
      {
        // Send init commands to instruments
        init_commands = config_setting_get_member(channels, "Init_string");
        k = 0;
        if (init_commands) {
            while (1) {
                if (config_setting_get_elem(init_commands, k) == NULL) {
                    break;
                }
                init_single = config_setting_get_string_elem(init_commands, k);
		lxi_send(Settings.device[i], init_single, strlen(init_single), Settings.Timeout[i]);  // Send SCPI commnd
                ++k;
            }
        }
      }
    }
  }
fprintf(csv_file_descriptor,"--------------\n");
// -------------------------------------------------------------


// Start time measurement
clock_gettime( CLOCK_REALTIME, &start);


// Read data ---------------------------------------------------
int screen_timeout_count=0;
while(exit_code==0)
{


  switch (getch())
  {
    case 'q':
      exit_code++;
      break;

    case ' ':
      while(getch() != ' ')sleep(1);
      break;

    case 'r':
      getmaxyx(stdscr, term_y, term_x);
      wresize(log_win,term_y-(channel_count+3)-1, term_x);
      wresize(channels_win,channel_count+3, 65);
      mvwin(help_win,term_y-1, 0);
      wresize(help_win,1, term_x);

      box(channels_win, 0, 0);
      wrefresh(help_win);
      wrefresh(log_win);
      wrefresh(channels_win);
      break;
  }

  sample_num++;
  read_temp();
  for(i = 0; i < channel_count; ++i) // Start threads
  {
     if(Settings.device[i]>=0) { 
                         pthread_create(&(tid[i]), NULL, measurement_thread,  (void*)((intptr_t)i));  // запуск LXI измерения
                      } else 
                      {
                         response_massive[i][0]='0';
                         response_massive[i][1]='\0';
                      }
  }

  for(i = 0; i < channel_count; ++i) // Wait threads complete
  {
     if(Settings.device[i]>=0)
     { 
          status = pthread_join(tid[i], (void**)&status_addr);
          if (status != SUCCESS) {
            wprintw(log_win,"main error: can't join thread, status = %d\n", status);
            exit(ERROR_JOIN_THREAD);
          }
 
     }
  }
  clock_gettime( CLOCK_REALTIME, &stop); // Fix clock
  // ---------------------------------------------------------

  wprintw(log_win,"\n");

  // Draw log table and save CSV
  wprintw(log_win,"%-5u   ", sample_num);
  fprintf(csv_file_descriptor,"%llu;", sample_num);

  // Calculate time
  accum = ( stop.tv_sec - start.tv_sec )
        + ( stop.tv_nsec - start.tv_nsec )
        / 1E9;

  sprintf(time_in_char,"%1.4lf", accum );

  time_in_char_pos=0;
  while (strchr(",", time_in_char[time_in_char_pos]) == NULL)time_in_char_pos++;
  time_in_char[time_in_char_pos] = Settings.csv_dots[0];

  wprintw(log_win,"%s   ", time_in_char );
  fprintf(csv_file_descriptor,"%s;", time_in_char );


  for(i = 0; i < channel_count; ++i)
  {
    if(Settings.device[i]>=0)mvwprintw(channels_win, i+2, 46,"%-15s", response_massive[i]); // Print response
    wprintw(log_win,"%-20s", response_massive[i]);
    fprintf(csv_file_descriptor,"%s;", response_massive[i]);

  }

  // finish
  fprintf(csv_file_descriptor,"\n");

  if(screen_timeout_count>=Settings.screen_timeout)
  {
    wrefresh(channels_win);
    wrefresh(log_win);
    screen_timeout_count=0;
  } else screen_timeout_count++;
  // ---------------------------------------------------------


}


////////////////////////////////////////////////////////////////////////////////
  for(i = 0; i < channel_count; ++i) // закрытие потоков и инструментов
  {
     if(Settings.device[i]>=0)
     { 
          printw("Close: i=%i device=%i tid=%i\n", i, Settings.device[i], tid[i]);
          status = pthread_join(tid[i], NULL);
          printw("Closed!\n");
          lxi_disconnect(Settings.device[i]);
     }
  }

  fclose(csv_file_descriptor);
  pthread_exit(NULL); 
  delwin(channels_win);
  delwin(log_win);
  delwin(help_win);
  endwin();  
  config_destroy(&cfg);
  return(EXIT_SUCCESS);
}
