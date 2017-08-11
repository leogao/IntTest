/******************************************************************************
* @file  ILatIntDrv.c 
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
*  version: FPGA_LatInt0.1.1
******************************************************************************/

#ifndef CONFIG_PCI
# error "This driver needs PCI support to be available"
#endif

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/pci.h>
#include <linux/slab.h>
//#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/div64.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <asm/dma.h>

#define __CONF_DRV__ 0
#include "latint_ioctl.h"

#ifdef DEBUG
#define PRINTK(fmt, args...)  printk(KERN_WARNING "ILatIntDrv DEBUG: %s:%d " fmt "\n", __FUNCTION__,__LINE__, ## args)
#define EPRINTK(fmt, args...) printk(KERN_ERR     "ILatIntDrv ERROR: %s:%d " fmt "\n", __FUNCTION__,__LINE__, ## args)
#else
#define PRINTK(args...)
#define EPRINTK(args...)
#endif

/* 
 * Spike detection threshold, once exceeded spike is detected stop host tracing.
 */
static unsigned int spike_threshold;

#define SUCCESS 							0
#define DEVICE_NAME 						"ILatIntDrv"	/* Dev name as it appears in /proc/devices   */

#define US_FPGA							125 /* 125MHz counter */

#define HOST_TRACING_ON_PORT					0x56
#define HOST_TRACING_OFF_PORT					0x55
 
#define THRESHOLD_MIN						10
#define THRESHOLD_MAX						100

/**********************************
 * TYPE DEFINITIONS
 *********************************/
struct pciext_dev_s 
{
	struct pciext_pci_info_s device;   /* PCI Device */
	/* Linked list of device_handles */
	struct pciext_dev_s *pnext;
	struct pciext_dev_s *pprev;
};

static bool cleanup = 0;
/*  
 *  Prototypes 
 */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static int device_mmap(struct file *file, struct vm_area_struct * vma);
static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int device_pci_shutdown(void);

static int pciext_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
static void pciext_remove(struct pci_dev *pdev);
static void pciext_cleanup_device(struct pciext_dev_s *pciext_dev);

static struct pciext_dev_s *pciext_table_check(struct pci_dev *pdev);
static struct pciext_dev_s *pciext_table_get_device(unsigned int device_id);
static void pciext_device_table_add(struct pciext_dev_s *pciext_dev);
static int pciext_device_table_remove(struct pciext_dev_s *pciext_dev);
static unsigned int pciext_table_get_device_count(void);

static int bind_irq(unsigned int * vector);
static void unbind_irq(unsigned int *vector);
static irqreturn_t pciext_interrupt_handler(int irg, void *dev_id);

static int device_start_threshold_tracing(void);
static int device_stop_threshold_tracing(void);

static int set_spike_threshold(unsigned int *);

static void start_host_tracing(void);
static void stop_host_tracing(void);

static struct pci_device_id pciext_pci_tbl[] = 
{
	/*PCI device table */
	{ PCI_DEVICE(PCIEXT_PCI_VENDOR_ID, PCIEXT_DEVICE_ID) },
	{0,}
};

static int _major;		/* _major number assigned to our device driver */
static int _device_open = 0;	/* Is device open? Used to prevent multiple access to device */


DECLARE_WAIT_QUEUE_HEAD(pciext_drv_q);

/* structure of function pointers for the device */
static struct file_operations fops = 
{
	.open = device_open,
	.release = device_release,
	.mmap = device_mmap,
	.unlocked_ioctl = device_ioctl	
};

/* structure of pci functions for the device */
static struct pci_driver pciext_driver =
{
	.name = DEVICE_NAME,
	.id_table = pciext_pci_tbl,
	.probe = pciext_probe,
	.remove = pciext_remove,
};

/*pointer to a linked list of PCI devices that the driver is managing*/
static struct pciext_dev_s *pciext_table = NULL;

static void start_host_tracing(void)
{
	outl(1, HOST_TRACING_ON_PORT);
}

static void stop_host_tracing(void)
{
	outl(1, HOST_TRACING_OFF_PORT);
}

static int device_start_threshold_tracing()
{
	start_host_tracing();

	return 0;
}

static int device_stop_threshold_tracing()
{
	stop_host_tracing();

	return 0;
}

static int set_spike_threshold(unsigned int * arg)
{
	if (copy_from_user(&spike_threshold, arg, sizeof(unsigned int))) {
		spike_threshold = 0;
		return -EFAULT;
	}

	if ((spike_threshold < THRESHOLD_MIN) || (spike_threshold > THRESHOLD_MAX))
		return -EINVAL;

	return spike_threshold;
}

/*
 * This function is called when the module is loaded
 */
static int __init pciext_init_module(void)
{

	PRINTK("PCIe Peripheral Device Driver Module Initialization: VendorID %x DeviceID %x",
        PCIEXT_PCI_VENDOR_ID, PCIEXT_DEVICE_ID);

	_major = register_chrdev(0, DEVICE_NAME, &fops);

	if (_major < 0) 
	{
		EPRINTK("Registering char device failed with %d for master", _major);
		return _major;
	} 
	else 
		PRINTK("Char Dev Drv with _major %d", _major);

	return pci_register_driver(&pciext_driver);

}

/*
 * This function is called when the module is unloaded
 */
static void pciext_cleanup_module(void)
{

	struct pciext_pci_info_s *pci_info = NULL;
	struct pciext_dev_s * pciext_dev = NULL;


	pciext_dev = pciext_table_get_device(PCIEXT_DEVICE_ID);
	if (pciext_dev != NULL)
	{	
		pci_info = &(pciext_dev->device);
		unbind_irq(&pci_info->irq);
		pci_disable_msi(pci_info->pdev);

		PRINTK("Pci device id 0x%x Unregistered", (unsigned int)pciext_dev->device.device_id);
	}
	else
		PRINTK("Device 0x%x NOT FOUND.",PCIEXT_DEVICE_ID);


	PRINTK("FPGA Driver Module Cleanup");

	if (cleanup == 1)
		device_pci_shutdown();



	/* 
	* Unregister the device 
	*/
	pci_unregister_driver(&pciext_driver);
	unregister_chrdev(_major, DEVICE_NAME);

}

/* 
 * Called when a process tries to open the device file, like
 * "cat /dev/mycharfile"
 */
static int device_open(struct inode *inode, struct file *file)
{

	PRINTK("Device open");	
	if (try_module_get(THIS_MODULE))
	{
		_device_open++;
		return SUCCESS;
	}

	return -EBUSY;
}

/* 
 * Called when a process closes the device file.
 */
static int device_release(struct inode *inode, struct file *file)
{
	_device_open--;		/* We're now ready for our next caller */

	/* 
	 * Decrement the usage count, or else once you opened the file, you'll
	 * never get rid of the module. 
	 */
	module_put(THIS_MODULE);

	return 0;
}

/*
 * Called when the user-space library calls the mmap libc function */
static int device_mmap(struct file *file, struct vm_area_struct * vma)
{
	struct pciext_dev_s *ptr = NULL;
	unsigned long start = vma->vm_start;
	unsigned long size  = vma->vm_end - vma->vm_start;
	unsigned long pgoff = vma->vm_pgoff;
	unsigned long pfn=0;

	unsigned int _DEVICE = pgoff / 0x10;
	unsigned int _AREA = pgoff % 0x10;
	
	struct timer_info_s *timer_info;
//	struct big_buffer_s *big_buff;
#ifdef DEBUG_MMAP_WORKING
    uint64_t *largeBuffer;
#endif

    PRINTK("LatIntDrv:::device_mmap():::beginning memory mapping...");
	switch (_AREA) 
	{
	
		case MMAP_BAR0:
		{
			ptr = pciext_table_get_device(_DEVICE);
			if (ptr == NULL)
			{
				EPRINTK("MMAP_BAR0: NULL pointer");
				return -EAGAIN;
			}

			pfn = ptr->device.pci_bars[PCIEXT_BAR0].base_addr >> PAGE_SHIFT;
			vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
			PRINTK("Mapped BAR0");
			break;
		}		
		case MMAP_BAR2:
		{
			ptr = pciext_table_get_device(_DEVICE);
			if (ptr == NULL)
			{
				EPRINTK("MMAP_BAR2: NULL pointer");
				return -EAGAIN;
			}

			pfn = ptr->device.pci_bars[PCIEXT_BAR2].base_addr >> PAGE_SHIFT;
			vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
			PRINTK("Mapped BAR2");
			break;
		}
		case MMAP_TIMER:	// Mapping timer info to user
		{
			ptr = pciext_table_get_device(_DEVICE);
			if (ptr == NULL)
				return -EAGAIN;
			pfn = page_to_pfn(virt_to_page(ptr->device.kern_timer_va));

			PRINTK("Mapped Timer Info");
			timer_info = (struct timer_info_s *)ptr->device.kern_timer_va;	

			break;
	
		}
#ifdef DEBUG_MMAP_WORKING
		case MMAP_TEST:	// Mapping struct containing big latency buffer to user
		{
            ptr = pciext_table_get_device(_DEVICE);
			if (ptr == NULL)
				return -EAGAIN;
			pfn = page_to_pfn(virt_to_page(ptr->device.kern_buffer_va));
			
			PRINTK("Mapped largeBuffer to user space");
			largeBuffer = (uint64_t*)ptr->device.kern_buffer_va;	
			break;
		}
#endif
		default:
		{
			EPRINTK("Wrong memory area!");
			return -EAGAIN;
		}
	}

	if (remap_pfn_range(vma, start, pfn, size, vma->vm_page_prot)) 
	{
		EPRINTK("remap_pfn_range failed");
		return -EAGAIN;
	}

	return 0;

}

/* Called when the user-space library issue the IOCTL with CPRI_IOCTL_INIT parameter */
static int device_pci_init(unsigned long arg)
{
	int ret_code=0;
	return ret_code;
}

/* Called when the user-space library issue the IOCTL with CPRI_IOCTL_SHUTDOWN parameter */
static int device_pci_shutdown(void)
{
	return 0;
}

/* Called when the user-space library issue the IOCTL with CPRI_IOCTL_GET_DEVICE parameter */
static int device_get_devices(struct pciext_function_s *args)
{
	struct pciext_function_s _tmp;
	struct pciext_dev_s *_device = NULL;

	unsigned int logicalDevice=0;
	if (copy_from_user (&_tmp, args, sizeof(struct pciext_function_s)))
		return -EFAULT;

	_device = pciext_table_get_device(_tmp.device_id);
	if (_device == NULL) 
	{
		EPRINTK("Device 0x%x NOT FOUND.",(unsigned int) _tmp.device_id);
		return -EFAULT;
	}

	//we are using pairs of logical device in 2 by 2: ALTERA_DEVICE_ID, and ALTERA_DEVICE_ID +1
	logicalDevice= _tmp.device_id-PCIEXT_DEVICE_ID;
	_tmp.device_id = _device->device.device_id;
	_tmp.irq = _device->device.irq;
	_tmp.num_bars = _device->device.num_bars;
	_tmp.kern_timer_va = _device->device.kern_timer_va;
	_tmp.lib_timer_va = _device->device.lib_timer_va;
#ifdef DEBUG_MMAP_WORKING
	// new stuff for big_buffer 
    _tmp.kern_buffer_va = _device->device.kern_buffer_va;
	_tmp.lib_buffer_va = _device->device.lib_buffer_va;
#endif
	memcpy(_tmp.pci_bars, _device->device.pci_bars, sizeof(_tmp.pci_bars));

	if (copy_to_user (args, &_tmp, sizeof(struct pciext_function_s)))
		return -EFAULT;

	return 0;
}

/* Called when the user-space library issue the IOCTL with pciext_IOCTL_DEVICE_COUNT parameter */
static int device_get_device_count(UINT *args)
{
	unsigned int count = 0;

	count = pciext_table_get_device_count() ;

	if (copy_to_user((unsigned long*)args, &count, sizeof(unsigned int)))
		return -EFAULT;

	return 0;
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int result = -ENOIOCTLCMD;

	switch (cmd) 
	{
		case PCIEXT_IOCTL_INIT:
		{
			result = device_pci_init(arg);
			break;
		}
		
		case PCIEXT_IOCTL_SHUTDOWN:
		{
			result = device_pci_shutdown();
			break;
		}
		case PCIEXT_IOCTL_GET_DEVICE:
		{
			result = device_get_devices((struct pciext_function_s *)arg);
			break;
		}
		case PCIEXT_IOCTL_DEVICE_COUNT:
		{
			result = device_get_device_count((UINT *)arg);
			break;
		}
		case PCIEXT_IOCTL_THRESHOLD_START:
		{
			result = device_start_threshold_tracing();
			break;
		}
		case PCIEXT_IOCTL_THRESHOLD_STOP:
		{
			result = device_stop_threshold_tracing();
			break;
		}
		case PCIEXT_IOCTL_SET_SPIKE_THRESHOLD:
		{
			result = set_spike_threshold((unsigned int *) arg);
			break;
		}
		default:
		{
			EPRINTK("Unknown ioctl: %u", cmd);
			break;
		}
	}
	return result;
}

/**********************************************************
		    PCI STUFF 
***********************************************************/


static int pciext_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct pciext_dev_s *pciextLogicalDevice;

	struct pciext_pci_info_s *pci_info = NULL;

	struct timer_info_s *timer_info;
#ifdef DEBUG_MMAP_WORKING
    uint64_t *largeBuffer;
#endif
	int new_device = 0;
	int status = 0;
	int i=0;
	uint16_t wValue;
	PRINTK("Probing device");
	if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(32))) 
	{
		if(pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(32)))
		{
			PRINTK ("PCIe device using 32 bits mask FAILED!!!!");

			if (!pci_set_dma_mask(pdev, DMA_BIT_MASK(24))) 
			{
				if(pci_set_consistent_dma_mask(pdev, DMA_BIT_MASK(24)))
				{
					EPRINTK ("PCIe device using 24 bits mask FAILED!!!!");
	 
					EPRINTK( "No suitable DMA available, aborting.");
					return -ENODEV;
				}
			}
		}
	}

	/* Ensure have valid device */
	if (ent->device != PCIEXT_DEVICE_ID) 
	{
		EPRINTK("invalid device 0x%x found..!!", ent->device);
		return -ENODEV;
	}

	/* Check device Table for an existing device structure */
	pciextLogicalDevice = pciext_table_check(pdev);
	if (pciextLogicalDevice == NULL) 
	{
		pciextLogicalDevice = kzalloc(sizeof(struct pciext_dev_s), GFP_KERNEL);
		if (pciextLogicalDevice == NULL)
		{
			EPRINTK("failed to allocate memory for device structure %d",i);
			return -ENOMEM;
		}	
		new_device = 1;
	} else 
	{
		new_device = 0;
		PRINTK("no new device?");
	}

	/* enable PCI device */
	if (pci_enable_device(pdev)) 
	{
		EPRINTK("PCI Enable Device Error");
		pciext_cleanup_device(pciextLogicalDevice);

		return -EIO;
	}

	pci_set_master(pdev);

	status = pci_set_mwi(pdev);
	if (status)
		PRINTK (" Warning: enabling mwi failed");

	status=0;

	if ((ent->device == PCIEXT_DEVICE_ID ))
	{
		PRINTK(" Requesting regions for %s", DEVICE_NAME);
		status = pci_request_regions(pdev, DEVICE_NAME);
	}

	if (status!=0) 
	{
		EPRINTK("PCI request regions failed for %s",DEVICE_NAME);	
		pciext_cleanup_device(pciextLogicalDevice);
		return -EIO;
	}
	
	/* Puts PCIe endpoint in MSI interrupt mode */
	

	pci_read_config_word(pdev,PCI_COMMAND,&wValue);
	wValue |= 0x0400;
	pci_write_config_word(pdev,PCI_COMMAND,wValue);	// Setting bit10 in COMMAND disables legacy interrupt
	pci_read_config_word(pdev,0x52,&wValue);
	wValue |= 0x0001;
	pci_write_config_word(pdev,0x52,wValue);	// Setting bit0 of MSI Ctrl/Status reg enables MSI interrupt
	
	status = pci_enable_msi(pdev);
	if (status!=0) 
	{
		EPRINTK("PCI MSI enabling failed for %s",DEVICE_NAME);	
	}

	pci_info = &pciextLogicalDevice->device;

	/*
	 * Setup PCI Info structure
	 */
	pci_info->pdev = pdev;
	pci_info->device_id = ent->device;
	pci_info->irq = pdev->irq;

	PRINTK("FPGA Device BARs:");

	pci_info->num_bars = PCIEXT_BARS_COUNT;
	for (i=0;i<PCIEXT_BARS_COUNT;i++)	// i is BAR no
	{
		pci_info->pci_bars[i].base_addr = pci_resource_start(pdev, i);
		pci_info->pci_bars[i].size = pci_resource_len(pdev, i);

		/* map the device memory region into kernel virtual address space */
		pci_info->pci_bars[i].virt_addr = ioremap(pci_info->pci_bars[i].base_addr,
		    pci_info->pci_bars[i].size);

		PRINTK("Bar %d: Start Virt Addr %p",
			i, pci_info->pci_bars[i].virt_addr);
		PRINTK("       Start Phys Addr %lx", pci_info->pci_bars[i].base_addr);
		PRINTK("       Length %lu bytes", pci_info->pci_bars[i].size);
	}	
	PRINTK("IRQ_LEVEL: %i ", pdev->irq);


	/* Allocate the Shared memory for use with the FPGA */
	if (ent->device == PCIEXT_DEVICE_ID)
	{
		/* Initialisation Block */
		if (pdev == NULL || pci_info == NULL) 
		{
			EPRINTK ("Can't allocate on the PCI peripheral PCI pointer is NULL.");
			return -ENOMEM;
		}
		// Allocate memory for timer info
		pci_info->kern_timer_ptr = kzalloc(((sizeof(struct timer_info_s)/PAGE_SIZE)+3)*PAGE_SIZE, GFP_KERNEL);
       	if(pci_info->kern_timer_ptr == NULL){
            EPRINTK("failed to allocate memory for device structure %d",i);
			return -ENOMEM; 
        }
        pci_info->kern_timer_va = (int *)((((unsigned long)pci_info->kern_timer_ptr) + PAGE_SIZE - 1) & PAGE_MASK);
		SetPageReserved(virt_to_page(((unsigned long)pci_info->kern_timer_va))); // On Page boundry

        // increment i to indicate next area
        i++;

		// initialize values of the timer_info struct
		timer_info = (struct timer_info_s *)pci_info->kern_timer_va;	
		timer_info->period = 110;
		timer_info->latency_min = 111;
		timer_info->latency_avg = 112;
		timer_info->latency_max = 113;

	    /* set buffer size for storing latency measurements [JMW] */  
	    timer_info->buffer_size = LATENCY_BUFFER_SIZE;

        PRINTK("timer_info struct initialized successfully");
#ifdef DEBUG_MMAP_WORKING
        // Allocate memory for largeBuffer
		pci_info->kern_buffer_ptr = kzalloc(sizeof(uint64_t)*BIG_BUFFER_SIZE, GFP_KERNEL); // was GFP_ATOMIC | __GFP_NOFAIL
	  	if(pci_info->kern_buffer_ptr == NULL){                            
            EPRINTK("failed to allocate memory for device structure %d",i); 
       	    return -ENOMEM;                                                  
        }
        pci_info->kern_buffer_va = (int *)((((unsigned long)pci_info->kern_buffer_ptr) + PAGE_SIZE - 1) & PAGE_MASK);
		SetPageReserved(virt_to_page(((unsigned long)pci_info->kern_buffer_va))); // On Page boundry

        largeBuffer = (uint64_t *)pci_info->kern_buffer_va;
        // initialize values of the array to zero with memset
#endif
	}

	/* Now Bind the IRQ */
	PRINTK("Binding PCI Conf IRQ");

	if (bind_irq(&pci_info->irq))
	{
		EPRINTK("Unable to Register FPGA IRQ");
		return -EINTR;
	}

	/* Add  device to table */
	if (new_device) 
	{
		pciext_device_table_add(pciextLogicalDevice);
	}
	return status;
}

/* Checks if the driver is configured to manage the given PCI device */
static struct pciext_dev_s *pciext_table_check(struct pci_dev *pdev)
{
	struct pciext_dev_s *ptr = pciext_table;
	while (ptr != NULL) 
	{
		if (ptr->device.pdev == pdev) 
			return ptr;
		ptr = ptr->pnext;
	}
	return ptr;
}

/* Retrieves a device, given the DEVICE_ID, from the devices table */
static struct pciext_dev_s *pciext_table_get_device(unsigned int device_id)
{
	struct pciext_dev_s *ptr = pciext_table;

	/*
	 *check for existing device
	 */
	while (ptr != NULL) 
	{
		if (ptr->device.device_id == device_id) 
			return ptr;
		ptr = ptr->pnext;
	}
	return NULL;
}

/* Gets the number of devices going through the devices table */
static unsigned int pciext_table_get_device_count()
{
	struct pciext_dev_s *ptr = pciext_table;
	unsigned int i = 0;

/*
*check for existing device
*/
	while (ptr != NULL)
	{
		ptr = ptr->pnext;
		i++;
	}
	return i;
}

/* Add a device to the devices table*/
static void pciext_device_table_add(struct pciext_dev_s *pciext_dev)
{
	PRINTK("Adding deviceID %x",(unsigned int) 	pciext_dev->device.device_id);
	if (pciext_dev == NULL) 
	{
		EPRINTK("NULL pointer provided");
		return;
	}

	if (pciext_table == NULL) 
	{
		pciext_table = pciext_dev;
		pciext_dev->pnext = NULL;
		pciext_dev->pprev = NULL;
	} else 
	{
		pciext_dev->pnext = pciext_table;
		pciext_table->pprev = pciext_dev;
		pciext_dev->pprev = NULL;
		pciext_table = pciext_dev;
	}

}

/* Remove a device from the devices table*/
static int pciext_device_table_remove(struct pciext_dev_s *pciext_dev)
{
	struct pciext_dev_s *prev = NULL;
	struct pciext_dev_s *next = NULL;

	if (!pciext_dev) 
		return -ENODEV;

	prev = pciext_dev->pprev;
	next = pciext_dev->pnext;

	if (prev) 
	{
		prev->pnext = pciext_dev->pnext;
		if (next)
			next->pprev = prev;
	} else 
	{
		pciext_table = pciext_dev->pnext;
		if (next)
			next->pprev = NULL;
	}
	return 0;
}


/*
 * Remove a PCI device
 */
static void pciext_remove(struct pci_dev *pdev)
{
	struct pciext_dev_s *pciext_dev = NULL;
	int i = 0;

	/* Find the device structure */
	pciext_dev = pciext_table_check(pdev);
	if (pciext_dev == NULL) 
	{
		EPRINTK("failed to find pciext_dev");
		return;
	}

	for (i = 0; i < pciext_dev->device.num_bars; i++) {
		iounmap(pciext_dev->device.pci_bars[i].virt_addr);
	}
	PRINTK("Unmapped 0x%x BARS",(unsigned int)pciext_dev->device.num_bars);
	pciext_dev->device.num_bars = 0;
	
	// clear reserved page for timer info
    ClearPageReserved(virt_to_page( (unsigned long)pciext_dev->device.kern_timer_va));
	kfree(pciext_dev->device.kern_timer_ptr);

#ifdef DEBUG_MMAP_WORKING
    // clear reserved pages for largeBuffer
    ClearPageReserved(virt_to_page( (unsigned long)pciext_dev->device.kern_buffer_va));
	kfree(pciext_dev->device.kern_buffer_ptr);
#endif

	pciext_cleanup_device(pciext_dev);

	PRINTK("Device 0x%x Removal Completed",(unsigned int)pciext_dev->device.device_id);
	return;
}

/*
 * Release the memory regions that belong to a PCI Device and disable
 * the device
 */
static void pciext_cleanup_device(struct pciext_dev_s *pciext_dev)
{
	int status = 0;

	if (pciext_dev == NULL) {
		EPRINTK("NULL pointer provided");
		return;
	}

	if (pciext_dev->device.pdev) 
	{
		pci_release_regions(pciext_dev->device.pdev);
		pci_disable_device(pciext_dev->device.pdev);
	}

	/* Remove the device from the device table */
	status = pciext_device_table_remove(pciext_dev);
	if (status != 0) {
		EPRINTK("failed to remove pciext_dev from device table");
	}

	kfree(pciext_dev);

	return;
}

static uint64_t readTimestampCounter(void)
{
	uint64_t a, d;
	asm volatile("rdtsc" : "=a" (a), "=d" (d));
	return (((uint64_t)a) | (((uint64_t)d) << 32)); 
}


/******************************************************************
 **	       INTERRUPTS				    ***
 ******************************************************************/

/*
 * Bind the interrupt handler for a slot
 */
int bind_irq(unsigned int * vector)
{
	if (request_irq (*vector, pciext_interrupt_handler, IRQF_SHARED|IRQF_NO_THREAD, "FPGA_DEV", vector))
	{
		EPRINTK("Failed to bind FPGA IRQ Level: %u", *vector);
		return -EPERM;
	}
	
    PRINTK("Bind IRQ Level successful: %u", *vector);

	return 0;
}

/*
 * Unbind the interrupt handler for a slot
 */
void unbind_irq(unsigned int *vector)
{
	free_irq(*vector, vector);
	PRINTK("Unbind IRQ Level successful: %u", *vector);
}

static void stop_timer(void)
{
	struct pciext_dev_s *pciext = NULL;

	pciext = pciext_table_get_device(PCIEXT_DEVICE_ID);

	if (pciext == NULL)
	{
		PRINTK("Null PCIe Device");
		return;
	}

	/*
	 * Issue a "stop" command to the FPGA card so it stops generating interrupts
	 * 0x0008 is the value and TIMER_CONTROL is the offset
	 */
	iowrite16(0x0008, pciext->device.pci_bars[2].virt_addr+TIMER_CONTROL);

	/*
	 * Set Qsys interrupt enable 0
	 * 0x0000 is the value and AVALON_INT_ENABLE is the offset
	 */
	iowrite16(0x0000, pciext->device.pci_bars[2].virt_addr+AVALON_INT_ENABLE);
}

/*
 * Interrupt handler for FPGA (ISR Top Half)
 */
static irqreturn_t pciext_interrupt_handler(int irg, void *dev_id)
{
	struct pciext_dev_s *pciext = NULL;
	struct timer_info_s *timer_info;
    uint32_t latency;
	uint64_t timestamp;
    uint64_t index;
    uint64_t num_interrupts = 0;
#ifdef SPIKE_DETECTION
	const uint32_t usFPGA = 125; // ==125E6 * 1E-6
    const uint32_t MAX_SPIKE_SIZE = 300 * usFPGA;
#endif
#ifdef DEBUG_MMAP_WORKING
//    int bigBuffSize = BIG_BUFFER_SIZE;
	uint64_t *largeBuffer;
    uint64_t index2;
#endif
	/* CPRI_RX function */
	pciext = pciext_table_get_device(PCIEXT_DEVICE_ID);
	if (pciext == NULL)
	{
		PRINTK("Null PCIe Device");
		return IRQ_NONE;
	}
	// Trigger FPGA timer snapshot
	iowrite16(0,pciext->device.pci_bars[2].virt_addr+TIMER_SNAPL); 
	// Reset interrupt;
	iowrite16(0,pciext->device.pci_bars[2].virt_addr+0x5000);

	// get timer_info from kernel memory
	timer_info = pciext->device.kern_timer_va;

    // Latency since last FPGA interrupt
 	latency = timer_info->period-(  ioread16(pciext->device.pci_bars[2].virt_addr+TIMER_SNAPL)+
                            (ioread16(pciext->device.pci_bars[2].virt_addr+TIMER_SNAPH)<<16));
	
	// Update jitter
	if ( unlikely(timer_info->last_timestamp == 0) )
	{
		timer_info->last_timestamp = timestamp = readTimestampCounter();
		timer_info->no_interrupts=0;
		timer_info->jitter_avg = 0;
		timer_info->latency_avg = 0;
	}
	else
	{
		timestamp = readTimestampCounter()-timer_info->last_timestamp;
		if (timestamp > timer_info->jitter_max)
			timer_info->jitter_max = timestamp;
		if (timestamp < timer_info->jitter_min)
			timer_info->jitter_min = timestamp;
		timer_info->last_timestamp += timestamp;
		timer_info->jitter_avg +=timestamp;
	}

	// Update latency statistics
    if (latency > timer_info->latency_max)
		timer_info->latency_max = latency;
    if (latency < timer_info->latency_min)
		timer_info->latency_min = latency;
	timer_info->latency_avg +=latency;

	// Check if latency spikes above limit
#ifdef SPIKE_DETECTION
    // spike is statistically unlikely so mark it to prevent branch prediction
	if ( unlikely(latency > MAX_SPIKE_SIZE) ){
		// turn off tracing on the host
        tracing_off();
        
        // issue a "stop" command to the FPGA card so it stops generating interrupts 
        // 0x0008 is the value and TIMER_CONTROL is the offset
	    iowrite16(0x0008, pciext->device.pci_bars[2].virt_addr+TIMER_CONTROL); 

        // Set Qsys interrupt enable 0
        // 0x0000 is the value and AVALON_INT_ENABLE is the offset
        iowrite16(0x0000, pciext->device.pci_bars[2].virt_addr+AVALON_INT_ENABLE);  
        
        // the following line issues an unconditional kernel printk to mark the spike 
        printk(KERN_WARNING "ILatIntDrv: %s:%d: FPGA latency spike detected (%d us) -> tracing_off\n", __FUNCTION__,__LINE__,latency/usFPGA);
//		PRINTK("FPGA Latency spike detected: %d us.  Tracing is now off.", latency/usFPGA );
	}
#endif

    num_interrupts = timer_info->no_interrupts;

    /* append latency measurements to buffer in a circular fashion */
    //index = num_interrupts % timer_info->buffer_size;
    index = (num_interrupts-1) % LATENCY_BUFFER_SIZE;
    timer_info->latency_buffer[index] = latency;

#ifdef DEBUG_MMAP_WORKING
    // get big_buffer from kernel memory
 	largeBuffer = pciext->device.kern_buffer_va;
 	
	// append latency to largeBuffer in a circular fashion
	index2 = (num_interrupts-1) % BIG_BUFFER_SIZE;
	largeBuffer[index2] = latency;
#endif

    /* update the number of interrupts performed */
	timer_info->no_interrupts++;

	if (spike_threshold && latency >= (spike_threshold * US_FPGA)) { /* 15us period based on 125Mhz counter */
		stop_host_tracing();

		stop_timer();

        	printk(KERN_WARNING "ILatIntDrv: %s:%d: FPGA latency spike detected (%d us) -> tracing_off\n", __FUNCTION__, __LINE__, latency/US_FPGA);
	}
	return IRQ_HANDLED;
//	return IRQ_NONE;

}

module_init(pciext_init_module);
module_exit(pciext_cleanup_module);


MODULE_DESCRIPTION("Intel PCIe Interrupt Testing Driver");
MODULE_AUTHOR("Intel Corporation");
MODULE_VERSION("0.1.1");
MODULE_LICENSE("GPL");
MODULE_DEVICE_TABLE(pci, pciext_pci_tbl);
