/******************************************************************************
* @file  cpri_api.h
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
******************************************************************************/
#ifndef LATINT_API_H
#define LATINT_API_H

typedef enum _LatIntStatus
{
	LATINT_SUCCESS = 0,
	LATINT_FAIL

} LatIntStatus;


typedef struct latint_handle_s
{
	/* Buffer which hold the data read from the CPRI */ 
//	UINT* cpri_bitstream_buffer_rx_ptr;
	/* Buffer which hold the data to be written to the CPRI */ 
//	UINT* cpri_bitstream_buffer_tx_ptr;

	/* Reserved to internal use of the library */
	pciext_device_t* latint;

	/* The size of the buffers is defined in: */
//	UINT cpri_bitstream_buffer_length;

	/* slots size and number of slots stats: */
//	UINT cpri_rx_slot_size;
//	UINT cpri_rx_slot_count;
//	UINT cpri_tx_slot_size;
//	UINT cpri_tx_slot_count;



} latint_handle_t;
typedef latint_handle_t* LatintHandle;

/*
 * @description
 * 		This function takes care of initializing the library and open the file descriptor for
 * 		kernel to user space communication.
 * 		It registers the signal handler routine which are invoked the first time an interrupt
 * 		occurs.
 *
 * @retval	CpriStatus
 */
LatIntStatus init_latint_lib();

/*
 * @description
 * 		This function provides a list of the device on the system. The pointer can be incremented
 * 		by the number of devices which is stored in device_count.
 *
 * @param[out]	device_count	The number of devices on the system
 *
 * @retval	List of devices
 */
pciext_device_t* get_latint_device_list(unsigned int* device_count);

/*
 * @description
 * 		This function takes care of reserving mapping the BARs memory space and the DMA memory
 * 		used for RX/TX buffers. It also reserves the device on the system.
 *
 * @param[in]	cpri_device		The device which is going to be reserved.
 *
 * @retval	CpriHandle
 */
LatintHandle reserve_latint_device(unsigned int cpri_device);

/*
 * @description
 * 		This function takes care of unreserving the device on the system.
 *
 * @param[in]	CpriHandle		The handle to the device previously reserved.
 *
 * @retval	CpriStatus
 */
LatIntStatus free_latint_device(LatintHandle handle);

/*
 * @description
 * 		This function takes care of shutting down the library. The main operation is to close the
 * 		file descriptor for kernel to user space communication.
 *
 * @retval	CpriStatus
 */
LatIntStatus shutdown_latint_lib();

/*
 * @description
 * 		This function takes care of enabling the CPRI
 *
 * @retval	CpriStatus
 */
LatIntStatus enable_latint_device(LatintHandle handle);


// Function which can be used to read bar registers value 
uint32_t bar2_read_csr(LatintHandle handle,  uint32_t offset);

// Function which can be used to write bar registers value 
void bar2_write_csr(LatintHandle handle, uint32_t offset, uint32_t value);

// Function which can be used to read bar registers value 
uint32_t bar0_read_csr(LatintHandle handle,  uint32_t offset);

// Function which can be used to write bar registers value 
void bar0_write_csr(LatintHandle handle, uint32_t offset, uint32_t value);
// Get timer interval period
uint32_t GetTimerPeriod(LatintHandle handle);
void SetTimerPeriod(LatintHandle handle, uint32_t period);
void GetTimerStats(LatintHandle handle, timer_info_t *timer_info);
//inline void GetBigBuffer(LatintHandle handle, big_buffer_t *buffer);
void GetLargeBuffer(LatintHandle handle, uint64_t *largeBuffer);

#endif
