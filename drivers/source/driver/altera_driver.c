#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#define ON_BOTAO 1
#define ON_LEDR 2
#define ON_LEDG 3
#define ON_SWITCH 4
#define ON_SSEG 5
#define ON_SSEG2 6

MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("Basic Driver PCIHello");
MODULE_AUTHOR("Patrick Schaumont");

//-- Hardware Handles

static void *seteseg;  // handle to 32-bit output PIO
static void *seteseg2;
static void *switchport;   // handle to 16-bit input PIO
static void *ledvermelho;
static void *ledverde;
static void *botao;

//-- Char Driver Interface
static int   access_count =  0;
static int   MAJOR_NUMBER = 91;

static int     char_device_open    ( struct inode * , struct file *);
static int     char_device_release ( struct inode * , struct file *);
static ssize_t char_device_read    ( struct file * , char *,       size_t , loff_t *);
static ssize_t char_device_write   ( struct file * , const char *, size_t , loff_t *);

static struct file_operations file_opts = {
      .read = char_device_read,
      .open = char_device_open,
     .write = char_device_write,
   .release = char_device_release
};

static int char_device_open(struct inode *inodep, struct file *filep) {
   access_count++;
   printk(KERN_ALERT "altera_driver: opened %d time(s)\n", access_count);
   return 0;
}

static int char_device_release(struct inode *inodep, struct file *filep) {
   printk(KERN_ALERT "altera_driver: device closed.\n");
   return 0;
}

static ssize_t char_device_read(struct file *filep, char *buf, size_t len, loff_t *off) {
  int leitura, i;
  size_t count = len;

  if (len == ON_BOTAO) {
    leitura = ioread16(botao);
  } else {
    leitura = ioread16(switchport);
  }

  put_user(leitura & 0xFF, buf++);
  put_user((leitura >> 8) & 0xFF, buf++);
  
  count = 4;
  
  return count;
}

static ssize_t char_device_write(struct file *filep, const char *buf, size_t len, loff_t *off) {
  char *ptr = (char *) buf;
  size_t count = len;
  //  printk(KERN_ALERT "altera_driver: write %d bytes\n", len);
  unsigned k = *((int *) ptr);
  
  if (len == ON_SSEG) {
    iowrite32(k, seteseg);
  } else if (len == ON_SSEG2) {
    iowrite32(k, seteseg2);
  } else if (len == ON_LEDR) {
    iowrite32(k, ledvermelho);
  } else if (len == ON_LEDG) {
    iowrite32(k, ledverde);
  }

  count = 4;
  
  return count;
}

//-- PCI Device Interface

static struct pci_device_id pci_ids[] = {
  { PCI_DEVICE(0x1172, 0x0004), },
  { 0, }
};
MODULE_DEVICE_TABLE(pci, pci_ids);

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id);
static void pci_remove(struct pci_dev *dev);

static struct pci_driver pci_driver = {
  .name     = "alterahello",
  .id_table = pci_ids,
  .probe    = pci_probe,
  .remove   = pci_remove,
};

static unsigned char pci_get_revision(struct pci_dev *dev) {
  u8 revision;

  pci_read_config_byte(dev, PCI_REVISION_ID, &revision);
  return revision;
}

static int pci_probe(struct pci_dev *dev, const struct pci_device_id *id) {
  int vendor;
  int retval;
  unsigned long resource;

  retval = pci_enable_device(dev);
  
  if (pci_get_revision(dev) != 0x01) {
    printk(KERN_ALERT "altera_driver: cannot find pci device\n");
    return -ENODEV;
  }

  pci_read_config_dword(dev, 0, &vendor);
  printk(KERN_ALERT "altera_driver: Found Vendor id: %x\n", vendor);

  resource = pci_resource_start(dev, 0);
  printk(KERN_ALERT "altera_driver: Resource start at bar 0: %lx\n", resource);

  seteseg = ioremap_nocache(resource + 0XC000, 0x20);
  switchport  = ioremap_nocache(resource + 0XC020, 0x20);
  botao = ioremap_nocache(resource + 0XC060, 0x20);
  ledvermelho  = ioremap_nocache(resource + 0XC080, 0x20);
  ledverde = ioremap_nocache(resource + 0XC0A0, 0x20);
  seteseg2 = ioremap_nocache(resource + 0XC0C0, 0x20);

  return 0;
}

static void pci_remove(struct pci_dev *dev) {
  iounmap(seteseg);
  iounmap(switchport);
  iounmap(seteseg2);
  iounmap(ledverde);
  iounmap(ledvermelho);
  iounmap(botao);
}


//-- Global module registration

static int __init altera_driver_init(void) {
   int t = register_chrdev(MAJOR_NUMBER, "de2i150_altera", &file_opts);
   t = t | pci_register_driver(&pci_driver);

   if(t<0)
      printk(KERN_ALERT "altera_driver: error: cannot register char or pci.\n");
   else
     printk(KERN_ALERT "altera_driver: char+pci drivers registered.\n");

   return t;
}

static void __exit altera_driver_exit(void) {
  printk(KERN_ALERT "Goodbye from de2i150_altera.\n");

  unregister_chrdev(MAJOR_NUMBER, "de2i150_altera");
  pci_unregister_driver(&pci_driver);
}

module_init(altera_driver_init);
module_exit(altera_driver_exit);
