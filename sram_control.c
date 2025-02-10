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

static int major_number; // huvud nummer för enhet 
static struct class* sram_class = NULL; // pekar till enhetsklassen 
static struct device* sram_device = NULL; //pekar till enhetsenhet 
static char response_buffer[BUFFER_SIZE]; //buffer för svar från arduino

static int dev_open(struct inode *inodep, struct file *filep) { // anrops när enheten öppnas 
    printk(KERN_INFO "SRAM Control: Device opened\n");
    return 0;
}

static int dev_release(struct inode *inodep, struct file *filep) { // anrops när det stängs
    printk(KERN_INFO "SRAM Control: Device closed\n");
    return 0;
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) { // läser data från enheten 
    int bytes_read = 0;
    int error_count = 0;
    
    if (*offset >= strlen(response_buffer)) return 0; // kontroll om vi läst allt
    
    bytes_read = strlen(response_buffer) - *offset; //berkänar antal bytes att läsa  
    if (bytes_read > len) bytes_read = len; // begränsa till längden 
    
    error_count = copy_to_user(buffer, response_buffer + *offset, bytes_read); //kopierar data till användaren yta
    
    if (error_count == 0) {
        *offset += bytes_read;
        return bytes_read;
    } else {
        return -EFAULT; // fel vid kopiering
    }
}
 // skriver till enehten 
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char cmd[32];
    struct file *tty_file;
    int ret, read_count = 0;
    char read_buf[BUFFER_SIZE];

    if (len > 31) len = 31; //begränsa kommandolängd
    ret = copy_from_user(cmd, buffer, len); // kopierar data 
    if (ret) {
        printk(KERN_ALERT "SRAM Control: Failed to copy from user\n");
        return -EFAULT;
    }
    cmd[len] = '\0';
    
    tty_file = filp_open("/dev/ttyACM0", O_RDWR, 0); // öpppnar en serieport till arduino 
    if (IS_ERR(tty_file)) {
        printk(KERN_ALERT "SRAM Control: Failed to open Arduino port\n");
        return PTR_ERR(tty_file);
    }

    ret = kernel_write(tty_file, cmd, len, 0); // skickar kommando till arduino 
    if (ret < 0) {
        filp_close(tty_file, NULL);
        return ret;
    }

    msleep(100);  // delay för att det ska hinna svara
    
    memset(response_buffer, 0, BUFFER_SIZE);
    read_count = kernel_read(tty_file, response_buffer, BUFFER_SIZE - 1, 0); //läsa svar från ard
    
    filp_close(tty_file, NULL);

    if (read_count > 0) {
        response_buffer[read_count] = '\0';
        printk(KERN_INFO "SRAM Control: Received response: %s\n", response_buffer);
    }


    msleep(50);  // delay för att stablisera buffert 

    return len;
}

static struct file_operations fops = { // filoperationer som är koppklade till enhetsdriv
    .open = dev_open,
    .read = dev_read,
    .write = dev_write,
    .release = dev_release,
};

static int __init sram_init(void) { //initerara funtkionen för modulen 
    printk(KERN_INFO "SRAM Control: Initializing module\n");
    
    major_number = register_chrdev(0, DEVICE_NAME, &fops); // registerar enheten 
    if (major_number < 0) return major_number;
    
    sram_class = class_create(CLASS_NAME); //skapa enhetsklass
    if (IS_ERR(sram_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(sram_class);
    }
    
    sram_device = device_create(sram_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME); //skapa enhetsenhet 
    if (IS_ERR(sram_device)) {
        class_destroy(sram_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(sram_device);
    }
    
    return 0;
}
 // avslutnings funktion till modulen 
static void __exit sram_exit(void) {
    device_destroy(sram_class, MKDEV(major_number, 0)); //ta bort enhet 
    class_unregister(sram_class); //avreg klass
    class_destroy(sram_class); // ta bort klass
    unregister_chrdev(major_number, DEVICE_NAME); // avreg
}

module_init(sram_init); // reg int funktion 
module_exit(sram_exit); // reg exit funktion 

MODULE_LICENSE("GPL"); // licens 
MODULE_AUTHOR("Your Name"); 
MODULE_DESCRIPTION("SRAM Control through Arduino");
MODULE_VERSION("0.1");
