//DRIVER ADS1115 ADC

#include <linux/init.h>
#include <linux/module.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#define ADS1115_MAJOR             160
#define N_ADS1115_MINORS          16
#define DEVICE_NAME               "ads1115"

#define ADS1115_CONV_REG          0x00
#define ADS1115_CONFIG_REG        0x01

#define ADS1115_OS_MASK           0x8000
#define ADS1115_PGA_MASK          0x0E00
#define ADS1115_MODE_MASK         0x0100

struct ioctl_data {
    int channel;
    int32_t voltage;
};

#define ADS1115_IOC_MAGIC         'a'
#define ADS1115_GET_VOLTAGE _IOWR(ADS1115_IOC_MAGIC, 1, struct ioctl_data)

struct ads1115_data {
    dev_t devt;
    struct mutex lock;
    struct i2c_client *client;
    struct list_head device_entry;
    unsigned users;
};

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);
static DECLARE_BITMAP(minors, N_ADS1115_MINORS);

static int ads1115_read_voltage(struct i2c_client *client, int channel)
{
    int ret;
    uint16_t config, conversion;
    uint8_t config_buf[3];
    uint8_t conv_buf[2];
    uint8_t conv_reg;

    config = ADS1115_OS_MASK | (channel << 12) | 0x0200 | ADS1115_MODE_MASK;

    config_buf[0] = ADS1115_CONFIG_REG;
    config_buf[1] = config >> 8;
    config_buf[2] = config & 0xFF;

    ret = i2c_master_send(client, config_buf, 3);
    if (ret < 0)
        return ret;

    msleep(10);

    conv_reg = ADS1115_CONV_REG;
    ret = i2c_master_send(client, &conv_reg, 1);
    if (ret < 0)
        return ret;

    ret = i2c_master_recv(client, conv_buf, 2);
    if (ret < 0)
        return ret;

    conversion = (conv_buf[0] << 8) | conv_buf[1];
    return (int32_t)((conversion * 4096) / 32767);
}

static long ads1115_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct ads1115_data *ads1115 = filp->private_data;
    struct i2c_client *client = ads1115->client;
    int retval = 0;
    int32_t voltage;
    struct ioctl_data data;

    mutex_lock(&ads1115->lock);

    if (!client) {
        retval = -ESHUTDOWN;
        goto out;
    }

    switch (cmd) {
    case ADS1115_GET_VOLTAGE:
        if (copy_from_user(&data, (struct ioctl_data __user *)arg, sizeof(data))) {
            retval = -EFAULT;
            break;
        }

        if (data.channel < 0 || data.channel > 3) {
            retval = -EINVAL;
            break;
        }

        voltage = ads1115_read_voltage(client, data.channel);
        if (voltage < 0) {
            retval = voltage;
            break;
        }

        data.voltage = voltage;

        if (copy_to_user((struct ioctl_data __user *)arg, &data, sizeof(data)))
            retval = -EFAULT;
        break;

    default:
        retval = -ENOTTY;
    }

out:
    mutex_unlock(&ads1115->lock);
    return retval;
}

static int ads1115_open(struct inode *inode, struct file *filp)
{
    struct ads1115_data *ads1115 = NULL, *iter;
    int status = -ENXIO;

    mutex_lock(&device_list_lock);
    list_for_each_entry(iter, &device_list, device_entry) {
        if (iter->devt == inode->i_rdev) {
            ads1115 = iter;
            status = 0;
            break;
        }
    }

    if (!ads1115) {
        mutex_unlock(&device_list_lock);
        return status;
    }

    ads1115->users++;
    filp->private_data = ads1115;
    nonseekable_open(inode, filp);

    mutex_unlock(&device_list_lock);
    return 0;
}

static int ads1115_release(struct inode *inode, struct file *filp)
{
    struct ads1115_data *ads1115;
    int dofree;

    mutex_lock(&device_list_lock);
    ads1115 = filp->private_data;
    filp->private_data = NULL;

    mutex_lock(&ads1115->lock);
    dofree = (ads1115->client == NULL);
    mutex_unlock(&ads1115->lock);

    ads1115->users--;
    if (!ads1115->users && dofree)
        kfree(ads1115);

    mutex_unlock(&device_list_lock);
    return 0;
}

static const struct file_operations ads1115_fops = {
    .owner = THIS_MODULE,
    .open = ads1115_open,
    .release = ads1115_release,
    .unlocked_ioctl = ads1115_ioctl,
};

static struct class ads1115_class = {
    .name = "ads1115",
};

static int ads1115_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct ads1115_data *ads1115;
    int status;
    unsigned long minor;

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        dev_err(&client->dev, "I2C functionality not supported\n");
        return -ENODEV;
    }

    ads1115 = kzalloc(sizeof(*ads1115), GFP_KERNEL);
    if (!ads1115)
        return -ENOMEM;

    ads1115->client = client;
    mutex_init(&ads1115->lock);
    INIT_LIST_HEAD(&ads1115->device_entry);

    mutex_lock(&device_list_lock);
    minor = find_first_zero_bit(minors, N_ADS1115_MINORS);
    if (minor < N_ADS1115_MINORS) {
        struct device *dev;
        ads1115->devt = MKDEV(ADS1115_MAJOR, minor);
        dev = device_create(&ads1115_class, &client->dev, ads1115->devt,
                            ads1115, "ads1115%lu", minor);
        status = PTR_ERR_OR_ZERO(dev);
    } else {
        status = -ENODEV;
    }

    if (status == 0) {
        set_bit(minor, minors);
        list_add(&ads1115->device_entry, &device_list);
    }
    mutex_unlock(&device_list_lock);

    if (status == 0)
        i2c_set_clientdata(client, ads1115);
    else
        kfree(ads1115);

    return status;
}

static void ads1115_remove(struct i2c_client *client)
{
    struct ads1115_data *ads1115 = i2c_get_clientdata(client);

    mutex_lock(&device_list_lock);
    mutex_lock(&ads1115->lock);
    ads1115->client = NULL;
    mutex_unlock(&ads1115->lock);

    list_del(&ads1115->device_entry);
    device_destroy(&ads1115_class, ads1115->devt);
    clear_bit(MINOR(ads1115->devt), minors);
    if (ads1115->users == 0)
        kfree(ads1115);
    mutex_unlock(&device_list_lock);
}

static const struct i2c_device_id ads1115_id[] = {
    { "ads1115", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, ads1115_id);

static const struct of_device_id ads1115_of_match[] = {
    { .compatible = "ti,ads1115" },
    { }
};
MODULE_DEVICE_TABLE(of, ads1115_of_match);

static struct i2c_driver ads1115_driver = {
    .driver = {
        .name = "ads1115",
        .of_match_table = ads1115_of_match,
    },
    .probe = ads1115_probe,
    .remove = ads1115_remove,
    .id_table = ads1115_id,
};

static int __init ads1115_init(void)
{
    int status;

    status = register_chrdev(ADS1115_MAJOR, DEVICE_NAME, &ads1115_fops);
    if (status < 0)
        return status;

    status = class_register(&ads1115_class);
    if (status) {
        unregister_chrdev(ADS1115_MAJOR, DEVICE_NAME);
        return status;
    }

    status = i2c_add_driver(&ads1115_driver);
    if (status < 0) {
        class_unregister(&ads1115_class);
        unregister_chrdev(ADS1115_MAJOR, DEVICE_NAME);
    }

    return status;
}
module_init(ads1115_init);

static void __exit ads1115_exit(void)
{
    i2c_del_driver(&ads1115_driver);
    class_unregister(&ads1115_class);
    unregister_chrdev(ADS1115_MAJOR, DEVICE_NAME);
}
module_exit(ads1115_exit);

MODULE_AUTHOR("TRANTIEN");
MODULE_DESCRIPTION("DRIVER ADS1115 ADC");
MODULE_LICENSE("GPL");
