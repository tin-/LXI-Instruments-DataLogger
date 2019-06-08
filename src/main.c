#include "main.h"
#include <features.h>
// ---------------------------------------------------------------------------------------------------
//
//
//
// ---------------------------------------------------------------------------------------------------
void *measurement_thread(void *arg)
{
  extern SettingsDef Settings;
  extern ChannelsDef Channels;
  int myid = (int) ((intptr_t) arg);
  unsigned long i = 0;
  char response[RESPONSE_LEN];

  send_command_to_instrument(myid, Channels.Read_command[myid]);
  lxi_receive(Channels.device[myid], response, sizeof(response), Channels.Timeout[myid]);       // Wait for response

  while (strchr("\t\n\v\f\r ", response[i]) == NULL)
  {
    if(strchr(".", response[i]) != NULL)
      response[i] = Settings.csv_dots[0];
    response_massive[myid][i] = response[i];
    i++;
  }
  response[i] = '\0';
  response_massive[myid][i] = '\0';

  return NULL;
}

// ---------------------------------------------------------------------------------------------------
//
//
//
// ---------------------------------------------------------------------------------------------------
void send_command_to_instrument(int chan, const char *arg)
{
  extern ChannelsDef Channels;
  char command[SEND_LEN];

  strcpy(command, arg);
  if(Channels.Protocol[chan] == 0)
    strcat(command, "\n");
  lxi_send(Channels.device[chan], command, strlen(command), Channels.Timeout[chan]);    // Send SCPI commnd

  return;
}

// ---------------------------------------------------------------------------------------------------
//
//
//
// ---------------------------------------------------------------------------------------------------
void i2c_write(char i2c_dev_addr, char register_pointer, char data_MSB, char data_LSB)
{
  int ret;

  char data[3] = { register_pointer, data_MSB, data_LSB };
  struct i2c_msg msg[1];
  struct i2c_rdwr_ioctl_data xfer = {
    .msgs = msg,
    .nmsgs = 1,
  };


  if(i2c_dev_addr < 0 || i2c_dev_addr > 255)
  {
    fprintf(stderr, "i2c: Invalid I2C address. \n");
    return;
  }

  ret = ioctl(i2c_fd, I2C_SLAVE_FORCE, i2c_dev_addr);
  if(ret < 0)
  {
    perror("i2c: Failed to set i2c device address");
    return;
  }


  msg[0].addr = i2c_dev_addr;
  msg[0].flags = 0;
  msg[0].buf = data;
  msg[0].len = 3;

  ioctl(i2c_fd, I2C_RDWR, &xfer);

  return;
}

// ---------------------------------------------------------------------------------------------------
//
//
//
// ---------------------------------------------------------------------------------------------------
int16_t i2c_read_temp(char i2c_dev_addr, char addr)
{
  int ret;
  char data[2] = { 0, 0 };
  struct i2c_msg msg[2];
  struct i2c_rdwr_ioctl_data xfer = {
    .msgs = msg,
    .nmsgs = 2,
  };

  if(i2c_dev_addr < 0 || i2c_dev_addr > 255)
  {
    fprintf(stderr, "i2c: Invalid I2C address. \n");
    return 0;
  }

  ret = ioctl(i2c_fd, I2C_SLAVE_FORCE, i2c_dev_addr);
  if(ret < 0)
  {
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

  return (data[0] << 8) + data[1];
}

// ---------------------------------------------------------------------------------------------------
//
//
//
// ---------------------------------------------------------------------------------------------------
void configure_tmp117(int addr, int config)
{
  char config1 = (config >> 8) & 0xFF;
  char config2 = config & 0xFF;
  i2c_fd = open(i2c_file_name, O_RDWR);

  if(i2c_fd < 0)
  {
    perror("i2c: Failed to open i2c device");
    return;
  }
  i2c_write(addr & 0xFF, 0x01, config1, config2);

  close(i2c_fd);
}

// ---------------------------------------------------------------------------------------------------
//
//
//
// ---------------------------------------------------------------------------------------------------
void read_temp(int chan, int addr)
{
  float result;

  i2c_fd = open(i2c_file_name, O_RDWR);

  if(i2c_fd < 0)
  {
    perror("i2c: Failed to open i2c device");
    return;
  }

  result = i2c_read_temp(addr & 0xFF, 0x00);    // Read temperature register

  close(i2c_fd);

  result *= 0.0078125;

  if(result > -256)
  {
    temperature_sensors[chan].temperature = result;
  }
}

// ---------------------------------------------------------------------------------------------------
//
//
//
// ---------------------------------------------------------------------------------------------------
void init_config()
{
  const char *csv_dir;
  char time_in_char[32];
  time_t tdate = time(NULL);
  char csv_file_name[512];
  int i, k;

  config_init(&cfg);            // Initialize Libconfig library

  if(!config_read_file(&cfg, "lxiidl.cfg"))
  {
    fprintf(stderr, "%s:%d - %s\n", config_error_file(&cfg), config_error_line(&cfg), config_error_text(&cfg));
    config_destroy(&cfg);
    exit_code++;
    return;
  }


  if(!config_lookup_string(&cfg, "dev_file", &i2c_file_name))
    i2c_file_name = "/dev/i2c-1";

  // Open CSV File
  if(!config_lookup_string(&cfg, "csv_save_dir", &csv_dir))
    csv_dir = "./";

  strftime(time_in_char, sizeof(time_in_char), "%d_%m_%Y_%H:%M:%S", localtime(&tdate));
  csv_file_name[0] = '\0';
  strcat(csv_file_name, csv_dir);
  strcat(csv_file_name, "/");
  strcat(csv_file_name, time_in_char);
  strcat(csv_file_name, ".csv");

  csv_file_descriptor = fopen(csv_file_name, "w");
  if(csv_file_descriptor == NULL)
  {
    printf("\n Error trying to open 'csv' file. %s", csv_file_name);
    exit_code++;
    return;
  }

  strcat(csv_file_name, ".js");
  js_file_descriptor = fopen(csv_file_name, "w");
  if(js_file_descriptor == NULL)
  {
    printf("\n Error trying to open 'csv.js' file. %s", csv_file_name);
    exit_code++;
    return;
  }


  fprintf(js_file_descriptor, "var skip_samples = 1;    // Skip first samples\n");
  fprintf(js_file_descriptor, "var cut_samples = 1666600; // Maximum  samples\n");
  fprintf(js_file_descriptor, "var avgSamples = 64; // Moving average window \n");
  fprintf(js_file_descriptor, "var circle_size =  2; // Bubble size          \n");
  fprintf(js_file_descriptor, "var circle_op = 0.4;  // Bubble transparent   \n");
  fprintf(js_file_descriptor, "var line_op = 1.0;    // Line transparent     \n");
  fprintf(js_file_descriptor, "var axis_tick = 40;   // Tick of Y axis       \n");
  fprintf(js_file_descriptor, "                                              \n");
  fprintf(js_file_descriptor, "var curveArray = [                            \n");


  if(!config_lookup_string(&cfg, "csv_dots", &Settings.csv_dots))
    Settings.csv_dots = ".";
  if(!config_lookup_string(&cfg, "csv_delimeter", &Settings.csv_delimeter))
    Settings.csv_delimeter = ",";
  if(!config_lookup_int(&cfg, "screen_refresh_div", &Settings.screen_timeout))
    Settings.screen_timeout = 1;
  if(!config_lookup_int(&cfg, "syncfs", &Settings.syncfs))
    Settings.syncfs = 0;
  if(!config_lookup_int(&cfg, "display_state", &Settings.display_state))
    Settings.display_state = 1;
  if(!config_lookup_int(&cfg, "LXI_connect_timeout", &Settings.lxi_connect_timeout))
    Settings.lxi_connect_timeout = 1000;

  fprintf(csv_file_descriptor, "sample%sdate%stime%s", Settings.csv_delimeter, Settings.csv_delimeter, Settings.csv_delimeter);


  // Load tmp117 config -----------------------------------------
  setting_temp = config_lookup(&cfg, "inventory.tmp117_temperature");
  channel_count_temp = config_setting_length(setting_temp);


  if(setting_temp != NULL)
  {
    const char *device_temp_name;

    for (i = 0; i < channel_count_temp; ++i)
    {
      config_setting_t *channels_temp = config_setting_get_elem(setting_temp, i);

      /* Выводим только те записи, если они имеют все нужные поля. */
      if(!(config_setting_lookup_int(channels_temp, "address", &temperature_sensors[i].i2c_address)
           && config_setting_lookup_int(channels_temp, "config", &temperature_sensors[i].config_word)
           && config_setting_lookup_float(channels_temp, "delay", &temperature_sensors[i].delay) && config_setting_lookup_string(channels_temp, "device_name", &device_temp_name)));
//        continue;

      if(temperature_sensors[i].i2c_address > 0)
      {
        tspan_count++;
        if(tspan_count > 1)
        {
          fprintf(js_file_descriptor, "    {\"curveTitle\":\"%s\",		\"channel\":\"ch%i\",	\"offset\":0,		\"scale\":100,	\"group\":0,	\"tspan\":0,	\"axis_is_ppm\":0}, \n", device_temp_name, i + 17);
        } else
          fprintf(js_file_descriptor, "    {\"curveTitle\":\"%s\",		\"channel\":\"ch%i\",	\"offset\":0,		\"scale\":100,	\"group\":0,	\"tspan\":1,	\"axis_is_ppm\":0}, \n", device_temp_name, i + 17);
      }


    }
    for (i = 0; i < channel_count_temp; ++i)
      if(temperature_sensors[i].i2c_address > 0)
      {
        configure_tmp117(temperature_sensors[i].i2c_address, temperature_sensors[i].config_word);
        fprintf(csv_file_descriptor, "temp%i%s", i + 1, Settings.csv_delimeter);
      }

  }

  setting = config_lookup(&cfg, "inventory.channels");
  channel_count = config_setting_length(setting);


  // Read settings -----------------------------------------------
  if(setting != NULL)
  {
    for (i = 0; i < channel_count; ++i)
    {
      config_setting_t *channels = config_setting_get_elem(setting, i);

      /* Выводим только те записи, если они имеют все нужные поля. */
      if(!(config_setting_lookup_string(channels, "device_name", &Channels.Device_name[i])
           && config_setting_lookup_string(channels, "IP", &Channels.IP[i])
           && config_setting_lookup_int(channels, "Protocol", &Channels.Protocol[i])
           && config_setting_lookup_string(channels, "Instance", &Channels.Instance[i])
           && config_setting_lookup_int(channels, "Port", &Channels.Port[i]) && config_setting_lookup_int(channels, "Timeout", &Channels.Timeout[i]) && config_setting_lookup_string(channels, "Read_command", &Channels.Read_command[i])))
        continue;

      if(!config_setting_lookup_string(channels, "Exit_command", &Channels.Exit_command[i]))
        Channels.Exit_command[i] = "";
      if(!config_setting_lookup_string(channels, "Display_on_command", &Channels.Display_on_command[i]))
        Channels.Display_on_command[i] = "";
      if(!config_setting_lookup_string(channels, "Display_off_command", &Channels.Display_off_command[i]))
        Channels.Display_off_command[i] = "";

      fprintf(csv_file_descriptor, "val%i%s", i + 1, Settings.csv_delimeter);
      fprintf(js_file_descriptor, "    {\"curveTitle\":\"%s\",		\"channel\":\"ch%i\",	\"offset\":0,		\"scale\":1,	\"group\":0,	\"tspan\":0,	\"axis_is_ppm\":0}, \n", Channels.Device_name[i], i + 1);

      // LXI Connect and init devices
      if(Channels.Protocol[i] == 1)
      {
        Channels.device[i] = lxi_connect(Channels.IP[i], Channels.Port[i], Channels.Instance[i], Settings.lxi_connect_timeout, VXI11);  // Try connect to LXI
      } else
      {
        Channels.device[i] = lxi_connect(Channels.IP[i], Channels.Port[i], Channels.Instance[i], Settings.lxi_connect_timeout, RAW);    // Try connect to LXI
      }

      if(!(Channels.device[i] < 0))
      {
        init_commands = config_setting_get_member(channels, "Init_string");
        k = 0;
        if(init_commands)
        {
          while (1)
          {
            if(config_setting_get_elem(init_commands, k) == NULL)
            {
              Channels.Init_commands[i][k] = "";
              break;
            }
            Channels.Init_commands[i][k] = config_setting_get_string_elem(init_commands, k);
            k++;
          }
        }
      }
    }
  }

}

// ---------------------------------------------------------------------------------------------------
//
//
//
// ---------------------------------------------------------------------------------------------------
void draw_info_win()
{
  int i;

  wmove(log_win, 0, 0);
  wmove(legend_win, 0, 0);
  wmove(help_win, 0, 0);

  wprintw(help_win, "  SPACE - pause, q - quit, r - refresh window, d - display ON/OFF  ");

  wprintw(legend_win, "Sample     Time     ");

  for (i = 0; i < channel_count_temp; ++i)
  {
    if(temperature_sensors[i].i2c_address > 0)
    {
      wprintw(legend_win, "Temp%i    ", i);
    }
  }

  for (i = 0; i < channel_count; ++i)
  {
    wprintw(legend_win, "Channel%i            ", i);

  }

  wrefresh(log_win);
  wrefresh(legend_win);
  wrefresh(channels_win);
  wrefresh(help_win);

  wmove(log_win, 0, 0);
  // -------------------------------------------------------------


  // Read settings -----------------------------------------------
  if(setting != NULL)
  {

    wmove(channels_win, 1, 1);
    wprintw(channels_win, "%-7s %-20s %-15s %-15s", "Channel", "Device", "IP", "Data");

    for (i = 0; i < channel_count; ++i)
    {
      wmove(channels_win, i + 2, 1);
      wprintw(channels_win, "%-7i %-20s %-15s", i, Channels.Device_name[i], Channels.IP[i]);
      wrefresh(channels_win);

      if(Channels.device[i] < 0)
      {
        wmove(channels_win, i + 2, 46);
        wprintw(channels_win, "Connection failed!");
        wrefresh(channels_win);
        wprintw(log_win, "Can't connect to %s\n", Channels.Device_name[i]);
      }
    }
  }
}

// ---------------------------------------------------------------------------------------------------
//
//
//
// ---------------------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
  int i, k;
  int status_addr, status;
  struct timespec start, stop;
  double accum;
  char time_in_char[32], temp_in_char[32];
  int time_in_char_pos = 0, temp_in_char_pos = 0;

  lxi_init();                   // Initialize LXI library
  init_config();

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


  channels_win = newwin(channel_count + 3, 65, 0, 0);
  wattron(channels_win, COLOR_PAIR(2));
  box(channels_win, 0, 0);

  log_win = newwin(term_y - (channel_count + 3) - 1 - 1, term_x, channel_count + 3 + 1, 0);
  scrollok(log_win, TRUE);

  legend_win = newwin(term_y - (channel_count + 3) - 1 - 1, term_x, channel_count + 3, 0);
  scrollok(legend_win, TRUE);

  help_win = newwin(1, term_x, term_y - 1, 0);
  wattron(help_win, COLOR_PAIR(1));


  wmove(log_win, 0, 0);

  draw_info_win();

  for (i = 0; i < channel_count; ++i)
  {
    if(!(Channels.device[i] < 0))
    {                           // Send init commands to instruments
      k = 0;
      while (strlen(Channels.Init_commands[i][k]) > 0)
      {

        send_command_to_instrument(i, Channels.Init_commands[i][k]);
        wprintw(log_win, "%s send init: %s\n", Channels.Device_name[i], Channels.Init_commands[i][k]);
        wrefresh(log_win);
        k++;
      }
    }
  }

  fprintf(csv_file_descriptor, "\n");
  fprintf(js_file_descriptor, "  ];                                          \n");
  fclose(js_file_descriptor);

// Send display ON/OFF commands to instruments
  for (i = 0; i < channel_count; ++i)
    if(Channels.device[i] >= 0)
    {
      if(Settings.display_state == 0)
      {
        send_command_to_instrument(i, Channels.Display_off_command[i]);
        wprintw(log_win, "%s send init: %s\n", Channels.Device_name[i], Channels.Display_off_command[i]);
      } else
      {
        send_command_to_instrument(i, Channels.Display_on_command[i]);
        wprintw(log_win, "%s send init: %s\n", Channels.Device_name[i], Channels.Display_on_command[i]);
      }
    }
  wrefresh(log_win);

// -------------------------------------------------------------


// Start time measurement
  clock_gettime(CLOCK_REALTIME, &start);


// Read data ---------------------------------------------------
  int screen_timeout_count = 0;
  while (exit_code == 0)
  {


    switch (getch())
    {
    case 'q':

      for (i = 0; i < channel_count; ++i)
        if(Channels.device[i] >= 0)
        {
          send_command_to_instrument(i, Channels.Exit_command[i]);
        }

      exit_code++;
      break;

    case 'd':

      if(Settings.display_state == 0)
      {
        Settings.display_state = 1;
      } else
      {
        Settings.display_state = 0;
      }

      wprintw(log_win, "\n");

      for (i = 0; i < channel_count; ++i)
        if(Channels.device[i] >= 0)
        {
          // Send display ON/OFF commands to instruments
          if(Settings.display_state == 0)
          {
            send_command_to_instrument(i, Channels.Display_off_command[i]);
            wprintw(log_win, "%s send init: %s\n", Channels.Device_name[i], Channels.Display_off_command[i]);
          } else
          {
            send_command_to_instrument(i, Channels.Display_on_command[i]);
            wprintw(log_win, "%s send init: %s\n", Channels.Device_name[i], Channels.Display_on_command[i]);
          }
        }
      wrefresh(log_win);

      break;


    case ' ':
      while (getch() != ' ')
        sleep(1);
      break;

    case 'r':
      getmaxyx(stdscr, term_y, term_x);
      wresize(log_win, term_y - (channel_count + 3) - 1 - 1, term_x);
      wresize(legend_win, 1, term_x);
      wresize(channels_win, channel_count + 3, 65);
      mvwin(help_win, term_y - 1, 0);
      wresize(help_win, 1, term_x);

      draw_info_win();

      box(channels_win, 0, 0);
      wrefresh(help_win);
      wrefresh(log_win);
      wrefresh(legend_win);
      wrefresh(channels_win);
      break;
    }

    if(exit_code != 0)
      break;

    sample_num++;

    for (i = 0; i < channel_count; ++i) // Start threads
    {
      if(Channels.device[i] >= 0)
      {
        pthread_create(&(Channels.tid[i]), NULL, measurement_thread, (void *) ((intptr_t) i));  // запуск LXI измерения
      } else
      {
        response_massive[i][0] = '0';
        response_massive[i][1] = '\0';
      }
    }

    for (i = 0; i < channel_count; ++i) // Wait threads complete
    {
      if(Channels.device[i] >= 0)
      {
        status = pthread_join(Channels.tid[i], (void **) &status_addr);
        if(status != SUCCESS)
        {
          wprintw(log_win, "main error: can't join thread, status = %d\n", status);
          exit(ERROR_JOIN_THREAD);
        }

      }
    }

    // Calculate time
    clock_gettime(CLOCK_REALTIME, &stop);       // Fix clock
    accum = (stop.tv_sec - start.tv_sec) + (stop.tv_nsec - start.tv_nsec) / 1E9;

    for (i = 0; i < channel_count_temp; ++i)
    {
      if(temperature_sensors[i].i2c_address > 0)
        if(((accum - temperature_sensors[i].tmp117_last_read_time) > temperature_sensors[i].delay) || temperature_sensors[i].tmp117_last_read_time == 0)
        {
          temperature_sensors[i].tmp117_last_read_time = accum;
          read_temp(i, temperature_sensors[i].i2c_address);     // Read TMP117
        }
    }

    // ---------------------------------------------------------

    wprintw(log_win, "\n");

    // Draw log table and save CSV

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(time_buffer, 80, "%d/%m/%Y-%H:%M:%S%z", timeinfo);


    wprintw(log_win, "%-5u   ", sample_num);
    fprintf(csv_file_descriptor, "%llu%s%s%s", sample_num, Settings.csv_delimeter, time_buffer, Settings.csv_delimeter);

    sprintf(time_in_char, "%.4f", accum);

    time_in_char_pos = 0;
    while (strchr(",", time_in_char[time_in_char_pos]) == NULL)
      time_in_char_pos++;
    time_in_char[time_in_char_pos] = Settings.csv_dots[0];

    wprintw(log_win, "%9.4f  ", accum);
    fprintf(csv_file_descriptor, "%s%s", time_in_char, Settings.csv_delimeter);


    // Draw temperature
    for (i = 0; i < channel_count_temp; ++i)
    {
      if(temperature_sensors[i].i2c_address > 0)
      {
        sprintf(temp_in_char, "%.3f", temperature_sensors[i].temperature);

        temp_in_char_pos = 0;
        while (strchr(",", temp_in_char[temp_in_char_pos]) == NULL)
          temp_in_char_pos++;
        temp_in_char[temp_in_char_pos] = Settings.csv_dots[0];

        wprintw(log_win, "%7.3lf  ", temperature_sensors[i].temperature);
        fprintf(csv_file_descriptor, "%s%s", temp_in_char, Settings.csv_delimeter);
      }
    }

    // Draw measure
    for (i = 0; i < channel_count; ++i)
    {
      if(Channels.device[i] >= 0)
        mvwprintw(channels_win, i + 2, 46, "%-15s", response_massive[i]);       // Print response
      wprintw(log_win, "%-20s", response_massive[i]);
      fprintf(csv_file_descriptor, "%s%s", response_massive[i], Settings.csv_delimeter);

    }

    // finish
    fprintf(csv_file_descriptor, "\n");
    if(Settings.syncfs == 1)
    {
      fflush(csv_file_descriptor);
      syncfs(fileno(csv_file_descriptor));
    }

    if(screen_timeout_count >= Settings.screen_timeout)
    {
      wrefresh(channels_win);
      wrefresh(log_win);
      screen_timeout_count = 0;
    } else
      screen_timeout_count++;
    // ---------------------------------------------------------


  }


////////////////////////////////////////////////////////////////////////////////
  for (i = 0; i < channel_count; ++i)   // закрытие потоков и инструментов
  {
    if(Channels.device[i] >= 0)
    {
      printw("Close: i=%i device=%i tid=%i\n", i, Channels.device[i], Channels.tid[i]);
      status = pthread_join(Channels.tid[i], NULL);
      printw("Closed!\n");
      lxi_disconnect(Channels.device[i]);
    }
  }

  fclose(csv_file_descriptor);
  pthread_exit(NULL);
  delwin(channels_win);
  delwin(log_win);
  delwin(help_win);
  endwin();
  config_destroy(&cfg);
  return (EXIT_SUCCESS);
}
