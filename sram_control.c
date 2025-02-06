#include <linux/init.h>        
#include <linux/module.h>       
#include <linux/kernel.h>       
#include <linux/fs.h>            
#include <linux/uaccess.h>       
#include <linux/serial_core.h>   
#include <linux/serial.h>       
#include <linux/tty.h>           
#include <linux/delay.h>        

#define DRIVER_NAME "sram_manager"  
#define CLASS_LABEL "sram_device"  
#define MAX_BUFFER_SIZE 128         

static int dev_major;                     // Huvudnummer för enheten som ska registreras
static struct class* sram_dev_class = NULL; // Klass för enheten
static struct device* sram_dev = NULL;     // Enhetsstruktur
static char kernel_buffer[MAX_BUFFER_SIZE]; // Buffert för att lagra data som läses från Arduino

// Funktion som anropas när enheten öppnas
static int device_open(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "SRAM Manager: Device opened\n");  
    return 0;  //framgång
}

// Funktion som anropas när enheten stängs
static int device_close(struct inode *inodep, struct file *filep) {
    printk(KERN_INFO "SRAM Manager: Device closed\n");  // Loggar att enheten stängdes
    return 0;  // framgång
}

// Funktion som läser data från bufferten och skickar den till användaren
static ssize_t device_read(struct file *filep, char *user_buffer, size_t len, loff_t *offset) {
    int bytes_to_read = 0;  
    int result = 0;       

    if (*offset >= strlen(kernel_buffer)) return 0;  // Om vi är vid slutet av bufferten, returnera 0

    bytes_to_read = strlen(kernel_buffer) - *offset;  // Beräkna antalet byte som ska läsas
    if (bytes_to_read > len) bytes_to_read = len;    // Begränsa antalet byte om det överskrider bufferten

    result = copy_to_user(user_buffer, kernel_buffer + *offset, bytes_to_read);  // Kopiera data till användarutrymme
    if (result == 0) {
        *offset += bytes_to_read;  // Uppdatera offset
        return bytes_to_read;      
    } else {
        return -EFAULT;  // Returnera fel om kopiering misslyckades
    }
}

// Funktion som skriver data till den seriella porten och tar emot svar
static ssize_t device_write(struct file *filep, const char *user_buffer, size_t len, loff_t *offset) {
    char command[32];           // Buffert för att lagra kommandot
    struct file *serial_port;   // Pekare till den seriella porten
    int write_result, read_len = 0;  // Variabler för skriv- och läsresultat
    char read_data[MAX_BUFFER_SIZE]; // Buffert för att lagra svar från Arduino

    if (len > 31) len = 31;  // kommandolängden till 31 tecken
    if (copy_from_user(command, user_buffer, len)) {  // Kopiera kommandot från användarutrymme till kärna
        printk(KERN_ALERT "SRAM Manager: Failed to copy data from user\n");
        return -EFAULT;  // Returnera fel 
    }
    command[len] = '\0';  //  avsluta strängen

    // Öppna den seriella porten (Arduino är ansluten till /dev/ttyACM0)
    serial_port = filp_open("/dev/ttyACM0", O_RDWR, 0);
    if (IS_ERR(serial_port)) {  // Kontrollera om öppningen misslyckades
        printk(KERN_ALERT "SRAM Manager: Could not open serial port\n");
        return PTR_ERR(serial_port);  // Returnera felet
    }

    // Skicka kommandot till den seriella porten
    write_result = kernel_write(serial_port, command, len, 0);
    if (write_result < 0) {
        filp_close(serial_port, NULL);  // Stäng porten om skrivningen misslyckades
        return write_result;
    }

    msleep(150);  

    memset(kernel_buffer, 0, MAX_BUFFER_SIZE);  // Rensa bufferten
    read_len = kernel_read(serial_port, kernel_buffer, MAX_BUFFER_SIZE - 1, 0);  // Läs svar från Arduino

    filp_close(serial_port, NULL);  

    if (read_len > 0) {
        kernel_buffer[read_len] = '\0';  // Avslutar strängen
        printk(KERN_INFO "SRAM Manager: Response received: %s\n", kernel_buffer);  // Logga svaret
    }

    msleep(50);  // Kort paus för att säkerställa att seriella porten är redo för nästa operation

    return len;  
}

// filoperationerna för enheten
static struct file_operations fops = {
    .open = device_open,    
    .read = device_read,       
    .write = device_write,    
    .release = device_close,   
};

// Initieringsfunktion som körs när modulen laddas
static int __init sram_manager_init(void) {
    printk(KERN_INFO "SRAM Manager: Module initialization\n");

    // Registrera en karaktärsenhet med dynamiskt huvudnummer
    dev_major = register_chrdev(0, DRIVER_NAME, &fops);
    if (dev_major < 0) return dev_major;

    // Skapa en enhetsklass
    sram_dev_class = class_create(THIS_MODULE, CLASS_LABEL);
    if (IS_ERR(sram_dev_class)) {
        unregister_chrdev(dev_major, DRIVER_NAME);
        return PTR_ERR(sram_dev_class);
    }

    // Skapa en enhet i /dev
    sram_dev = device_create(sram_dev_class, NULL, MKDEV(dev_major, 0), NULL, DRIVER_NAME);
    if (IS_ERR(sram_dev)) {
        class_destroy(sram_dev_class);
        unregister_chrdev(dev_major, DRIVER_NAME);
        return PTR_ERR(sram_dev);
    }

    printk(KERN_INFO "SRAM Manager: Device created successfully\n");
    return 0;
}

// Avslutningsfunktion som körs när modulen avinstalleras
static void __exit sram_manager_exit(void) {
    device_destroy(sram_dev_class, MKDEV(dev_major, 0));  // Ta bort enheten
    class_unregister(sram_dev_class);  // Avregistrera enhetsklassen
    class_destroy(sram_dev_class);     // Förstör enhetsklassen
    unregister_chrdev(dev_major, DRIVER_NAME);  // Avregistrera karaktärsenheten
    printk(KERN_INFO "SRAM Manager: Module exited\n");
}

module_init(sram_manager_init);  // Ange initieringsfunktionen
module_exit(sram_manager_exit);  // Ange avslutningsfunktionen

MODULE_LICENSE("GPL");               // Licens för modulen
MODULE_AUTHOR("Modified by ChatGPT"); // Författarinformation
MODULE_DESCRIPTION("SRAM Manager for Arduino Communication"); // Beskrivning av modulen
MODULE_VERSION("0.2");               // Versionsnummer
