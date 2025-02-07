#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/serial_core.h>
#include <linux/serial.h>
#include <linux/tty.h>
#include <linux/delay.h>

#define DEVICE_NAME "sram_control"
#define CLASS_NAME "sram"
#define BUFFER_SIZE 128

static int major_number;
static struct class* sram_class = NULL;
static struct device* sram_device = NULL;
static char response_buffer[BUFFER_SIZE];

static int dev_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "SRAM Control: Device opened\n");
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "SRAM Control: Device closed\n");
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    int bytes_read = 0;
    int error_count = 0;
    
    if (*offset >= strlen(response_buffer)) return 0;
    
    bytes_read = strlen(response_buffer) - *offset;
    if (bytes_read > len) bytes_read = len;
    
    error_count = copy_to_user(buffer, response_buffer + *offset, bytes_read);
    
    if (error_count == 0) {
        *offset += bytes_read;
        return bytes_read;
    } else {
        return -EFAULT;
    }
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char cmd[32];
    struct file *tty_file;
    int ret, read_count = 0;
    char read_buf[BUFFER_SIZE];

    if (len > 31) len = 31;
    ret = copy_from_user(cmd, buffer, len);
    if (ret) {
        printk(KERN_ALERT "SRAM Control: Failed to copy from user\n");
        return -EFAULT;
    }
    cmd[len] = '\0';
    
    tty_file = filp_open("/dev/ttyACM0", O_RDWR, 0);
    if (IS_ERR(tty_file)) {
        printk(KERN_ALERT "SRAM Control: Failed to open Arduino port\n");
        return PTR_ERR(tty_file);
    }

    ret = kernel_write(tty_file, cmd, len, 0);
    if (ret < 0) {
        filp_close(tty_file, NULL);
        return ret;
    }

    // Introduce a delay to allow Arduino to process the command
    msleep(100);  // Wait 100ms for Arduino to process and respond
    
    memset(response_buffer, 0, BUFFER_SIZE);
    read_count = kernel_read(tty_file, response_buffer, BUFFER_SIZE - 1, 0);
    
    filp_close(tty_file, NULL);

    if (read_count > 0) {
        response_buffer[read_count] = '\0';
        printk(KERN_INFO "SRAM Control: Received response: %s\n", response_buffer);
    }

    // Introduce another delay after reading response from Arduino
    msleep(50);  // Wait 50ms to ensure serial port buffer has settled

    return len;
}

static struct file_operations fops = {
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static int __init sram_init(void) {
    printk(KERN_INFO "SRAM Control: Initializing module\n");
    
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) return major_number;
    
    sram_class = class_create(CLASS_NAME);
    if (IS_ERR(sram_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(sram_class);
    }
    
    sram_device = device_create(sram_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(sram_device)) {
        class_destroy(sram_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(sram_device);
    }
    
    return 0;
}

static void __exit sram_exit(void) {
    device_destroy(sram_class, MKDEV(major_number, 0));
    class_unregister(sram_class);
    class_destroy(sram_class);
    unregister_chrdev(major_number, DEVICE_NAME);
}

module_init(sram_init);
module_exit(sram_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("SRAM Control through Arduino");
MODULE_VERSION("0.1");
