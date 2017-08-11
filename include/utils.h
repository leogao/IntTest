/******************************************************************************
* @file  utils.h
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
#ifndef UTILS_H
#define UTILS_H

#ifdef DEBUG
#ifndef XENOMAI_PATCH
#define PRINT(fmt, args...)   printf("DEBUG UNSAFE: %s:%d " fmt "\n", __FUNCTION__,__LINE__, ## args)
#define FPRINTF(fmt, args...)   fprintf(stderr,"DEBUG UNSAFE: %s:%d " fmt "\n", __FUNCTION__,__LINE__, ## args)
#else
#define PRINT(fmt, args...)   rt_printf("%s:%d " fmt "\n", __FUNCTION__,__LINE__, ## args)
#define FPRINTF(fmt, args...)  rt_printf("DEBUG: %s:%d \n" fmt "\n", __FUNCTION__,__LINE__, ## args)
#endif
#else
#define PRINT(args...)
#define FPRINTF(args...)
#endif

#ifndef XENOMAI_PATCH
#define LOG(fmt, args...)   printf("LOG UNSAFE: " fmt "\n", ## args)
#else
#define LOG(fmt, args...)   rt_printf( fmt "\n", ## args)
#endif


#endif
