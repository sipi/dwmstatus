/** ********************************************************************
 * DWM STATUS by <clement@6pi.fr>
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

#define CPU_NBR 4
#define BAT_NOW_FILE "/sys/class/power_supply/BAT0/charge_now"
#define BAT_FULL_FILE "/sys/class/power_supply/BAT0/charge_full"

#define TEMP_SENSOR_FILE "/sys/class/hwmon/hwmon1/temp1_input"

int   getBattery();
void  getCpuUsage(int *cpu_percent);
char* getDateTime();
int   getEnergyStatus();
float getFreq(char *file);
int   getTemperature();
int   getVolume();
void  setStatus(Display *dpy, char *str);
int   getWifiPercent();


/* *******************************************************************
 * MAIN
 ******************************************************************* */

int 
main(void) 
{
  Display *dpy;
  char *status;
  
  int cpu_percent[CPU_NBR];
  char *datetime;
  int bat0, temp, vol, wifi;
  
  const char CELSIUS_CHAR = (char)176;
  
  if (!(dpy = XOpenDisplay(NULL))) {
    fprintf(stderr, "Cannot open display.\n");
    return EXIT_FAILURE;
  }

  if((status = malloc(256)) == NULL)
    return EXIT_FAILURE;

   while(1)
    {
      
      temp = getTemperature();
      datetime = getDateTime();
      bat0 = getBattery();
      vol = getVolume();
      getCpuUsage(cpu_percent);
      wifi = getWifiPercent();
      
      snprintf(
               status, 
               256, 
               " [VOL %d%%] [CPU %d %d %d %d] [W %d] [^c#FF0000^TEMP %d%cC^c#FFFFFF^] [BAT %d%%] %s ", 
               vol, 
               cpu_percent[0], 
               cpu_percent[1],  
               cpu_percent[2],  
               cpu_percent[3],  
               wifi,
               temp, CELSIUS_CHAR, 
               bat0, datetime
               );
      
      free(datetime);
      setStatus(dpy, status);
      sleep(1);
    }
   
  /* USELESS
  free(status);
  XCloseDisplay(dpy);
  
  return EXIT_SUCESS;
  */
}

/* *******************************************************************
 * FUNCTIONS
 ******************************************************************* */

void 
setStatus(Display *dpy, char *str) 
{
  XStoreName(dpy, DefaultRootWindow(dpy), str);
  XSync(dpy, False);
}

int 
getBattery()
{
  FILE *fd;
  int energy_now;

  static int energy_full = -1;
  if(energy_full == -1)
    {
      fd = fopen(BAT_FULL_FILE, "r");
      if(fd == NULL) {
        fprintf(stderr, "Error opening energy_full.\n");
        return -1;
      }
      fscanf(fd, "%d", &energy_full);
      fclose(fd);
    }
  
  fd = fopen(BAT_NOW_FILE, "r");
  if(fd == NULL) {
    fprintf(stderr, "Error opening energy_now.\n");
    return -1;
  }
  fscanf(fd, "%d", &energy_now);
  fclose(fd);
  
  return ((float)energy_now  / (float)energy_full) * 100;
}

void
getCpuUsage(int* cpu_percent)
{
  size_t len = 0;
  char *line = NULL;
  int i;
  long int idle_time, other_time;
  char cpu_name[8]; 
  
  static int new_cpu_usage[CPU_NBR][4];
  static int old_cpu_usage[CPU_NBR][4];

  FILE *f;
  if(NULL == (f = fopen("/proc/stat", "r")))
    return;
  
  for(i = 0; i < CPU_NBR; ++i)
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

int
getTemperature()
{
  int temp;
  FILE *fd = fopen(TEMP_SENSOR_FILE, "r");
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
getWifiPercent()
{
	size_t len = 0;
	int percent = 0;
	char line[512] = {'\n'};
	FILE *fd = fopen("/proc/net/wireless", "r");
	if(fd == NULL)
		{
			fprintf(stderr, "Error opening wireless info");
			return -1;
		}
	fscanf(fd, "%*[^\n]\n%*[^\n]\n%*s %*[0-9] %d", &percent);
	fclose(fd);
	return percent;
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

