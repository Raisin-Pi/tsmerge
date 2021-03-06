
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <errno.h>
#include <inttypes.h>
#include "merger.h"

/* This function is run on a thread, started from main()
 *
 * This function loops over the 'viewers' array,
 *  checking for when 'last_station' and 'last_count' do not match the latest selected segment
 * On this condition, the latest segment is sent to the relevant viewer,
 *  and their 'last_station' and 'last_count' data is updated.
 */
void *merger_file_feed(void* arg)
{
  (void) arg;
  mx_packet_t *p;
  uint64_t timestamp;
  struct stat st = {0};
  char csvText[128];
  uint8_t csvTextSize;
  FILE *f;
  int r;
  

  /* The main network loop */
  while(1)
  {	
    if(file_viewer.tsenabled || file_viewer.csvenabled)
    {
      p = NULL;
		    
      if(file_viewer.tsenabled || file_viewer.csvenabled)
      {
        if(stat("logs/", &st) == -1)
        {
          printf("Creating file log directory.\n");
          mkdir("logs/", 0777);
        }
      }

      /* See if there is any data still to send to this viewer */
      pthread_mutex_lock(&merger.lock);
      while((p = mx_next(&merger, file_viewer.last_station, file_viewer.last_counter)) != NULL)
      {
        timestamp = timestamp_ms();

        if(file_viewer.tsenabled)
        {
          /* Append TS segment to TS file */
          f = fopen(file_viewer.tsfilename, "a+");
          if(f == NULL)
          {
            fprintf(stderr,"Error opening ts output file: %s\n", file_viewer.tsfilename);
            file_viewer.tsenabled = 0;
            break;
          }
          /* Try to send the new data to the viewer */
          r = fwrite(p->raw, TS_PACKET_SIZE, 1, f);
          if(r <= 0)
          {
            /* An error has occured. */
            fprintf(stderr,"Error saving ts output to file: %d\n", r);
            /* Disable file output */
            file_viewer.tsenabled = 0;
            break;
          }
          fclose(f);
        }

        if(file_viewer.csvenabled)
        {
          /* Append CSV segment description to CSV file */
          f = fopen(file_viewer.csvfilename, "a+");
          if(f == NULL)
          {
            fprintf(stderr,"Error opening csv output file: %s\n", file_viewer.csvfilename);
            file_viewer.csvenabled = 0;
            break;
          }
          /* Create text [timestamp_ms, station id, station name, station packet counter, packet pcr base, packet pcr extension, packet pid, packet continuity counter] */
          csvTextSize = snprintf(csvText, 127, "%"PRIu64",%"PRIu32",%s,%"PRIu32",%"PRIu64",%"PRIu16",%"PRIu16",%"PRIu8"\n",
            timestamp,
            p->station,
            merger.station[p->station].sid,
            p->counter,
            p->header.pcr_base,
            p->header.pcr_extension,
            p->header.pid,
            p->header.continuity_counter
          );
          /* Try to send the new data to the viewer */
          r = fwrite(csvText, csvTextSize, 1, f);
          if(r <= 0)
          {
            /* An error has occured. */
            fprintf(stderr,"Error saving csv output to file: %d\n", r);
            /* Disable file output */
            file_viewer.csvenabled = 0;
            break;
          }
          fclose(f);
        }

        /* Update viewer state */
        file_viewer.last_station = p->station;
        file_viewer.last_counter = p->counter;
        file_viewer.timestamp = timestamp;
      }
      pthread_mutex_unlock(&merger.lock);
    }		
    sleep_ms(5);
  }
}
