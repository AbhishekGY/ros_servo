#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/uaccess.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/tty_driver.h>
#include <linux/kfifo.h>

#define DEVICE_NAME "servo"
#define CLASS_NAME "servo_class"
#define FIFO_SIZE 256
#define MAX_SERVO_ANGLE 180
#define N_SERVO 20  // Define our line discipline number

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Servo driver for tty0tty communication");
MODULE_VERSION("0.1");

static int major_number;
static struct class* servo_class = NULL;
static struct device* servo_device = NULL;
static DEFINE_MUTEX(servo_mutex);
static DECLARE_WAIT_QUEUE_HEAD(servo_wait);

// TTY port for communication
static struct tty_struct *servo_tty;
static int servo_angle = 0;
static int data_available = 0;

// Function prototypes
static int dev_open(struct inode *, struct file *);
static int dev_release(struct inode *, struct file *);
static ssize_t dev_read(struct file *, char *, size_t, loff_t *);
static ssize_t dev_write(struct file *, const char *, size_t, loff_t *);

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};


// TTY operations
static void servo_receive_buf(struct tty_struct *tty, const unsigned char *cp,
                            const char *fp, int count)
{
    static char buffer[16];
    static int buf_pos = 0;
    int new_angle;
    int i;

    mutex_lock(&servo_mutex);
    
    for (i = 0; i < count; i++) {
        char c = cp[i];
        
        if (c >= '0' && c <= '9' && buf_pos < 15) {
            buffer[buf_pos++] = c;
        }
        else if (c == '\n' || c == '\r') {
            if (buf_pos > 0) {
                buffer[buf_pos] = '\0';
                if (kstrtoint(buffer, 10, &new_angle) == 0) {
                    if (new_angle >= 0 && new_angle <= 180) {
                        servo_angle = new_angle;
                        data_available = 1;
                        wake_up_interruptible(&servo_wait);
                    }
                }
            }
            buf_pos = 0;
        }
    }
    
    mutex_unlock(&servo_mutex);
}

static int servo_ldisc_open(struct tty_struct *tty) 
{
    tty->receive_room = 65536;
    return 0;
}
static void servo_ldisc_close(struct tty_struct *tty)
{
    pr_info("servo_ldisc_close: tty device closed\n");
}

static struct tty_ldisc_ops servo_ldisc = {
    .name         = "servo",
    .owner        = THIS_MODULE,
    .num          = N_SERVO,
    .open         = servo_ldisc_open,
    .close        = servo_ldisc_close,
    .receive_buf  = servo_receive_buf
};

// Device file operations
static int dev_open(struct inode *inodep, struct file *filep)
{
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep)
{
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset)
{
    char angle_str[8];
    int bytes_written;
    int ret;
    
    ret = wait_event_interruptible(servo_wait, data_available);
    if (ret)
        return ret;
        
    data_available = 0;  // Reset the flag
    
    bytes_written = snprintf(angle_str, sizeof(angle_str), "%d\n", servo_angle);
    
    if (bytes_written > len) {
        bytes_written = len;
    }
    
    if (copy_to_user(buffer, angle_str, bytes_written)) {
        return -EFAULT;
    }
    
    return bytes_written;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset)
{
    char kernel_buffer[FIFO_SIZE];
    int ret;
    int new_angle;

    if (len > FIFO_SIZE - 1)
        return -EINVAL;

    ret = copy_from_user(kernel_buffer, buffer, len);
    if (ret) {
        pr_err("Failed to receive data from user\n");
        return -EFAULT;
    }

    kernel_buffer[len] = '\0';
    if (kstrtoint(kernel_buffer, 10, &new_angle) == 0) {
        if (new_angle >= 0 && new_angle <= MAX_SERVO_ANGLE) {
            // Send to tty0tty
            if (servo_tty && servo_tty->ops->write) {
                char cmd[16];
                int cmd_len = snprintf(cmd, sizeof(cmd), "%d\n", new_angle);
                servo_tty->ops->write(servo_tty, cmd, cmd_len);
            }
            servo_angle = new_angle;
            return len;
        }
    }

    return -EINVAL;
}


static int __init servo_driver_init(void)
{
    int ret;

    // Register line discipline first
    ret = tty_register_ldisc(&servo_ldisc);
    if (ret < 0) {
        pr_alert("Failed to register line discipline (error %d)\n", ret);
        return ret;
    }
    pr_info("Successfully registered line discipline %d\n", servo_ldisc.num);

    // Register character device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        pr_alert("Failed to register a major number\n");
        tty_unregister_ldisc(&servo_ldisc);
        return major_number;
    }
    pr_info("Registered character device with major number %d\n", major_number);
    
    // Register device class
    servo_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(servo_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        tty_unregister_ldisc(&servo_ldisc);
        pr_alert("Failed to register device class\n");
        return PTR_ERR(servo_class);
    }

    // Create device
    servo_device = device_create(servo_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(servo_device)) {
        class_destroy(servo_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        tty_unregister_ldisc(&servo_ldisc);
        pr_alert("Failed to create the device\n");
        return PTR_ERR(servo_device);
    }

    return 0;
}

static void __exit servo_driver_exit(void)
{
    tty_unregister_ldisc(&servo_ldisc);
    device_destroy(servo_class, MKDEV(major_number, 0));
    class_destroy(servo_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("Servo driver removed\n");
}
module_init(servo_driver_init);
module_exit(servo_driver_exit);
