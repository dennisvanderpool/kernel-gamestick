/* Copyright Statement:
 *
 * This software/firmware and related documentation ("MediaTek Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to MediaTek Inc. and/or its licensors. Without
 * the prior written permission of MediaTek inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of MediaTek Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 * 
 * MediaTek Inc. (C) 2010. All rights reserved.
 * 
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("MEDIATEK SOFTWARE")
 * RECEIVED FROM MEDIATEK AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. MEDIATEK EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES MEDIATEK PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE MEDIATEK SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN MEDIATEK
 * SOFTWARE. MEDIATEK SHALL ALSO NOT BE RESPONSIBLE FOR ANY MEDIATEK SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND MEDIATEK'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE MEDIATEK SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT MEDIATEK'S OPTION, TO REVISE OR REPLACE THE
 * MEDIATEK SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO MEDIATEK FOR SUCH MEDIATEK SOFTWARE AT ISSUE.
 *
 * The following software/firmware and/or related documentation ("MediaTek
 * Software") have been modified by MediaTek Inc. All revisions are subject to
 * any receiver's applicable license agreements with MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include "bt_hwctl.h"

/**************************************************************************
 *                        D E F I N I T I O N S                           *
***************************************************************************/

#define BTHWCTL_NAME                 "bthwctl"
#define BTHWCTL_DEV_NAME             "/dev/bthwctl"
#define BTHWCTL_IOC_MAGIC            0xf6
#define BTHWCTL_IOCTL_SET_POWER      _IOWR(BTHWCTL_IOC_MAGIC, 0, uint32_t)
#define BTHWCTL_IOCTL_SET_EINT       _IOWR(BTHWCTL_IOC_MAGIC, 1, uint32_t)

wait_queue_head_t eint_wait;
int eint_gen;
int eint_mask;

struct bt_hwctl {
    bool powerup;
    dev_t dev_t;
    struct class *cls;
    struct device *dev;
    struct cdev cdev;
    struct mutex sem;
};
static struct bt_hwctl *bh = NULL;

/*****************************************************************************
 *  bt_hwctl_open
*****************************************************************************/
static int bt_hwctl_open(struct inode *inode, struct file *file)
{
    BT_HWCTL_DBG("bt_hwctl_open\n");
    eint_gen = 0;
    eint_mask = 0;
    return 0;
}

/*****************************************************************************
 *  bt_hwctl_ioctl
*****************************************************************************/
static long bt_hwctl_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
     
    BT_HWCTL_DBG("bt_hwctl_ioctl\n");
    
    if(!bh) {
        BT_HWCTL_ERR("bt_hwctl struct not initialized\n");
        return -EFAULT;
    }
    
    switch(cmd)
    {
        case BTHWCTL_IOCTL_SET_POWER:
        {
            unsigned long pwr = 0;
            if (copy_from_user(&pwr, (void*)arg, sizeof(unsigned long)))
                return -EFAULT;
                
            BT_HWCTL_DBG("BTHWCTL_IOCTL_SET_POWER: %d\n", (int)pwr);
            
            mutex_lock(&bh->sem);
            if (pwr){
                ret = mt_bt_power_on();
            }
            else{
                mt_bt_power_off();
            }
            mutex_unlock(&bh->sem);
            
            break;
        }
        case BTHWCTL_IOCTL_SET_EINT:
        {
            unsigned long eint = 0;
            if (copy_from_user(&eint, (void*)arg, sizeof(unsigned long)))
                return -EFAULT;
                
            BT_HWCTL_DBG("BTHWCTL_IOCTL_SET_EINT: %d\n", (int)eint);
            
            mutex_lock(&bh->sem);
            if (eint){
                /* Enable irq from user space */ 
                BT_HWCTL_DBG("Set BT EINT enable\n");
                mt_bt_enable_irq();
            }
            else{
                /* Disable irq from user space, maybe time to close driver */
                BT_HWCTL_DBG("Set BT EINT disable\n");
                mt_bt_disable_irq();
                eint_mask = 1;
                wake_up_interruptible(&eint_wait);
            }
            mutex_unlock(&bh->sem);
            
            break;
        }    
        default:
            BT_HWCTL_ALERT("BTHWCTL_IOCTL not support\n");
            return -EPERM;
    }
    
    return ret;
}

/*****************************************************************************
 *  bt_hwctl_release
*****************************************************************************/
static int bt_hwctl_release(struct inode *inode, struct file *file)
{
    BT_HWCTL_DBG("bt_hwctl_release\n");
    eint_gen = 0;
    eint_mask = 0;
    return 0;
}

/*****************************************************************************
 *  bt_hwctl_poll
*****************************************************************************/
static unsigned int bt_hwctl_poll(struct file *file, poll_table *wait)
{
    uint32_t mask = 0;
    
    BT_HWCTL_DBG("bt_hwctl_poll eint_gen %d, eint_mask %d ++\n", eint_gen, eint_mask);
    //poll_wait(file, &eint_wait, wait);
    wait_event_interruptible(eint_wait, (eint_gen == 1 || eint_mask == 1));
    BT_HWCTL_DBG("bt_hwctl_poll eint_gen %d, eint_mask %d --\n", eint_gen, eint_mask);
    
    if(eint_gen == 1){
        mask = POLLIN|POLLRDNORM;
        eint_gen = 0;
    }
    else if (eint_mask == 1){
        mask = POLLERR;
        eint_mask = 0;
    }
    
    return mask;
}

/**************************************************************************
 *                K E R N E L   I N T E R F A C E S                       *
***************************************************************************/
static struct file_operations bt_hwctl_fops = {
    .owner      = THIS_MODULE,
//    .ioctl      = bt_hwctl_ioctl,
    .unlocked_ioctl = bt_hwctl_ioctl,
    .open       = bt_hwctl_open,
    .release    = bt_hwctl_release,
    .poll       = bt_hwctl_poll,
};

/*****************************************************************************/
static int __init bt_hwctl_init(void)
{
    int ret = -1, err = -1;

    if (!(bh = kzalloc(sizeof(struct bt_hwctl), GFP_KERNEL)))
    {
        BT_HWCTL_ERR("bt_hwctl_init allocate dev struct failed\n");
        err = -ENOMEM;
        goto ERR_EXIT;
    }
    
    ret = alloc_chrdev_region(&bh->dev_t, 0, 1, BTHWCTL_NAME);
    if (ret) {
        BT_HWCTL_ERR("alloc chrdev region failed\n");
        goto ERR_EXIT;
    }
    
    BT_HWCTL_INFO("alloc %s: %d:%d\n", BTHWCTL_NAME, MAJOR(bh->dev_t), MINOR(bh->dev_t));
    
    cdev_init(&bh->cdev, &bt_hwctl_fops);
    
    bh->cdev.owner = THIS_MODULE;
    bh->cdev.ops = &bt_hwctl_fops;
    
    err = cdev_add(&bh->cdev, bh->dev_t, 1);
    if (err) {
        BT_HWCTL_ERR("add chrdev failed\n");
        goto ERR_EXIT;
    }
    
    bh->cls = class_create(THIS_MODULE, BTHWCTL_NAME);
    if (IS_ERR(bh->cls)) {
        err = PTR_ERR(bh->cls);
        BT_HWCTL_ERR("class_create failed, errno:%d\n", err);
        goto ERR_EXIT;
    }
    
    bh->dev = device_create(bh->cls, NULL, bh->dev_t, NULL, BTHWCTL_NAME);
    mutex_init(&bh->sem);
    
    init_waitqueue_head(&eint_wait);
    /* request gpio used by BT */
    mt_bt_gpio_init();
    
    BT_HWCTL_INFO("Bluetooth hardware control driver initialized\n");
    
    return 0;
    
ERR_EXIT:
    if (err == 0)
        cdev_del(&bh->cdev);
    if (ret == 0)
        unregister_chrdev_region(bh->dev_t, 1);
        
    if (bh){
        kfree(bh);
        bh = NULL;
    }     
    return -1;
}

/*****************************************************************************/
static void __exit bt_hwctl_exit(void)
{    
    if (bh){
        cdev_del(&bh->cdev);
        
        unregister_chrdev_region(bh->dev_t, 1);
        device_destroy(bh->cls, bh->dev_t);
        
        class_destroy(bh->cls);
        mutex_destroy(&bh->sem);
        
        kfree(bh);
        bh = NULL;
    }
    
    /* release gpio used by BT */
    mt_bt_gpio_release();
}

EXPORT_SYMBOL(eint_wait);
EXPORT_SYMBOL(eint_gen);

module_init(bt_hwctl_init);
module_exit(bt_hwctl_exit);
MODULE_AUTHOR("Tingting Lei <tingting.lei@mediatek.com>");
MODULE_DESCRIPTION("Bluetooth hardware control driver");
MODULE_LICENSE("GPL");
