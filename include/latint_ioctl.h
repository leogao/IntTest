/******************************************************************************
* @file  latint_ioctl.h 
*
* GPL LICENSE SUMMARY
* 
*   Copyright(c) 2010 Intel Corporation. All rights reserved.
* 
*   This program is free software; you can redistribute it and/or modify 
*   it under the terms of version 2 of the GNU General Public License as
*   published by the Free Software Foundation.
* 
*   This program is distributed in the hope that it will be useful, but 
*   WITHOUT ANY WARRANTY; without even the implied warranty of 
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
*   General Public License for more details.
* 
*   You should have received a copy of the GNU General Public License 
*   along with this program; if not, write to the Free Software 
*   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
*   The full GNU General Public License is included in this distribution 
*   in the file called LICENSE.GPL.
* 
*   Contact Information:
*   Intel Corporation
* 
*  version: FPGA_Latint.L.0.1.1
*
******************************************************************************/
#ifndef LATINT_IOCTL_H
#define LATINT_IOCTL_H

// comment out DEBUG_MMAP_WORKING to disable largeBuffer in all driver/user-space components
#define DEBUG_MMAP_WORKING

// uncomment SPIKE_DETECTION to enable the tracing_off() function when spike exceeded
// #define SPIKE_DETECTION

// uncomment DEBUG to enable debug mode in the driver and user_space code
//#define DEBUG

#define MAX_PCI_BARS      	 			6    /*Max No. of BARS for any device */

#define PCIEXT_DEVICE_ID				0x0004
#define PCIEXT_PCI_VENDOR_ID				0x1172

#define PCIEXT_BARS_COUNT 				3	// 3 BARS

/* Used to index the BARS array */
#define PCIEXT_BAR0 					0
#define PCIEXT_BAR2 					2

/* Used to offset the BARS address space */
#define PCIEXT_BAR_HW 					0

#define PCIEXT_BAR0_LENGTH				4194304
#define PCIEXT_BAR2_LENGTH				32768

#define MMAP_BAR0					0x0
#define MMAP_BAR2					0x2
#define MMAP_TIMER					0x3
#define	MMAP_TEST					0x4
//#define MMAP_DATA                  			0x5


/* BAR2 register space */
#define	TIME_STATUS		    0x5000
#define	TIMER_CONTROL		0x5004
#define	TIMER_PERIODL		0x5008
#define	TIMER_PERIODH		0x500C
#define	TIMER_SNAPL		    0x5010
#define	TIMER_SNAPH		    0x5014

#define	AVALON_INT_STATUS	0x0040
#define AVALON_INT_ENABLE	0x0050


#define WORDTYPE            uint32_t
#define WORDSIZE            sizeof(WORDTYPE)


#define SYSTEMTESTS_PRIORITY    90
#define STACK_SIZE              8192

/* IOCTLs commands */
#define PCIEXT_CMD_MAGIC			'e'
#define PCIEXT_CMD_INIT				(0)
#define PCIEXT_CMD_SHUTDOWN			(1)
#define PCIEXT_CMD_GET_DEVICE			(2)
#define PCIEXT_CMD_GET_DEVICE_COUNT		(3)
#define PCIEXT_CMD_THRESHOLD_START		(4)
#define PCIEXT_CMD_THRESHOLD_STOP		(5)
#define PCIEXT_CMD_SET_SPIKE_THRESHOLD		(6)

#define PCIEXT_IOCTL_INIT			_IOWR(PCIEXT_CMD_MAGIC, PCIEXT_CMD_INIT, unsigned int)
#define PCIEXT_IOCTL_SHUTDOWN			_IOWR(PCIEXT_CMD_MAGIC, PCIEXT_CMD_SHUTDOWN, unsigned int)
#define PCIEXT_IOCTL_GET_DEVICE			_IOWR(PCIEXT_CMD_MAGIC, PCIEXT_CMD_GET_DEVICE, struct pciext_function_s)
#define PCIEXT_IOCTL_DEVICE_COUNT		_IOWR(PCIEXT_CMD_MAGIC, PCIEXT_CMD_GET_DEVICE_COUNT, UINT)
#define PCIEXT_IOCTL_THRESHOLD_START		_IOWR(PCIEXT_CMD_MAGIC, PCIEXT_CMD_THRESHOLD_START, UINT)
#define PCIEXT_IOCTL_THRESHOLD_STOP		_IOWR(PCIEXT_CMD_MAGIC, PCIEXT_CMD_THRESHOLD_STOP, unsigned int)
#define PCIEXT_IOCTL_SET_SPIKE_THRESHOLD	_IOWR(PCIEXT_CMD_MAGIC, PCIEXT_CMD_SET_SPIKE_THRESHOLD, unsigned int)


#define DMA_OFFSET_IN_BAR				0x0000
#define CRA_OFFSET_IN_BAR				0x4000
#define CRA_OFFSET_ADDRESS_TRASLATION_OFFSET		0x1000
#define CRA_OFFSET_ADDRESS_TRASLATION			( CRA_OFFSET_ADDRESS_TRASLATION_OFFSET + CRA_OFFSET_IN_BAR )
#define CRA_LENGTH					0x8000

#define ALTERA_CRA_OFFSET				0x1000
#define ALTERA_CRA_ADDRESS_TRANSLATION_MASK		((UINT*)(0xFFFFFF00LL))


#ifndef PAGE_SIZE
#define PAGE_SIZE					4096
#endif

#define NUMBER_OF_FPGA					1
#define DEVICE_IDS					2

#define E_THREAD_ABORTED							(-1)
#define E_PACKET_ABORTED 							(-2)

// short-term buffer size - 100k
#define LATENCY_BUFFER_SIZE 100000 
//static const uint64_t LATENCY_BUFFER_SIZE = 100000;

/* Spike threshold range, 10us ~ 100us */
#define THRESHOLD_MIN					10
#define THRESHOLD_MAX					100

// BIG_BUFFER_SIZE for long-term latency data - 1M
//#define BIG_BUFFER_SIZE 500000
static const uint64_t BIG_BUFFER_SIZE = 500000;

#ifdef __x86_64__
typedef uint64_t UINT;
#else
typedef uint32_t UINT;
#endif

#ifndef __CONF_DRV__
typedef uint64_t dma_addr_t;
#endif

/* added a buffer to hold the latency measurements [JMW]*/
typedef struct timer_info_s 
{
	uint64_t	latency_min;
	uint64_t	latency_max;
	uint64_t	latency_avg;
	uint64_t	jitter_min;
	uint64_t	jitter_max;
	uint64_t	jitter_avg;
	uint64_t	no_interrupts;
	uint64_t	last_timestamp;
	uint32_t	period;
	uint64_t	buffer_size;
	uint64_t 	latency_buffer[LATENCY_BUFFER_SIZE];	
} timer_info_t;

#if 0
/* added a large buffer ptr to hold the long-term (1M) latency measurements [JMW]*/
typedef struct big_buffer_s
{
	uint64_t	buffer_size;
	uint64_t*	latency_data;
} big_buffer_t;
#endif

typedef struct function_bar_s 
{
	unsigned long base_addr;    		/* read from PCI config */
	void * virt_addr;			/* mapped to kernel VA */
	unsigned long size;         		/* size of BAR */
} function_bar_t;

/*
 * PCI information
 */
struct pciext_pci_info_s 
{
#ifdef __CONF_DRV__
	struct pci_dev *pdev;
#endif
	UINT device_id;
	unsigned int irq;
	UINT num_bars;
	void * kern_timer_va;
	void * kern_timer_ptr;
	function_bar_t pci_bars[MAX_PCI_BARS];
	unsigned int * lib_timer_va;
#ifdef DEBUG_MMAP_WORKING
    // new stuff for the big buffer:::
	void * kern_buffer_va;
	void * kern_buffer_ptr;
	unsigned int * lib_buffer_va;
#endif
//	void * kmalloc_ptr;
//	uint32_t* writeBuffer[TOTAL_DMA_ENGINES];
//	uint32_t* readBuffer[TOTAL_DMA_ENGINES];
//	dma_addr_t writeBufferDma[TOTAL_DMA_ENGINES];
//	dma_addr_t readBufferDma[TOTAL_DMA_ENGINES];
};

typedef struct pciext_function_s 
{
	UINT device_id;
	unsigned int irq;
	UINT num_bars;
	UINT bar_0_virtaddr;
	UINT bar_2_virtaddr;
	void * kern_timer_va;
	UINT* writeBuffer;
	UINT* readBuffer;
	dma_addr_t writeBufferDma;
	dma_addr_t readBufferDma;
	function_bar_t pci_bars[MAX_PCI_BARS];
	void * lib_timer_va;
// new stuff for big buffer::
#ifdef DEBUG_MMAP_WORKING
    void * kern_buffer_va;
	void * lib_buffer_va;
#endif
} pciext_function_t;

typedef enum
{
	FALSE = 0,
	TRUE
} boolean;


typedef struct
{
	boolean reserved;
	unsigned int pciextIndex;
	boolean pciextEnabled;
	boolean master;

	pciext_function_t pciextFunction;

} pciext_device_t;

#endif



