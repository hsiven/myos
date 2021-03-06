/*
 *  mouse driver
 */
#include <os/asm.h>
#include <os/arch_config.h>
#include <os/type.h>
#include <lib/klib.h>
#include <os/isr.h>
#include <os/io.h>
#include <drv/i8042.h>
#include <os/sem.h>
#include <drv/input.h>
#include <os/vfs.h>
#include <os/task.h>
#include <os/timer.h>
#define xstr(s) str(s)
#define str(x) #x

extern struct input_dev_desc mouse_desc;

void mouse_thread()
{
	int fd;
	struct s_event event;
	fd = vfs_open("/dev/input/"xstr(INPUT_MINOR_MOUSE), 0);
	assert(fd != -1);
	printk("mousefd = %d\n", fd);

	enable_irq();
	for(;;)
	{
		vfs_read(fd, &event, sizeof(event));
		printk("tick=%d type=%d code=%x value=(%d,%d)\n",
		       event.ticks,
		       event.type,
		       event.code,
		       (short)(event.value>>16),
		       (short)(event.value&0xffff));
	}
}

static int mouse_int()
{
	unsigned char data = inb(0x60);
	static int last_tick;
	static int xdiff;
	static int ydiff;
	static int count;
	static int xsgn;
	static int ysgn;
	static int lrb;
	struct s_event event;
	int ticks = timer_get_ticks();

	//trick:在真机上数据对不上号，用ticks计时，把过早数据丢弃
	if(!last_tick) last_tick = ticks;
	if(ticks - last_tick > 5)
		count = 0;
	switch (++count)
	{
		case 1:
			lrb = data & 0x7;
			xsgn = (data & 0x10) ? (~0xff) : 0;
			ysgn = (data & 0x20) ? (~0xff) : 0;
			break;
		case 2:
			xdiff = (xsgn | data);
			break;
		case 3:
			ydiff = -(ysgn | data);
			count = 0;
			/* printk("xdiff=%d, ydiff=%d\n", */
			/*        xdiff, */
			/*        ydiff); */
			event.ticks = ticks;
			event.type = 2;
			event.code = lrb;
			event.value = (xdiff<<16) | (ydiff&0xffff);
			input_dev_event(&mouse_desc, &event);
			break;
	}
	last_tick = ticks;
	return 0;
}

static int mouse_init()
{
	printk("mouse: up\n");
	irq_register(IRQ_MOUSE, mouse_int);
	pic_enable_irq(IRQ_MOUSE);
	return 0;
}

static int mouse_exit()
{
	printk("mouse: down\n");
	return 0;
}

struct input_dev_desc mouse_desc = {
	.name = "mouse",
	.init = mouse_init,
	.exit = mouse_exit,
};
