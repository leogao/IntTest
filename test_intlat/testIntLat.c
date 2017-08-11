/******************************************************************************
* @file  cpri_api.c 
*
* INTEL CONFIDENTIAL
* Copyright 2010 Intel Corporation All Rights Reserved.
* 
* The source code contained or described herein and all documents related to the
* source code ("Material") are owned by Intel Corporation or its suppliers or
* licensors. Title to the Material remains with Intel Corporation or its
* suppliers and licensors. The Material may contain trade secrets and proprietary
* and confidential information of Intel Corporation and its suppliers and
* licensors, and is protected by worldwide copyright and trade secret laws and
* treaty provisions. No part of the Material may be used, copied, reproduced,
* modified, published, uploaded, posted, transmitted, distributed, or disclosed
* in any way without Intels prior express written permission.
* No license under any patent, copyright, trade secret or other intellectual
* property right is granted to or conferred upon you by disclosure or delivery
* of the Materials, either expressly, by implication, inducement, estoppel or
* otherwise. Any license under such intellectual property rights must be
* express and approved by Intel in writing.
* 
* Include any supplier copyright notices as supplier requires Intel to use.
* Include supplier trademarks or logos as supplier requires Intel to use,
* preceded by an asterisk.
* An asterisked footnote can be added as follows: 
*   *Third Party trademarks are the property of their respective owners.
* 
* Unless otherwise agreed by Intel in writing, you may not remove or alter this
* notice or any other notice embedded in Materials by Intel or Intels suppliers
* or licensors in any way.
* 
*  version: FPGA_InteLa.L.0.1.1
******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h> 
#include <string.h>
#include "latint_ioctl.h"
#include "latint_api.h"

// #include "../lib/include/cpri_regs.h"

#ifdef XENOMAI_PATCH
#include <native/task.h>
#include <native/queue.h>
#include <native/intr.h>
#include <native/timer.h>


#include <rtdk.h> 
#endif

#undef PRINT
#define PRINT(fmt, args...)   printf("DEBUG: %s:%d " fmt "\n", __FUNCTION__,__LINE__, ## args)

#define WORDLENGTH uint32_t

//static pciext_device_t* latint = NULL;
static LatintHandle handle[2];
static uint64_t cpu_frequency=0;

#ifdef EXTRADEBUG
static void dumpRegisters(unsigned int _index);
static void pciBridgeBarDump(UINT baseAddr);
#endif

// global variables
static int usFPGA = 125E6 * 1E-6;

static int tracing_on = 0;

// function definitions
static inline void sleepVariable(int64_t microsecondsToSleep)
{
	if (microsecondsToSleep < 25)
		microsecondsToSleep=500;
#ifdef XENOMAI_PATCH	
	rt_task_sleep(rt_timer_ns2ticks(microsecondsToSleep*1000));
#else
	sleep(1);
#endif
}

void DisplayTimerStats(timer_info_t *timer_info)
{
//	int usFPGA = 125E6 * 1E-6;
    printf("Period is %d us \n",timer_info->period / usFPGA);
    printf("Latency is Min: %0.2fus Max: %0.2fus Avg: %0.2fus\n",
			(float)timer_info->latency_min / usFPGA,
			(float)timer_info->latency_max / usFPGA,
			(float)timer_info->latency_avg / usFPGA / timer_info->no_interrupts);
    printf("Jitter is Min: %0.2fus Max: %0.2fus Avg: %0.2fus\n",
			(float)timer_info->jitter_min / cpu_frequency,
			(float)timer_info->jitter_max / cpu_frequency,
			(float)timer_info->jitter_avg / cpu_frequency / (timer_info->no_interrupts-1));
    printf("No of interrupts = %u \n",(unsigned int)timer_info->no_interrupts);
}

void CalculateIndices(int * indices, int num_irq, int buff_size){
    /* indices is an array of ints of size=buff_size
     * buff_size is the length of the buffer 
     * num_irq is the number of interrupts measured */
    
    int index=0, count=0;
    int oldestIndex, newestIndex;
    newestIndex = (num_irq - 1) % buff_size;
    if(num_irq > buff_size)
    {
        oldestIndex = newestIndex + 1;
        for(index = oldestIndex; index < buff_size; ++index, ++count){
            indices[count] = index;
        }
    }
    else
    {
        oldestIndex = 0;
    }
    for(index = 0; index <= newestIndex; ++index, ++count){
        indices[count] = index;
    }
#ifdef DEBUG
    for(index = 0; index < buff_size; ++index){
        printf("Index: %d, value: %d\n", index, indices[index]);
    }
#endif
}

void PrintIndices(int maxIndex, int oldestIndex, int newestIndex){
        printf("maxIndex = %d\n", maxIndex);
        printf("oldestIndex = %d\n", oldestIndex);
        printf("newestIndex = %d\n", newestIndex);
}

void DumpLatencyData(timer_info_t *timer_info){
    /*This function takes the array of latency data inside the timer_info 
     * and writes it to a text file in the order it was collected.  
     * If the buffer has been overfilled (wrapped around), then it should
     * start writing from the middle of the array (oldestIndex), through
     * to the end of the array, then wrap around and continue from the
     * beginning of the array */
    
    /* open a file for output */
    char filename[]  = "latency_data.txt";
    FILE * output_fp = fopen(filename, "w");
    int index;
    int maxIndex, oldestIndex, newestIndex;
    int buff_size = LATENCY_BUFFER_SIZE;
    long long unsigned int num_interrupts = timer_info->no_interrupts;
    double latency;
    int count = 1;
    int *indices;
    // print meta-data about collected information 
    printf("Interrupts measured = %llu\n", num_interrupts);
    indices = (int*) malloc(sizeof(int) * buff_size);
    CalculateIndices(indices, num_interrupts, buff_size);
    
    newestIndex = (num_interrupts - 1) % buff_size;
    if(num_interrupts > buff_size){
        maxIndex = buff_size - 1;
        oldestIndex = newestIndex + 1;
    }else{
        maxIndex = num_interrupts - 1;
        oldestIndex = 0;
    }
    for(index = 0; index <= maxIndex; ++index, ++count){
//        latency = (float) timer_info->latency_buffer[indices[index]] / usFPGA;
        latency = (float) timer_info->latency_buffer[indices[index]] / usFPGA;
        fprintf(output_fp, "%d\t%2.4f\t%4.4f\n",count, latency,(float) timer_info->latency_buffer[indices[index]]  );
        // can also write out indices[index] instead of count above to prove correctness
    }

    PrintIndices(maxIndex, oldestIndex, newestIndex);
    
    /* close output file*/
    fclose(output_fp);
    printf("Latency data (%d measurements) written to \"%s\".\n", count-1, filename);
    free(indices);
}

void DumpBigBuffer(uint64_t *largeBuffer, timer_info_t *timer_info){
    /*This function takes the array of latency data inside the largeBuffer 
     * and writes it to a text file in the order it was collected.  
     * If the buffer has been overfilled (wrapped around), then it should
     * start writing from the middle of the array (oldestIndex), through
     * to the end of the array, then wrap around and continue from the
     * beginning of the array */
   
    /* open a file for output */
    char filename[]  = "big_latency_data.txt";
    FILE * output_fp = fopen(filename, "w");
    int index;
    int maxIndex, oldestIndex, newestIndex;
    int buff_size = BIG_BUFFER_SIZE;
    long long unsigned int num_interrupts = timer_info->no_interrupts;
    double latency;
    int count = 1;
    int *indices;
    // print meta-data about collected information 
    printf("Interrupts measured = %llu\n", num_interrupts);
    indices = (int*) malloc(sizeof(int) * buff_size);
    CalculateIndices(indices, num_interrupts, buff_size);
#ifdef DEBUG
    printf("DEBUG::Indices calculated successfully");
#endif
    newestIndex = (num_interrupts - 1) % buff_size;
    if(num_interrupts > buff_size){
        maxIndex = buff_size - 1;
        oldestIndex = newestIndex + 1;
    }else{
        maxIndex = num_interrupts - 1;
        oldestIndex = 0;
    }
#ifdef DEBUG
    printf("old/new calcualted successfully");
#endif
    for(index = 0; index <= maxIndex; ++index, ++count){
        latency = (float) largeBuffer[indices[index]] / usFPGA;
        fprintf(output_fp, "%d\t%2.4f\n", count, latency);
        // can also write out indices[index] instead of count above to prove correctness
    }

    PrintIndices(maxIndex, oldestIndex, newestIndex);
    
    /* close output file*/
    fclose(output_fp);
    printf("Long-term Latency data (%d measurements) written to \"%s\".\n",count-1, filename);
    free(indices);
}

void displayHeader(){
	printf("\nFollowing commands are valid\n");
	printf("1: Start 1m periodic test\n");
	printf("0: Stop 1m periodic test\n");
	printf("s: Show statistics\n");
	printf("p: Poll for increases in maxlat\n");
	printf("d: Dump latency measurements to a file\n");
	printf("b: Dump long-term latency measurements (large buffer) to a file\n");
    printf("r: Reset statistics\n");
    	printf("t: Set spike threshold and start spike detection tracing\n");
	printf("e: Exit\n");
}

void StopIntervalTimer(){
    printf("Stopping test\n");
//    printf("BAR2 VA (%x)\n",handle[0]->latint[(handle[0]->latint->pciextIndex)].pciextFunction.bar_2_virtaddr); 
    bar2_write_csr(handle[0],TIMER_CONTROL,0x0008);	// stop interval timer
    bar2_write_csr(handle[0],AVALON_INT_ENABLE,0x0000);	// Sets Qsys interrupt enable 0
}

static float lat_max = 0.0;
static time_t start_time;

#define ALM_INTERVAL 60

static void sigalm_handler(int signal)
{
	timer_info_t timer_info;
	float cur_max;
	time_t now;

	GetTimerStats(handle[0],&timer_info);
	cur_max = (float)timer_info.latency_max / usFPGA;

	if (cur_max > lat_max) {
		int hrs, min, sec;

		now = time(NULL);
		sec = now - start_time;
		min = sec / 60;
		hrs = min /60;
		min = min - (hrs * 60);
		printf("%dh:%dm: New latmax is: %0.2fus, prev. was: %0.2fus\n",
			hrs, min, cur_max, lat_max);
		lat_max = cur_max;
	}
	alarm(ALM_INTERVAL);
}

static struct sigaction act;
static struct sigaction oldact;

void enable_poll()
{
	memset(&act, 0, sizeof(act));
	memset(&act, 0, sizeof(oldact));

	act.sa_handler = sigalm_handler;

	if (sigaction(SIGALRM, &act, &oldact)) {
		printf("enable_poll: install new sigaction failed\n");
		exit(-1);
	}
	alarm(ALM_INTERVAL);
}

void disable_poll()
{
	if (sigaction(SIGALRM, &oldact, NULL)) {
		printf("disable_poll: restore old sigaction failed\n");
		exit(-1);
	}
	alarm(0);
}

void doTesting(void)
{
	int poll = 0;
	unsigned char c;
	uint32_t	period;
	timer_info_t	timer_info;
#ifdef DEBUG_MMAP_WORKING
    uint64_t       *largeBuffer = malloc( sizeof(uint64_t) * BIG_BUFFER_SIZE ); 
#endif
	uint32_t threshold;

    bar2_write_csr(handle[0],TIMER_CONTROL,0x0008);	// Make sure timer is off
	period = (1E-3 * 125E6); // 1ms period based on 125Mhz counter
        SetTimerPeriod(handle[0],period);

	displayHeader();
    do
	{
		scanf("%c",&c);
		fflush(stdin);
		switch (c)
		{
		case '1': // Start test
			printf("Starting test\n");
			start_time = time(NULL);
			bar2_write_csr(handle[0],AVALON_INT_ENABLE,0x0002);	// Sets Qsys interrupt enable 1
			bar2_write_csr(handle[0],TIMER_CONTROL,0x0007);	// run interval timer continuously
			break;
		case '0': // Stop test
			if (poll) {
				poll = 0;
				disable_poll();
			}
//			printf("Stopping test\n");
//			bar2_write_csr(handle[0],TIMER_CONTROL,0x0008);	// stop interval timer
//			bar2_write_csr(handle[0],AVALON_INT_ENABLE,0x0000);	// Sets Qsys interrupt enable 0
            StopIntervalTimer();
            displayHeader();
			break;
		case 's': // Show statistics 
			GetTimerStats(handle[0],&timer_info);
			DisplayTimerStats(&timer_info);
			break;
		case 'p': // toggle poll on/off statistics for max lat
			if (poll) {
				poll = 0;
				printf("Disabling polling for maxlat increases\n");
				disable_poll();
			} else {
				poll = 1;
				printf("Enabling polling for maxlat increases\n");
				enable_poll();
			}
			break;
		case 'd': // dump latency measurements to a file
//			printf("Stopping test...\n");
//			bar2_write_csr(handle[0],TIMER_CONTROL,0x0008);	// stop interval timer                      
//			bar2_write_csr(handle[0],AVALON_INT_ENABLE,0x0000);	// Sets Qsys interrupt enable 0            
            StopIntervalTimer();

            printf("Writing latency measurements to output file....\n");
            GetTimerStats(handle[0], &timer_info);
            DumpLatencyData(&timer_info);
            displayHeader();
            break;
		case 'b': // dump large-buffer to file
#ifdef DEBUG_MMAP_WORKING
            // stop test
//			printf("Stopping test...\n");
//			bar2_write_csr(handle[0],TIMER_CONTROL,0x0008);	// stop interval timer                                  
//			bar2_write_csr(handle[0],AVALON_INT_ENABLE,0x0000);	// Sets Qsys interrupt enable 0                        
            StopIntervalTimer();

            // write to file
            printf("Writing large data buffer to output file....\n");
			GetLargeBuffer(handle[0], largeBuffer);
			DumpBigBuffer(largeBuffer, &timer_info);
#else
            PRINT("Write out Large Buffer placeholder.......\n");
#endif
            displayHeader();
            break;
		case 'r': // Reset Statistics 
			bar2_write_csr(handle[0],TIMER_CONTROL,0x0008);	// stop interval timer
			SetTimerPeriod(handle[0],period);			// this also resets stats
			bar2_write_csr(handle[0],TIMER_CONTROL,0x0007);	// run interval timer continuously
			lat_max = 0.0;
			printf("Stats reset\n");
			break;
		case 't':
			threshold = set_spike_threshold();
			if (threshold != LATINT_FAIL) {
				if (start_tracing() == LATINT_SUCCESS) {
					tracing_on = 1;
					printf("Spike threshold set to %dus and auto stop tracing enabled.\n", threshold);
				} else
					perror("Tracing fail to start\n");
			} else
				perror("Spike threshold set fail\n");
			break;
		case 'e': // End test
			StopIntervalTimer();
			if (tracing_on != 0) {
				if (stop_tracing() == LATINT_FAIL)
					perror("Tracing fail to stop\n");
			} 
            printf("End test\n");

//			bar2_write_csr(handle[0],TIMER_CONTROL,0x0008);	// stop interval timer
//			bar2_write_csr(handle[0],AVALON_INT_ENABLE,0x0000);	// Sets Qsys interrupt enable 0
			break;	  
		}
	}
	while (c != 'e');

#ifdef DEBUG_MMAP_WORKING
    free(largeBuffer);
#endif
}




int main(int argc, char* argv[])
{
	unsigned int device_count = 0;
	unsigned int i = 0;
	pciext_device_t* list = NULL;
	uint32_t registerValue=0;
	uint32_t offset=0;

	FILE *cpuinfo=NULL;

	cpuinfo=fopen("/proc/cpuinfo","r");
	if (cpuinfo != NULL)
	{
		char buff[1024];
		while ( fgets( buff, sizeof buff, cpuinfo ) != NULL ) 
		{
			if ( sscanf( buff, "cpu MHz  : %lu", &cpu_frequency ) == 1 ) 
			{
				PRINT("Cpu frequency is %lu", cpu_frequency);
				break;
			}
		}
		fclose(cpuinfo);
	}
	else
	{
		PRINT("Fatal! can't open /proc/cpuinfo! aborting");
		return -6;
	}


	if (init_latint_lib() == LATINT_FAIL)
	{
		PRINT("Failed to initialize the library... now exiting.\n");
		goto error_init;
	}

	if ( (list = get_latint_device_list(&device_count)) == NULL)
	{
		PRINT("No handles found... now exiting.\n");
		goto error_init;
	}
	for(i=0; i< device_count; i++)
	{
		if ( (handle[i] = reserve_latint_device(i+1)) == NULL)
		{
			PRINT(" Failed to reserve the handle... now exiting.\n");
			goto error_init;
		}
		PRINT("Reserving device %ld",handle[i]->latint->pciextFunction.device_id);
	}
	
	enable_latint_device(handle[0]);

	if (argc <= 1)
	{
		doTesting();	// default testing
	}
	else
	{
		unsigned int selectedTest=atoi(argv[1]);
		switch (selectedTest)
		{
			//test 0: read timer registers
			case 0:
				for (i=0; i<6;i++)
				{
				  offset = 0x5000 + i*4;	
				  registerValue=bar2_read_csr(handle[0],offset);
				  PRINT("Register 0x%x is 0x%x",offset,registerValue);
				}
				break;

			//test 1: write timer register
			case 1:
				offset=atoi(argv[2]); //no responsabilities on wrong parameters...
				PRINT("Writing offset 0x%x",offset);
				registerValue = atoi(argv[3]); //no responsabilities on wrong parameters...
				PRINT("Writing value 0x%x",registerValue);
				bar2_write_csr(handle[0],offset+0x5000,registerValue);
				break;
			// read CRA register
			case 2:
				offset=atoi(argv[2]); //no responsabilities on wrong parameters...
				registerValue=bar2_read_csr(handle[0],offset);
	  		  	PRINT("Register 0x%x is 0x%x",offset,registerValue);
				break;
			// write CRA register
			case 3:
				offset=atoi(argv[2]); //no responsabilities on wrong parameters...
				PRINT("Writing offset 0x%x",offset);
				registerValue = atoi(argv[3]); //no responsabilities on wrong parameters...
				PRINT("Writing value 0x%x",registerValue);
				bar2_write_csr(handle[0],offset,registerValue);
				break;
			case 9:
				doTesting();
				break;
		}

	}
	

	printf("Ending test case\n");


	if (shutdown_latint_lib() == LATINT_FAIL)
	{
		PRINT("Cannot shutdown the handle.");
		goto error;
	}

	for(i=0; i< device_count; i++)
		free_latint_device(handle[i]);

	return 0;

error_init:
	shutdown_latint_lib();
	return -1;

error:
	for(i=0; i< device_count; i++)
		free_latint_device(handle[i]);
	shutdown_latint_lib();
	return -2;

}

