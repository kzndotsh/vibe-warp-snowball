/*
 * Blue Snowball USB Audio Driver
 * 
 * This driver handles the Blue Snowball USB microphone
 * Vendor ID: 0x0d8c, Product ID: 0x0013
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/usb.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/mutex.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/initval.h>

#define DRIVER_NAME "blue_snowball"
#define DRIVER_VERSION "1.0.0"
#define DRIVER_DESCRIPTION "Blue Snowball USB Audio Driver"

/* Blue Snowball USB IDs */
#define BLUE_SNOWBALL_VENDOR_ID  0x0d8c
#define BLUE_SNOWBALL_PRODUCT_ID 0x0013

/* Audio format constants */
#define BLUE_SNOWBALL_SAMPLE_RATE 44100
#define BLUE_SNOWBALL_CHANNELS 1
#define BLUE_SNOWBALL_BITS_PER_SAMPLE 16
#define BLUE_SNOWBALL_FRAME_SIZE 2
#define BLUE_SNOWBALL_BUFFER_SIZE (BLUE_SNOWBALL_SAMPLE_RATE * BLUE_SNOWBALL_FRAME_SIZE)

/* Device structure */
struct blue_snowball_device {
    struct usb_device *udev;
    struct usb_interface *interface;
    struct snd_card *card;
    struct snd_pcm *pcm;
    struct mutex mutex;
    
    /* USB endpoints */
    struct usb_endpoint_descriptor *audio_in_endpoint;
    struct usb_endpoint_descriptor *audio_out_endpoint;
    
    /* Audio buffer */
    unsigned char *audio_buffer;
    size_t buffer_size;
    
    /* Device state */
    int active;
    int card_index;
};

/* PCM substream private data */
struct blue_snowball_substream {
    struct blue_snowball_device *device;
    struct snd_pcm_substream *substream;
    struct urb *urb;
    unsigned char *buffer;
    size_t buffer_size;
    size_t buffer_pos;
};

/* USB device table */
static struct usb_device_id blue_snowball_table[] = {
    { USB_DEVICE(BLUE_SNOWBALL_VENDOR_ID, BLUE_SNOWBALL_PRODUCT_ID) },
    { } /* Terminating entry */
};
MODULE_DEVICE_TABLE(usb, blue_snowball_table);

/* Forward declarations */
static int blue_snowball_probe(struct usb_interface *interface, const struct usb_device_id *id);
static void blue_snowball_disconnect(struct usb_interface *interface);
static int blue_snowball_suspend(struct usb_interface *interface, pm_message_t message);
static int blue_snowball_resume(struct usb_interface *interface);

/* USB driver structure */
static struct usb_driver blue_snowball_driver = {
    .name = DRIVER_NAME,
    .probe = blue_snowball_probe,
    .disconnect = blue_snowball_disconnect,
    .suspend = blue_snowball_suspend,
    .resume = blue_snowball_resume,
    .id_table = blue_snowball_table,
};

/* Audio URB completion callback */
static void blue_snowball_urb_complete(struct urb *urb)
{
    struct blue_snowball_substream *subs = urb->context;
    struct snd_pcm_substream *substream = subs->substream;
    struct snd_pcm_runtime *runtime = substream->runtime;
    int ret;

    if (urb->status == 0) {
        /* Copy audio data to ALSA buffer */
        if (subs->buffer_pos + urb->actual_length <= runtime->buffer_size) {
            memcpy(runtime->dma_area + subs->buffer_pos, urb->transfer_buffer, urb->actual_length);
            subs->buffer_pos += urb->actual_length;
            
            /* Notify ALSA of new data */
            snd_pcm_period_elapsed(substream);
        }
    }

    /* Resubmit URB if still active */
    if (subs->device->active) {
        ret = usb_submit_urb(urb, GFP_ATOMIC);
        if (ret < 0) {
            dev_err(&subs->device->udev->dev, "Failed to resubmit URB: %d\n", ret);
        }
    }
}

/* PCM hardware definition */
static struct snd_pcm_hardware blue_snowball_pcm_hw = {
    .info = SNDRV_PCM_INFO_MMAP |
            SNDRV_PCM_INFO_INTERLEAVED |
            SNDRV_PCM_INFO_BLOCK_TRANSFER |
            SNDRV_PCM_INFO_MMAP_VALID,
    .formats = SNDRV_PCM_FMTBIT_S16_LE,
    .rates = SNDRV_PCM_RATE_44100,
    .rate_min = 44100,
    .rate_max = 44100,
    .channels_min = 1,
    .channels_max = 1,
    .buffer_bytes_max = BLUE_SNOWBALL_BUFFER_SIZE,
    .period_bytes_min = 1024,
    .period_bytes_max = BLUE_SNOWBALL_BUFFER_SIZE / 4,
    .periods_min = 2,
    .periods_max = 4,
};

/* PCM operations */
static int blue_snowball_pcm_open(struct snd_pcm_substream *substream)
{
    struct blue_snowball_device *device = snd_pcm_substream_chip(substream);
    struct blue_snowball_substream *subs;
    struct snd_pcm_runtime *runtime = substream->runtime;
    int ret;

    subs = kzalloc(sizeof(*subs), GFP_KERNEL);
    if (!subs)
        return -ENOMEM;

    subs->device = device;
    subs->substream = substream;
    runtime->private_data = subs;
    runtime->hw = blue_snowball_pcm_hw;

    ret = snd_pcm_hw_constraint_integer(runtime, SNDRV_PCM_HW_PARAM_PERIODS);
    if (ret < 0) {
        kfree(subs);
        return ret;
    }

    return 0;
}

static int blue_snowball_pcm_close(struct snd_pcm_substream *substream)
{
    struct blue_snowball_substream *subs = substream->runtime->private_data;

    if (subs) {
        if (subs->urb) {
            usb_kill_urb(subs->urb);
            usb_free_urb(subs->urb);
        }
        kfree(subs->buffer);
        kfree(subs);
    }

    return 0;
}

static int blue_snowball_pcm_hw_params(struct snd_pcm_substream *substream,
                                      struct snd_pcm_hw_params *hw_params)
{
    return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
}

static int blue_snowball_pcm_hw_free(struct snd_pcm_substream *substream)
{
    return snd_pcm_lib_free_pages(substream);
}

static int blue_snowball_pcm_prepare(struct snd_pcm_substream *substream)
{
    struct blue_snowball_substream *subs = substream->runtime->private_data;
    struct blue_snowball_device *device = subs->device;

    /* Allocate URB and buffer */
    subs->urb = usb_alloc_urb(0, GFP_KERNEL);
    if (!subs->urb)
        return -ENOMEM;

    subs->buffer_size = 1024; /* Adjust as needed */
    subs->buffer = kmalloc(subs->buffer_size, GFP_KERNEL);
    if (!subs->buffer) {
        usb_free_urb(subs->urb);
        return -ENOMEM;
    }

    /* Initialize URB */
    usb_fill_bulk_urb(subs->urb, device->udev,
                      usb_rcvbulkpipe(device->udev, device->audio_in_endpoint->bEndpointAddress),
                      subs->buffer, subs->buffer_size,
                      blue_snowball_urb_complete, subs);

    subs->buffer_pos = 0;
    return 0;
}

static int blue_snowball_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
    struct blue_snowball_substream *subs = substream->runtime->private_data;
    struct blue_snowball_device *device = subs->device;
    int ret = 0;

    switch (cmd) {
    case SNDRV_PCM_TRIGGER_START:
        device->active = 1;
        ret = usb_submit_urb(subs->urb, GFP_KERNEL);
        if (ret < 0) {
            dev_err(&device->udev->dev, "Failed to submit URB: %d\n", ret);
            device->active = 0;
        }
        break;
    case SNDRV_PCM_TRIGGER_STOP:
        device->active = 0;
        usb_kill_urb(subs->urb);
        break;
    default:
        ret = -EINVAL;
        break;
    }

    return ret;
}

static snd_pcm_uframes_t blue_snowball_pcm_pointer(struct snd_pcm_substream *substream)
{
    struct blue_snowball_substream *subs = substream->runtime->private_data;
    return bytes_to_frames(substream->runtime, subs->buffer_pos);
}

static struct snd_pcm_ops blue_snowball_pcm_ops = {
    .open = blue_snowball_pcm_open,
    .close = blue_snowball_pcm_close,
    .ioctl = snd_pcm_lib_ioctl,
    .hw_params = blue_snowball_pcm_hw_params,
    .hw_free = blue_snowball_pcm_hw_free,
    .prepare = blue_snowball_pcm_prepare,
    .trigger = blue_snowball_pcm_trigger,
    .pointer = blue_snowball_pcm_pointer,
};

/* Create ALSA card and PCM device */
static int blue_snowball_create_card(struct blue_snowball_device *device)
{
    struct snd_card *card;
    struct snd_pcm *pcm;
    int ret;

    /* Create ALSA card */
    ret = snd_card_new(&device->interface->dev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1,
                       THIS_MODULE, 0, &card);
    if (ret < 0)
        return ret;

    device->card = card;
    device->card_index = card->number;

    /* Set card info */
    strcpy(card->driver, DRIVER_NAME);
    strcpy(card->shortname, "Blue Snowball");
    sprintf(card->longname, "Blue Snowball USB Microphone");

    /* Create PCM device */
    ret = snd_pcm_new(card, "Blue Snowball PCM", 0, 0, 1, &pcm);
    if (ret < 0) {
        snd_card_free(card);
        return ret;
    }

    device->pcm = pcm;
    pcm->private_data = device;
    strcpy(pcm->name, "Blue Snowball PCM");

    /* Set PCM operations */
    snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &blue_snowball_pcm_ops);

    /* Allocate DMA buffer */
    snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_CONTINUOUS,
                                          NULL,
                                          BLUE_SNOWBALL_BUFFER_SIZE, BLUE_SNOWBALL_BUFFER_SIZE);

    /* Register card */
    ret = snd_card_register(card);
    if (ret < 0) {
        snd_card_free(card);
        return ret;
    }

    dev_info(&device->udev->dev, "ALSA card created: %s\n", card->longname);
    return 0;
}

/* USB probe function */
static int blue_snowball_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
    struct usb_device *udev = interface_to_usbdev(interface);
    struct blue_snowball_device *device;
    struct usb_host_interface *altsetting;
    struct usb_endpoint_descriptor *endpoint;
    int i, ret;

    dev_info(&interface->dev, "Blue Snowball device detected\n");

    /* Allocate device structure */
    device = kzalloc(sizeof(*device), GFP_KERNEL);
    if (!device)
        return -ENOMEM;

    device->udev = udev;
    device->interface = interface;
    mutex_init(&device->mutex);

    /* Find audio endpoints */
    altsetting = interface->cur_altsetting;
    for (i = 0; i < altsetting->desc.bNumEndpoints; i++) {
        endpoint = &altsetting->endpoint[i].desc;
        
        if (usb_endpoint_is_bulk_in(endpoint)) {
            device->audio_in_endpoint = endpoint;
            dev_info(&interface->dev, "Found bulk IN endpoint: 0x%02x\n", 
                     endpoint->bEndpointAddress);
        }
        
        if (usb_endpoint_is_bulk_out(endpoint)) {
            device->audio_out_endpoint = endpoint;
            dev_info(&interface->dev, "Found bulk OUT endpoint: 0x%02x\n", 
                     endpoint->bEndpointAddress);
        }
    }

    if (!device->audio_in_endpoint) {
        dev_err(&interface->dev, "No audio input endpoint found\n");
        kfree(device);
        return -ENODEV;
    }

    /* Create ALSA card */
    ret = blue_snowball_create_card(device);
    if (ret < 0) {
        dev_err(&interface->dev, "Failed to create ALSA card: %d\n", ret);
        kfree(device);
        return ret;
    }

    /* Store device pointer in interface */
    usb_set_intfdata(interface, device);

    dev_info(&interface->dev, "Blue Snowball driver loaded successfully\n");
    return 0;
}

/* USB disconnect function */
static void blue_snowball_disconnect(struct usb_interface *interface)
{
    struct blue_snowball_device *device = usb_get_intfdata(interface);

    if (device) {
        device->active = 0;
        
        if (device->card) {
            snd_card_disconnect(device->card);
            snd_card_free_when_closed(device->card);
        }
        
        kfree(device->audio_buffer);
        kfree(device);
    }

    usb_set_intfdata(interface, NULL);
    dev_info(&interface->dev, "Blue Snowball driver unloaded\n");
}

/* USB suspend function */
static int blue_snowball_suspend(struct usb_interface *interface, pm_message_t message)
{
    struct blue_snowball_device *device = usb_get_intfdata(interface);
    
    if (device) {
        device->active = 0;
    }
    
    return 0;
}

/* USB resume function */
static int blue_snowball_resume(struct usb_interface *interface)
{
    /* Device will be reactivated when audio stream starts */
    return 0;
}

/* Module initialization */
static int __init blue_snowball_init(void)
{
    int ret;

    printk(KERN_INFO "Blue Snowball USB Audio Driver v%s\n", DRIVER_VERSION);
    
    ret = usb_register(&blue_snowball_driver);
    if (ret < 0) {
        printk(KERN_ERR "Failed to register Blue Snowball driver: %d\n", ret);
        return ret;
    }

    return 0;
}

/* Module cleanup */
static void __exit blue_snowball_exit(void)
{
    usb_deregister(&blue_snowball_driver);
    printk(KERN_INFO "Blue Snowball USB Audio Driver unloaded\n");
}

module_init(blue_snowball_init);
module_exit(blue_snowball_exit);

MODULE_AUTHOR("Kernel Developer");
MODULE_DESCRIPTION(DRIVER_DESCRIPTION);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");
