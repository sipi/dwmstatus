/** ********************************************************************
 * DWM STATUS by <clement@6pi.fr>
 * 
 * This program is a patchwork of functions, I haven't all rights on 
 * this code. Thank you to the various authors.
 * 
 * Compile with:
 * gcc -Wall -pedantic -std=c99 -lX11 -lasound dwmstatus.c
 * 
 **/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <wchar.h>
#include <unistd.h>
#include <time.h>
#include <X11/Xlib.h>

/* Alsa */
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
/* Oss (not working, using popen + ossmix)
#include <linux/soundcard.h>
*/

//**********************************************************************
// UTILS
//**********************************************************************

void  freeTable2d(int **tableau);
char* getBar(int percent);
int** makeTable2d(int first_dim_len, int second_dim_len);

void 
freeTable2d(int **tableau)
{
  free(tableau[0]);
  free(tableau);
}

char *
getBar(int percent)
{
  char *bar = (char*) malloc(8*sizeof(char));
  int tmp = percent/20;
  int i;
  
  bar[0] = '[';
  for(i = 1; i <= tmp; ++i)
    bar[i] = '|';
  
  for(; i <= 5; ++i)
    bar[i] = ' ';

  bar[6] = ']';
  bar[7] = '\0';

  return bar;
}

int**
makeTable2d(int first_dim_len, int second_dim_len)
{
  int **tab = (int**) malloc(first_dim_len*sizeof(int*));
  int *main_mem = (int*)malloc(first_dim_len*second_dim_len*sizeof(int));
  for(int i = 0; i < first_dim_len; ++i)
    tab[i] = main_mem + i*second_dim_len;
  
  return tab;
}

//**********************************************************************
// MAIN FUNCTIONS
//**********************************************************************

int   getBattery();
void  getCpuUsage(int *cpu_percent, const int cpu_nbr);
char* getDatetime();
int   getEnergyStatus();
float getFreq(char *file);
int   getTemperature();
int   getVolume();
void  setStatus(Display *dpy, char *str);


int 
getBattery()
{
  FILE *fd;
  int energy_now;

  static int energy_full = -1;
  if(energy_full == -1)
    {
      fd = fopen("/sys/class/power_supply/BAT0/energy_full", "r");
      if(fd == NULL) {
        fprintf(stderr, "Error opening energy_full.\n");
        return -1;
      }
      fscanf(fd, "%d", &energy_full);
      fclose(fd);
    }
  
  fd = fopen("/sys/class/power_supply/BAT0/energy_now", "r");
  if(fd == NULL) {
    fprintf(stderr, "Error opening energy_now.\n");
    return -1;
  }
  fscanf(fd, "%d", &energy_now);
  fclose(fd);
  
  return ((float)energy_now  / (float)energy_full) * 100;
}

void
getCpuUsage(int* cpu_percent, const int cpu_nbr)
{
  size_t len = 0;
  char *line = NULL;
  int i, percent;
  long int idle_time, other_time;
  char cpu_name[cpu_nbr]; 
  
  static int **new_cpu_usage = NULL;
  if(new_cpu_usage == NULL)
    new_cpu_usage = makeTable2d(cpu_nbr, 4);

  static int **old_cpu_usage = NULL;
  if(old_cpu_usage == NULL)
    {
      old_cpu_usage = makeTable2d(cpu_nbr, 4);
      for(i = 0; i < cpu_nbr; ++i)
        for(int j = 0; j < 4; ++j)
          old_cpu_usage[i][j] = 0;
    }

  FILE *f;
  if(NULL == (f = fopen("/proc/stat", "r")))
    return;
  
  for(i = 0; i < cpu_nbr; ++i)
    {
      getline(&line,&len,f);
      sscanf(
             line, 
             "%s %d %d %d %d",
             cpu_name,
             &new_cpu_usage[i][0],
             &new_cpu_usage[i][1],
             &new_cpu_usage[i][2],
             &new_cpu_usage[i][3]
             );
      
      if(line != NULL)
        {
          free(line); 
          line = NULL;
        }

      idle_time = new_cpu_usage[i][3] - old_cpu_usage[i][3];
      other_time = new_cpu_usage[i][0] - old_cpu_usage[i][0]
        + new_cpu_usage[i][1] - old_cpu_usage[i][1]
        + new_cpu_usage[i][2] - old_cpu_usage[i][2];
      
      if(idle_time + other_time != 0)
        cpu_percent[i] = (other_time*100)/(idle_time + other_time);
      else
        cpu_percent[i] = 0; 

      old_cpu_usage[i][0] = new_cpu_usage[i][0];
      old_cpu_usage[i][1] = new_cpu_usage[i][1];
      old_cpu_usage[i][2] = new_cpu_usage[i][2];
      old_cpu_usage[i][3] = new_cpu_usage[i][3];
    }
    
    fclose(f);
}

char *
getDateTime() 
{
  char *buf;
  time_t result;
  struct tm *resulttm;
  
  if((buf = malloc(sizeof(char)*65)) == NULL)
    {
      fprintf(stderr, "Cannot allocate memory for buf.\n");
      exit(1);
    }

  result = time(NULL);
  resulttm = localtime(&result);
  if(resulttm == NULL)
    {
      fprintf(stderr, "Error getting localtime.\n");
      exit(1);
    }
  
  if(!strftime(buf, sizeof(char)*65-1, "%a %b %d %H:%M", resulttm))
    {
      fprintf(stderr, "strftime is 0.\n");
      exit(1);
    }
	
  return buf;
}

/** 
 * Return 0 if the battery is discharing, 1 if it's full or is 
 * charging, -1 if an error occured. 
 **/
int 
getEnergyStatus()
{
  FILE *fd;
  char first_letter;
  char file_path[] = "/sys/class/power_supply/BAT0/status";
  
  if( NULL == (fd = fopen(file_path, "r")))
    return -1;

  fread(&first_letter, sizeof(char), 1, fd);
  fclose(fd);

  if(first_letter == 'D')
    return 0;

  return 1;
}

float 
getFreq(char *file)
{
  FILE *fd;
  char *freq; 
  float ret;
  
  freq = malloc(10);
  fd = fopen(file, "r");
  if(fd == NULL)
    {
      fprintf(stderr, "Cannot open '%s' for reading.\n", file);
      exit(1);
    }

  fgets(freq, 10, fd);
  fclose(fd);

  ret = atof(freq)/1000000;
  free(freq);
  return ret;
}

int
getTemperature()
{
  int temp;
  FILE *fd = fopen("/sys/class/hwmon/hwmon1/device/temp1_input", "r");
  if(fd == NULL) 
    {
      fprintf(stderr, "Error opening temp1_input.\n");
      return -1;
    }
  fscanf(fd, "%d", &temp);
  fclose(fd);
  
  return temp / 1000;
}

int
getVolume()
{
  const char* MIXER = "Master";
  /* OSS
  const char* OSSMIXCMD = "ossmix vmix0-outvol";
  const char* OSSMIXFORMAT = "Value of mixer control vmix0-outvol is currently set to %f (dB)";
  */

  float vol = 0;
  long pmin, pmax, pvol;
  FILE *f = NULL;

  /* Alsa {{{ */
  snd_mixer_t *handle;
  snd_mixer_selem_id_t *sid;
  snd_mixer_elem_t *elem;
  snd_mixer_selem_id_alloca(&sid);

  if(snd_mixer_open(&handle, 0) < 0)
    return 0;

  if(snd_mixer_attach(handle, "default") < 0
     || snd_mixer_selem_register(handle, NULL, NULL) < 0
     || snd_mixer_load(handle) > 0)
    {
      snd_mixer_close(handle);
      return 0;
    }

  for(elem = snd_mixer_first_elem(handle); elem; elem = snd_mixer_elem_next(elem))
    {
      snd_mixer_selem_get_id(elem, sid); 
      if(!strcmp(snd_mixer_selem_id_get_name(sid), MIXER))
        {
          snd_mixer_selem_get_playback_volume_range(elem, &pmin, &pmax);
          snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &pvol);
          vol = ((float)pvol / (float)(pmax - pmin)) * 100;
        }
    }

  snd_mixer_close(handle);
  /* }}} */
  /* Oss (soundcard.h not working) {{{ 
     if(!(f = popen(OSSMIXCMD, "r")))
       return;

     fscanf(f, OSSMIXFORMAT, &vol);
     pclose(f);
     }}} */

  return vol;
}

void 
setStatus(Display *dpy, char *str) 
{
  XStoreName(dpy, DefaultRootWindow(dpy), str);
  XSync(dpy, False);
}

//**********************************************************************
// MAIN
//**********************************************************************

int 
main(void) 
{
  Display *dpy;
  char *status;
  
  float cpu_freq[4];
  int cpu_percent[4];
  char *datetime;
  int bat0, temp, vol; 
  
  const char CELSIUS_CHAR = 176;

  /* ansi color
   * Work with ansistatuscolors patch */
  char *reset = "\e[0m";
  char *redfg = "\e[38;5;196m";
  char *greenfg = "\e[38;5;82m";
  
  if (!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "Cannot open display.\n");
    return EXIT_FAILURE;
  }

  if((status = malloc(256)) == NULL)
    return EXIT_FAILURE;

   while(1)
    {
      cpu_freq[0] = getFreq("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
      cpu_freq[1] = getFreq("/sys/devices/system/cpu/cpu1/cpufreq/scaling_cur_freq");
      cpu_freq[2] = getFreq("/sys/devices/system/cpu/cpu2/cpufreq/scaling_cur_freq");
      cpu_freq[3] = getFreq("/sys/devices/system/cpu/cpu3/cpufreq/scaling_cur_freq");
      
      temp = getTemperature();
      datetime = getDateTime();
      bat0 = getBattery();
      vol = getVolume();
      getCpuUsage(cpu_percent, 4);
      
      snprintf(
               status, 
               256, 
               " [VOL %d%%] [CPU %d/%1.1f %d/%1.1f %d/%1.1f %d/%1.1f] [TEMP %d%cC] [BAT %d%%] %s ", 
               vol, 
               cpu_percent[0], cpu_freq[0], 
               cpu_percent[1], cpu_freq[1], 
               cpu_percent[2], cpu_freq[2], 
               cpu_percent[3], cpu_freq[3], 
               temp, CELSIUS_CHAR, 
               bat0, datetime
               );
      
      free(datetime);
      setStatus(dpy, status);
      sleep(3);
    }
   
  /* USELESS
  free(status);
  XCloseDisplay(dpy);
  
  return EXIT_SUCESS;
  */
}

