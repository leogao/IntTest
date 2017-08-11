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
*  version: FPGA_CPRI.L.0.1.4-14
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h> 
#include <string.h>
#include <sched.h>
#include "utils.h"
#include "latint_ioctl.h"
#include "latint_api.h"
//#include "include/cpri_regs.h"

#ifdef XENOMAI_PATCH
#include <rtdk.h> 
#include <native/task.h>
#include <native/queue.h>
#include <native/intr.h>
#include <native/timer.h>
#endif

#define PAGE_SELECTOR_OFFSET		21
#define DRAM_ADDRESS_MASK  		0x1ffff8

static int fd = -1;
static unsigned int latint_device_count = 0;
static pciext_device_t* latint = NULL;
//static struct timer_info_s *timer_info;

#define min(a,b) (((a) < (b)) ? (a) : (b))

// Function which can be used to read bar2 registers value 
uint32_t bar2_read_csr(LatintHandle handle,  uint32_t offset) 
{
	if ( (handle == NULL) || (handle->latint==NULL) || (offset > (PCIEXT_BAR2_LENGTH -1 )) ) 
		return -1;
	return *((volatile uint32_t *) (  latint[(handle->latint->pciextIndex)].pciextFunction.bar_2_virtaddr + (offset)));
}

/* Function which can be used to write bar registers value */
void bar2_write_csr(LatintHandle handle, uint32_t offset, uint32_t value)
{
	if ( (handle == NULL) || (handle->latint==NULL) || (offset > (PCIEXT_BAR2_LENGTH -1 )) ) 
		return; // -1;

	volatile uint32_t * tempValue=(volatile uint32_t *)(latint[(handle->latint->pciextIndex)].pciextFunction.bar_2_virtaddr + (offset));

	*( tempValue ) = value;
}

// Function which can be used to read bar0 registers value 
uint32_t bar0_read_csr(LatintHandle handle,  uint32_t offset) 
{
	if ( (handle == NULL) || (handle->latint==NULL) || (offset > (PCIEXT_BAR2_LENGTH -1 )) ) 
		return -1;
	return *((volatile uint32_t *) (  latint[(handle->latint->pciextIndex)].pciextFunction.bar_0_virtaddr + (offset)));
}

/* Function which can be used to write bar registers value */
void bar0_write_csr(LatintHandle handle, uint32_t offset, uint32_t value)
{
	if ( (handle == NULL) || (handle->latint==NULL) || (offset > (PCIEXT_BAR0_LENGTH -1 )) ) 
		return; // -1;

	volatile uint32_t * tempValue=(volatile uint32_t *)(latint[(handle->latint->pciextIndex)].pciextFunction.bar_0_virtaddr + (offset));

	*( tempValue ) = value;
}

uint32_t GetTimerPeriod(LatintHandle handle)
{
	return ((struct timer_info_s *) (latint[(handle->latint->pciextIndex)].pciextFunction.lib_timer_va))->period;	
}


void SetTimerPeriod(LatintHandle handle, uint32_t period)
{
	struct timer_info_s *mt_info = (latint[(handle->latint->pciextIndex)].pciextFunction.lib_timer_va);
	bar2_write_csr(handle,TIMER_PERIODL,period & 0xFFFF);	
	bar2_write_csr(handle,TIMER_PERIODH,period >> 16);
	mt_info->period = period;
	// reset defaults also
	mt_info->latency_min = 0xFFFFFFFF;
	mt_info->latency_max = 0x0;
	mt_info->latency_avg = 0x0;
	mt_info->jitter_min = 0xFFFFFFFF;
	mt_info->jitter_max = 0x0;
	mt_info->jitter_avg = 0x0;
	mt_info->no_interrupts = 0x0;
	mt_info->last_timestamp = 0x0;
}

// Function which can be used to get the structure containing latency statistics
void GetTimerStats(LatintHandle handle, timer_info_t *timer_info)
{
	struct timer_info_s *mt_info = (latint[(handle->latint->pciextIndex)].pciextFunction.lib_timer_va);
    int index = 0;
	timer_info->period = mt_info->period;
	timer_info->latency_min = mt_info->latency_min;
	timer_info->latency_max = mt_info->latency_max;
	timer_info->latency_avg = mt_info->latency_avg;
	timer_info->jitter_min = mt_info->jitter_min;
	timer_info->jitter_max = mt_info->jitter_max;
	timer_info->jitter_avg = mt_info->jitter_avg;
	timer_info->no_interrupts = mt_info->no_interrupts;
    timer_info->buffer_size = mt_info->buffer_size;
    // copy the buffer
    for(index = 0; index < timer_info->buffer_size; index++){
       	timer_info->latency_buffer[index] = mt_info->latency_buffer[index];
    }
}

//// Function which may be used to retreive a large-buffer of latency measurements
//inline void GetBigBuffer(LatintHandle handle, big_buffer_t *buffer)
//{
//	struct big_buffer_s *mt_buffer = (latint[(handle->latint->pciextIndex)].pciextFunction.lib_buffer_va); 
//	int index = 0;
//	buffer->buffer_size = mt_buffer->buffer_size;
//	// deep-copy latency buffer
//	for(index = 0; index < buffer->buffer_size; index++){
//		buffer->latency_data[index] = mt_buffer->latency_data[index];
//	}
//}

// Function which may be used to retreive a large-buffer of latency measurements
void GetLargeBuffer(LatintHandle handle, uint64_t *largeBuffer)
{
#ifdef DEBUG_MMAP_WORKING
    uint64_t *kernelBuffer =  (latint[(handle->latint->pciextIndex)].pciextFunction.lib_buffer_va); 
    int index = 0;
	// deep-copy latency buffer
	for(index = 0; index < BIG_BUFFER_SIZE; index++){
		largeBuffer[index] = kernelBuffer[index];
	}
#endif
}

//with this public function, the upper stack can enable the cpri any time. 
LatIntStatus enable_latint_device(LatintHandle handle)
{	

	if (handle == NULL)
	{
		PRINT("Invalid handle.");
		return LATINT_FAIL;
	}
	
	if (handle->latint == NULL)
	{
		PRINT("Invalid device.");
		return LATINT_FAIL;
	}
	handle->latint->pciextEnabled = TRUE;

//	timer_info = (struct timer_info_s *)(handle->latint->pciextFunction).lib_timer_va;	

	return LATINT_SUCCESS;
}



//first initialization step function. 
LatIntStatus init_latint_lib() 
{
	unsigned int i = 0;

	//lock all pages for no page faults during runtime.
	mlockall(MCL_CURRENT | MCL_FUTURE);
#ifdef XENOMAI_PATCH
	rt_print_auto_init(1);
	rt_print_init(4096, "Init LatInt Lib");
#endif


	fd = open("/dev/ILatIntDrv", O_RDWR);

	if (fd == -1)
	{
		perror("Failed to open the device. Error is:  ");
		printf("init_latint_lib()::: failed to open the device");
		return LATINT_FAIL;	
	}


	if (ioctl(fd, PCIEXT_IOCTL_INIT, &i) < 0)
	{
		PRINT("INIT failed.");
		close(fd);
		fd=-1;
		printf("init_latint_lib()::: IOCTL INIT failed");
		return LATINT_FAIL;
	}

	if (ioctl(fd, PCIEXT_IOCTL_DEVICE_COUNT, &latint_device_count) < 0)
	{
		PRINT("PCIEXT_IOCTL_DEVICE_COUNT failed.");
		close(fd);
		fd=-1;
		printf("init_latint_lib()::: IOCTL device count failed");
		return LATINT_FAIL;
	}

	if (latint_device_count < 1 )
	{
		PRINT("No Fpga present! Aborting... ");
		close(fd);
		fd=-1;
		printf("init_latint_lib()::: No FPGA present!");
		return LATINT_FAIL;
	}
	latint = malloc(sizeof(pciext_device_t) * latint_device_count);
	if (latint==NULL)
	{
		PRINT("No memory available!!!");
		close(fd);
		fd=-1;
		printf("init_latint_lib()::: No FPGA present!");
		return LATINT_FAIL;
	}

	for(i=0; i<latint_device_count; i++)
	{
		memset(&latint[i].pciextFunction, 0, sizeof(latint[i].pciextFunction));

		latint[i].pciextFunction.device_id = PCIEXT_DEVICE_ID +i ;

		if (ioctl(fd,PCIEXT_IOCTL_GET_DEVICE, &latint[i].pciextFunction) < 0)
		{
			PRINT("PCIEXT_IOCTL_GET_DEVICE failed: cannot get TX for dev %d ID 0x%lX",
				i,latint[i].pciextFunction.device_id );
			printf("init_latint_lib()::: PCIEXT_IOCTL_GET_DEVICE failed: cannot get TX for dev %d ID 0x%lX",
				i,latint[i].pciextFunction.device_id );
			return LATINT_FAIL;
		}
		
	}

	return LATINT_SUCCESS;
}

//second step initialization function
pciext_device_t* get_latint_device_list(unsigned int* device_count)
{
	*device_count = latint_device_count;
	return latint;
}

//third and last step initialization function
LatintHandle reserve_latint_device(unsigned int latint_device)
{
	LatintHandle handle = NULL;
	unsigned int _index = latint_device - 1;


	if ((latint_device > latint_device_count) || (latint_device == 0))
	{
		PRINT("Device %u does not exist.", latint_device);
		return NULL;
	}

	if (latint == NULL)
	{
		PRINT("The library has not been initialized yet.You need to call init_cpri_lib() first.");
		return NULL;
	}

	if (latint[_index].reserved == TRUE)
	{
		PRINT("This device seems to be already reserved.");
		return NULL;
	}

	// FPGA I/O space for TX 
	latint[_index].pciextFunction.bar_2_virtaddr = 
		(UINT) mmap(NULL, PCIEXT_BAR2_LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 
			((latint[_index].pciextFunction.device_id*0x10+ MMAP_BAR2)*(PAGE_SIZE)));
	
   	latint[_index].pciextFunction.bar_0_virtaddr = 
		(UINT) mmap(NULL, PCIEXT_BAR0_LENGTH, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 
			((latint[_index].pciextFunction.device_id*0x10+ MMAP_BAR0)*(PAGE_SIZE)));

   	latint[_index].pciextFunction.lib_timer_va = 
		(void *)mmap(NULL, sizeof(struct timer_info_s), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, 
			((latint[_index].pciextFunction.device_id*0x10+ MMAP_TIMER)*(PAGE_SIZE)));

#ifdef DEBUG_MMAP_WORKING
   	latint[_index].pciextFunction.lib_buffer_va = 
		(void *)mmap(NULL, sizeof(uint64_t)*BIG_BUFFER_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_LOCKED, fd, 
			((latint[_index].pciextFunction.device_id*0x10+ MMAP_TEST)*(PAGE_SIZE)));
#endif

	latint[_index].reserved = TRUE;
	latint[_index].pciextEnabled = FALSE;
	latint[_index].master = FALSE;
	latint[_index].pciextIndex = _index;

	handle = (LatintHandle) malloc(sizeof(latint_handle_t));

	if (handle == NULL)
	{
		PRINT("Error!! handle pointer is null!! aborting");
		return NULL;
	}

	handle->latint = &latint[_index];

	return handle;
}

//Public function for cleaning the library and closing the special file handle.
LatIntStatus shutdown_latint_lib()
{
	unsigned int i = 0;

	if (fd == -1)
	{
		PRINT("Device not open");
		return LATINT_FAIL;	
	}


	if (ioctl(fd, PCIEXT_IOCTL_SHUTDOWN, &i) < 0)
	{
		PRINT("PCIEXT_IOCTL_SHUTUDOWN failed.");
		return LATINT_FAIL;
	}

    //needed if we shutdown multiple time and then we dont call reserver!!
	fsync(fd);
	close(fd);
	fd=-1; 
	return LATINT_SUCCESS;
}

//releasing cpri handle. 
LatIntStatus free_latint_device(LatintHandle handle)
{
	pciext_device_t * deviceHandle=NULL;

	if (handle == NULL)
	{
		PRINT("Invalid handle.");
		return LATINT_FAIL;
	}
	
	if (handle->latint == NULL)
	{
		PRINT("Invalid device.");
		return LATINT_FAIL;
	}
	deviceHandle=((pciext_device_t*)(handle->latint));
	if (deviceHandle->reserved == TRUE)
	{
		munmap((void*)( deviceHandle->pciextFunction.bar_0_virtaddr), PCIEXT_BAR0_LENGTH);
		munmap((void*)( deviceHandle->pciextFunction.bar_2_virtaddr), PCIEXT_BAR2_LENGTH);
		munmap((void*)( deviceHandle->pciextFunction.lib_timer_va), sizeof(struct timer_info_s));
#ifdef DEBUG_MMAP_WORKING
        munmap((void*)( deviceHandle->pciextFunction.lib_buffer_va), sizeof(uint64_t)*BIG_BUFFER_SIZE );
#endif
    }
	deviceHandle->reserved = FALSE;
	free(handle);

	return LATINT_SUCCESS;
}

uint32_t set_spike_threshold()
{
	uint32_t spike_threshold = 0;

	printf("Enter spike threshold in microseconds(us)\n");

	do {
		if (scanf("%d", &spike_threshold) < 0) {
			perror("Input spike threshold error.\n");
			return LATINT_FAIL;
		}

		fflush(stdin);

		if ((spike_threshold < THRESHOLD_MIN) || (spike_threshold > THRESHOLD_MAX))
			printf("Threshold value is out of range. Enter a threshold between %d - %d.\n", THRESHOLD_MIN, THRESHOLD_MAX);
	} while ((spike_threshold < THRESHOLD_MIN) || (spike_threshold > THRESHOLD_MAX));

	if (ioctl(fd, PCIEXT_IOCTL_SET_SPIKE_THRESHOLD, &spike_threshold) < 0) { 
		perror("PCIEXT_IOCTL_SET_SPIKE_THRESHOLD failed.\n");
		return LATINT_FAIL;
	}

	return spike_threshold;
}
		
uint32_t start_tracing()
{
	if (ioctl(fd, PCIEXT_IOCTL_THRESHOLD_START) < 0) {
		perror("PCIEXT_IOCTL_THRESHOLD_START failed.\n");
		return LATINT_FAIL;
	}

	return LATINT_SUCCESS;
}

uint32_t stop_tracing()
{
	if (ioctl(fd, PCIEXT_IOCTL_THRESHOLD_STOP) < 0) {
		perror("PCIEXT_IOCTL_THRESHOLD_STOP failed.\n");
		return LATINT_FAIL;
	}

	return LATINT_SUCCESS;
}
