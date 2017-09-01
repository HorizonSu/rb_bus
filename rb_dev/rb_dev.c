#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/kfifo.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>

#define DEBUG       0                       /* 调试信息开关 */
#define DEVICE_NAME "rb_dev"                /* 设备文件名 */
#define BUFF_SIZE   512//1024                    /* 缓冲区大小 */

#define BUSY_IO     GPIO_PB(28)             /* BUSY引脚GPIO号 */
#define CLK_IO      GPIO_PA(22)             /* CLK引脚GPIO号 */
#define DATA_IO     GPIO_PB(29)             /* DATA引脚GPIO号 */
#define BUSY_IRQ    gpio_to_irq(BUSY_IO)    /* BUSY引脚中断号 */

#if DEBUG
#define	DBG_MSG(fmt, ...)	printk(fmt, ##__VA_ARGS__)
#else
#define DBG_MSG(fmt, ...)
#endif

/* 设备结构 */
struct rb_dev {
    struct miscdevice rb_misc;
    struct kfifo rxfifo;                    /* 接收环形缓冲区 */
    struct kfifo txfifo;                    /* 发送环形缓冲区 */
};

static struct mutex rb_mtx;                 /* 设备互斥锁，同一时刻只允许一个线程访问设备 */
static struct rb_dev *rb;                   /* 设备结构全局指针 */

static int rb_irq_ctrl(bool sw);

/* GPIO申请 */
static int rb_gpio_set(void)
{
    if (gpio_request(BUSY_IO, "rb_busy") != 0)
        goto err_busy;

    if (gpio_request(CLK_IO, "rb_clk") != 0)
        goto err_clk;

    if (gpio_request(DATA_IO, "rb_data") != 0)
        goto err_data;

    DBG_MSG("\n!================================> rb_gpio_set()\n");

    return 0;

err_data:
    gpio_free(CLK_IO);

err_clk:
    gpio_free(BUSY_IO);

err_busy:
    return -1;
}

/* GPIO释放 */
static void rb_gpio_free(void)
{
    gpio_free(DATA_IO);
    gpio_free(CLK_IO);
    gpio_free(BUSY_IO);

    DBG_MSG("\n!================================> rb_gpio_free()\n");

    return;
}

/* 释放RB总线 */
static void rb_free_bus(void)
{
    gpio_direction_input(BUSY_IO);	// 1
    gpio_direction_input(CLK_IO);	// 1
    gpio_direction_output(DATA_IO, 0);// 0

    return;
}

//wpb add: 
static int rb_Init_bus(void)
{

    gpio_direction_input(BUSY_IO);
    gpio_direction_input(CLK_IO);
    gpio_direction_output(DATA_IO, 0);

	if(rb_irq_ctrl(1) !=0)	//wpb add
	{
		DBG_MSG("\n!rb_Init_bus===> rb_irq_ctrl(1): failed \n");//wpb add
		return -1;
	}

    return 0;
}

//wpb add: 
/*
static void rb_IO_test(void)
{
	int cnt = 10;
	int value = 0;

// interrupt gpio must be free_irq before it works as an GPIO
free_irq(BUSY_IRQ, rb);
//disable_irq(BUSY_IRQ);// 将关闭硬件中断并等待（可能有的）中断处理完成才返回。

    gpio_direction_output(BUSY_IO,0);
    gpio_direction_output(CLK_IO,0);
    gpio_direction_output(DATA_IO, 0);

	while(cnt--)
	{
		gpio_direction_output(BUSY_IO,value);
		gpio_direction_output(CLK_IO,value);
		gpio_direction_output(DATA_IO, value);
		//gpio_set_value(BUSY_IO, value);
		//gpio_set_value(CLK_IO, value);
		//gpio_set_value(DATA_IO, value);

		udelay(1000);
		value = (value==0)  ? 1 : 0;		

	}

//	gpio_set_value(BUSY_IO, 0);	//BAD set to input gpio
//	gpio_set_value(CLK_IO, 0);
//	gpio_set_value(DATA_IO, 0);


    return;
}
*/





/* 检测RB总线是否忙 */
static int rb_ask_bus(void)
{
    int count = 0;

    while (gpio_get_value(BUSY_IO) == 0 || gpio_get_value(CLK_IO) == 0) {
        if (count >=200)
            goto bus_busy;
        udelay(50);
        count++;
    }
    
    udelay(50);
    count = 0;

    while (gpio_get_value(BUSY_IO) == 0 || gpio_get_value(CLK_IO) == 0) {
        if (count >= 200)
            goto bus_busy;
        udelay(50);
        count++;
    }
    
    gpio_direction_output(BUSY_IO, 0);

    return 0;

bus_busy:
    return 1;
}

/* 发送一个字节 */
static int rb_send_byte(unsigned char dat) 
{
    int count = 0;
    int i = 0;

//DBG_MSG("\n!==========> rb_send_byte: start \n");//wpb add
    gpio_direction_output(BUSY_IO, 0);//BUSY=0
    gpio_direction_input(CLK_IO);	// CLK=1
    gpio_direction_input(DATA_IO);	// DATA=1

//DBG_MSG("\n!==========> rb_send_byte: 1 \n");//wpb add    
    while (gpio_get_value(DATA_IO) == 0) {
        if (count >=1000)
		{
            goto err_bus;
		}
       	udelay(10);
        count++;
    }


//DBG_MSG("\n!==========> rb_send_byte: 2 \n");//wpb add
    for (i = 0; i < 8; i++) {
        if (dat & 0x80)
            gpio_direction_input(DATA_IO);
        else
            gpio_direction_output(DATA_IO, 0);

//DBG_MSG("\n!==========> rb_send_byte: 3 \n");//wpb add

        dat <<= 1;
        gpio_direction_output(CLK_IO, 0);
        udelay(25);
        gpio_direction_input(CLK_IO);
        if (i != 7)
            udelay(25);
//DBG_MSG("\n!==========> rb_send_byte: 4 \n");//wpb add
            
        count = 0;
        while (gpio_get_value(CLK_IO) == 0) {
            if (count >= 1000)
                goto err_bus;
            udelay(10);
            count++;
        }
    }

//DBG_MSG("\n!==========> rb_send_byte: 5-end \n");//wpb add
    
    gpio_direction_input(BUSY_IO);		//BUSY=1
    gpio_direction_output(CLK_IO, 0);	//CLK=0
    gpio_direction_input(DATA_IO);		//DATA=1

//DBG_MSG("\n!==========> rb_send_byte: 5-end \n");//wpb add

    return 0;

err_bus:
    rb_free_bus();

DBG_MSG("\n!==========> rb_send_byte:err_bus end \n");//wpb add

    return -1;
}

/* 发送多个字节 */
static int rb_send(unsigned char *dat, unsigned char num)
{
    int ret = 0;
    int i = 0;

	//    disable_irq(BUSY_IRQ);
	rb_irq_ctrl(0); //wpb add: disable irq

    if (rb_ask_bus() == 1) {
        ret = -1;
        goto done_send;
    }

    for (i = 0; i < num; i++) {
        if (rb_send_byte(dat[i]) != 0) {
            ret = i;
            goto done_send;
        }
//DBG_MSG("\n!==========> rb_send: 1 \n");//wpb add

        udelay(100);
    }

    rb_free_bus(); //wpb zhushi: if this command enable, 
					//it will generate many pulse on BUSY IO
    ret = num;
//DBG_MSG("\n!==========> rb_send: 2 \n");//wpb add

done_send:
	//    enable_irq(BUSY_IRQ);
	rb_irq_ctrl(1); //wpb add: enable irq
//DBG_MSG("\n!==========> rb_send: 3 \n");//wpb add

    return ret;
}

/* 读取一个字节 */
static int rb_read_byte(unsigned char *dat) 
{
    int ret = -1;
    int count = 0;
    int i = 0;

//DBG_MSG("\n!==========> rb_read_byte: 1 \n");//wpb add

    if (gpio_get_value(BUSY_IO) != 0)
        goto done_read;

    gpio_direction_input(DATA_IO);
    gpio_direction_output(BUSY_IO, 0);
	gpio_direction_input(CLK_IO);	//wpb add

//disable_irq_nosync(BUSY_IRQ);//wpb: can not and no need used in ISR

//DBG_MSG("\n!==========> rb_read_byte: 2 \n");//wpb add

    for (i = 0; i < 8; i++) {
        count = 0;
        while (gpio_get_value(CLK_IO) != 0) {
            if (count >= 1000)
                goto done_read;
            udelay(10);
            count++;
        }

//DBG_MSG("\n!==========> rb_read_byte: 4 \n");//wpb add

        gpio_direction_output(CLK_IO, 0);
        gpio_direction_input(DATA_IO);
        udelay(25);
        *dat <<= 1;
        *dat |= (unsigned char)gpio_get_value(DATA_IO) & 0x01;
        gpio_direction_input(CLK_IO);

//DBG_MSG("\n!==========> rb_read_byte: 5 \n");//wpb add

        count = 0;
        while (gpio_get_value(CLK_IO) == 0) {
            if (count >= 1000)
                goto done_read;
            udelay(10);
            count++;
        }
    }
    ret = 0;

//DBG_MSG("\n!==========> rb_read_byte: 6 \n");//wpb add

done_read:
    rb_free_bus();
//    enable_irq(BUSY_IRQ);	//wpb: can not and no need used in ISR



    return ret;
}

/* 中断处理函数 */
static irqreturn_t rb_irq(int irq, void *dev)
{
    unsigned char dat = 0;
    struct rb_dev *p = (struct rb_dev *)dev;

 //   disable_irq(BUSY_IRQ);	//wpb: can not used in ISR
//DBG_MSG("\n!==========> rb_irq-begin \n");//wpb add
     
    if (rb_read_byte(&dat) == 0)
        kfifo_put(&p->rxfifo, &dat);
//DBG_MSG("\n!==========> rb_irq-end=%x \n",dat);//wpb add

    return IRQ_HANDLED;
}


//wpb add:
// sw=0, disable irq 
// sw=1, enbale  irq 
static int rb_irq_ctrl(bool sw)
{
	//remember the status of irq 
	static int irq_Flag = 0; // 0-disable, 1-enable
	int ret;

	// enable irq
	if(sw == 1)
	{
	 	if(irq_Flag == 0) // irq previous has been disabled 
		{
			if ((ret = request_irq(BUSY_IRQ, rb_irq, IRQF_TRIGGER_FALLING, DEVICE_NAME, rb)) != 0)
			 {
				DBG_MSG("\n!rb_irq_ctrl===> request_irq: failed \n");//wpb add
				return ret;
			 }	
//DBG_MSG("\n!rb_irq_ctrl=1 \n");//wpb add
			disable_irq_nosync(BUSY_IRQ);//强行关闭指定中断，不会等待当前中断处理程序执行完毕。，立即返回
//DBG_MSG("\n!rb_irq_ctrl=2 \n");//wpb add
		 	enable_irq(BUSY_IRQ);
//DBG_MSG("\n!rb_irq_ctrl=3 \n");//wpb add

		}
		irq_Flag = 1;// remember the status of irq
	}
	// disable irq
	else
	{
	 	if(irq_Flag == 1) // irq previous has been enabled
		{
			//disable_irq(BUSY_IRQ);// 将关闭硬件中断并等待（可能有的）中断处理完成才返回。
			disable_irq_nosync(BUSY_IRQ);//强行关闭指定中断，不会等待当前中断处理程序执行完毕。，立即返回
			free_irq(BUSY_IRQ, rb);		 //释放中断
		}
		irq_Flag = 0;// remember the status of irq
	}
	return 0;
}


/* 数据发送tasklet */
static void do_send(unsigned long data)
{ 
    int len = 0;
    int ret = 0;
//    unsigned char *buff;
    unsigned char buff[BUFF_SIZE]={0};

//    if ((buff = kmalloc(BUFF_SIZE, GFP_KERNEL)) == NULL)
//        return;

    if (kfifo_is_empty(&rb->txfifo))
        return;

    len = kfifo_out(&rb->txfifo, buff, kfifo_len(&rb->txfifo));

	//wpb add
	ret = rb_send(buff, len);
	if (ret == -1)
		DBG_MSG("\n!rb_send:failed end\n");
/*
    do {
        ret = rb_send(buff, len);
        if (ret == -1)
            udelay(500);
        else
            len -= ret;
    } while (len == 0);
*/

//    kfree(buff);

    return;
}
DECLARE_TASKLET(send_tasklet, do_send, 0);

static int rb_dev_open(struct inode *inode, struct file *filp)
{
    DBG_MSG("\n!================================> rb_dev_open()\n");

	rb_Init_bus();//wpb add

    return mutex_trylock(&rb_mtx) ? 0 : -EBUSY;
}

static ssize_t rb_dev_read(struct file *filp, char __user *buff, size_t count, loff_t *offp)
{
    int n = 0;

    if (kfifo_to_user(&rb->rxfifo, buff, count, &n) != 0)
        return -EFAULT;


    DBG_MSG("\n!================================> rb_dev_read()\n");

    return n;
}

static ssize_t rb_dev_write(struct file *filp, const char __user *buff, size_t count, loff_t *offp)
{
    int n = 0;

    if (kfifo_from_user(&rb->txfifo, buff, count, &n) != 0)
        return -EFAULT;

//    tasklet_schedule(&send_tasklet);
	do_send(0); //wpb add

    DBG_MSG("\n!================================> rb_dev_write()\n");

    return n;
}

static int rb_dev_release(struct inode *inode, struct file *filp)
{
    mutex_unlock(&rb_mtx);

    DBG_MSG("\n!================================> rb_dev_release()\n");

    return 0;
}

static struct file_operations rb_dev_fops = {
    .owner      = THIS_MODULE,
    .open       = rb_dev_open,
    .read       = rb_dev_read,
    .write      = rb_dev_write,
    .release    = rb_dev_release,
};

/* 设备结构初始化 */
static int rb_dev_set(void)
{
    int ret = 0;

    if ((rb = kzalloc(sizeof(*rb), GFP_KERNEL)) == NULL) {
        printk("kzalloc failed!\n");
        return -ENOMEM;
    }

    ret = kfifo_alloc(&rb->rxfifo, BUFF_SIZE, GFP_KERNEL);
    if (ret != 0) {
        printk("kfifo_alloc failed!\n");
        goto err_rxfifo;
    }

    ret = kfifo_alloc(&rb->txfifo, BUFF_SIZE, GFP_KERNEL);
    if (ret != 0) {
        printk("kfifo_alloc failed!\n");
        goto err_txfifo;
    }

    rb->rb_misc.minor = MISC_DYNAMIC_MINOR;
    rb->rb_misc.name = DEVICE_NAME;
    rb->rb_misc.fops = &rb_dev_fops;

    DBG_MSG("\n!================================> rb_dev_set()\n");

    return ret;

err_txfifo:
    kfifo_free(&rb->rxfifo);

err_rxfifo:
    kfree(rb);
    return ret;
}

/* 设备结构释放 */
static void rb_dev_destroy(void)
{
    kfifo_free(&rb->txfifo);
    kfifo_free(&rb->rxfifo);
    kfree(rb);

    DBG_MSG("\n!================================> rb_dev_destroy()\n");

    return;
}

static int  __init rb_dev_init(void)
{
    int ret = 0;

    if ((ret = rb_dev_set()) != 0)
        return ret;

    if ((ret = rb_gpio_set()) != 0)
        return ret;
    
//    if ((ret = request_irq(BUSY_IRQ, rb_irq, IRQF_TRIGGER_FALLING, DEVICE_NAME, rb)) != 0)
//        return ret;


    mutex_init(&rb_mtx);

    DBG_MSG("\n!================================> rb_dev_init()\n");
    
    return misc_register(&rb->rb_misc);
}

static void __exit rb_dev_exit(void)
{
    misc_deregister(&rb->rb_misc);
//    free_irq(BUSY_IRQ, rb); 
	rb_irq_ctrl(0);	//wpb add 
    rb_gpio_free();
    rb_dev_destroy();

    DBG_MSG("\n!================================> rb_dev_exit()\n");

    return;
}

module_init(rb_dev_init);
module_exit(rb_dev_exit);
MODULE_DESCRIPTION("RB bus driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Horizon");
