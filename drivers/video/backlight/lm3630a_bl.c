/*
* Simple driver for Texas Instruments LM3630A Backlight driver chip
* Copyright (C) 2012 Texas Instruments
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
*/
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/pwm.h>
#include <linux/platform_data/lm3630a_bl.h>
#include <linux/regulator/consumer.h>

#define REG_CTRL	0x00
#define REG_BOOST	0x02
#define REG_CONFIG	0x01
#define REG_BRT_A	0x03
#define REG_BRT_B	0x04
#define REG_I_A		0x05
#define REG_I_B		0x06
#define REG_INT_STATUS	0x09
#define REG_INT_EN	0x0A
#define REG_FAULT	0x0B
#define REG_PWM_OUTLOW	0x12
#define REG_PWM_OUTHIGH	0x13
#define REG_MAX		0x1F

#define INT_DEBOUNCE_MSEC	10
struct lm3630a_chip {
	struct device *dev;
#if 0
	struct delayed_work work;
	int irq;
	struct workqueue_struct *irqthread;
#endif
	struct i2c_client *client;
	struct lm3630a_platform_data *pdata;
	struct backlight_device *bleda;
	struct backlight_device *bledb;
	struct backlight_device *bled;
	struct pwm_device *pwmd;
	int frontlight_table;
};

static struct lm3630a_chip *gpchip;

static const unsigned char bank_a_percent[] = {
	1, 3, 7, 14, 26, 30, 41, 52, 60, 66, 				// 1.. 10
	71, 75, 80, 85, 88, 92, 96, 103, 109, 114, 			// 11 .. 20
	118, 122, 126, 129, 131, 134, 135, 138, 141, 147,	// 21 .. 30
	150, 153, 157, 158, 159, 161, 164, 166, 168, 170,	// 31 .. 40
	172, 174, 175, 177, 178, 179, 184, 185, 187, 189,	// 41 .. 50
	191, 193, 194, 196, 198, 200, 201, 203, 204, 206,	// 51 .. 60
	207, 208, 210, 211, 213, 215, 216, 217, 218, 219,	// 61 .. 70
	220, 221, 222, 223, 224, 226, 227					// 71 .. 77
};

static const unsigned char bank_b_percent[] = {
	1, 2, 6, 11, 21, 25, 35, 47, 54, 60,				// 1.. 10
	65, 69, 73, 77, 80, 86, 90, 96, 103, 108,           // 11 .. 20
	112, 116, 120, 123, 125, 127, 128, 132, 135, 140,   // 21 .. 30
	144, 147, 150, 151, 152, 154, 157, 159, 161, 163,   // 31 .. 40
	165, 167, 170, 172, 173, 174, 177, 178, 180, 182,   // 41 .. 50
	184, 186, 187, 189, 191, 193, 194, 195, 197, 198,   // 51 .. 60
	199, 201, 203, 204, 206, 208, 209, 210, 211, 212,   // 61 .. 70
	213, 214, 215, 216, 217, 219, 220,                  // 71 .. 77
};

static int gLM3630a_Fl_Table;
#define LM3630A_COLOR_TEMPERATURES		3
unsigned char lm3630a_fl_table[LM3630A_COLOR_TEMPERATURES][2][100] = {
	{
		{	// led A 100% (full_scale 4)
			43 , 66 , 79 , 90 , 96 , 107, 114, 120, 126, 132,	// 1 ~ 10
			135, 139, 143, 145, 147, 150, 152, 154, 156, 158,	// 11 ~ 20
			160, 163, 165, 168, 170, 172, 174, 176, 178, 180,	// 21 ~ 30
			181, 183, 185, 186, 188, 190, 191, 192, 193, 194,	// 31 ~ 40
			195, 196, 197, 198, 199, 200, 201, 202, 203, 204,	// 41 ~ 50
			205, 206, 207, 208, 209, 210, 211, 212, 213, 214,	// 51 ~ 60
			215, 216, 217, 218, 219, 220, 221, 222, 223, 224,	// 61 ~ 70
			225, 226, 227, 228, 229, 230, 231, 232, 233, 234,	// 71 ~ 80
			235, 236, 237, 238, 239, 240, 241, 242, 243, 244,	// 81 ~ 90
			245, 246, 247, 248, 249, 250, 251, 252, 253, 254,	// 91 ~ 100
		},
		{},
	},
	{
		{	// led A 90% (full_scale 4)
			37 , 61 , 75 , 86 , 93 , 102, 110, 117, 123, 128,	// 1 ~ 10
			133, 137, 140, 143, 144, 147, 149, 151, 153, 155,	// 11 ~ 20
			157, 159, 162, 165, 167, 170, 173, 174, 175, 176,	// 21 ~ 30
			177, 179, 180, 181, 184, 186, 187, 188, 189, 190,	// 31 ~ 40
			191, 192, 193, 194, 195, 196, 197, 198, 199, 200,	// 41 ~ 50
			201, 202, 203, 204, 205, 206, 207, 208, 209, 210,	// 51 ~ 60
			211, 212, 213, 214, 215, 216, 217, 218, 219, 220,	// 61 ~ 70
			221, 222, 223, 224, 225, 226, 227, 229, 229, 230,	// 71 ~ 80
			231, 232, 233, 234, 235, 236, 237, 238, 239, 240,	// 81 ~ 90
			241, 242, 243, 244, 245, 246, 247, 248, 249, 250,	// 91 ~ 100
		},
		{	// led B 10% (full_scale 7)
			4  , 6  , 14 , 22 , 28 , 35 , 42 , 48 , 53 , 57 ,	// 1 ~ 10
			62 , 65 , 69 , 71 , 73 , 76 , 78 , 80 , 81 , 84 ,	// 11 ~ 20
			85 , 87 , 90 , 93 , 95 , 100, 105, 97 , 107, 108,	// 21 ~ 30
			109, 110, 111, 113, 114, 116, 117, 117, 118, 119,	// 31 ~ 40
			120, 121, 122, 123, 124, 124, 125, 126, 127, 128,	// 41 ~ 50
			128, 129, 130, 131, 132, 133, 134, 135, 136, 136,	// 51 ~ 60
			141, 141, 144, 144, 144, 142, 143, 148, 148, 151,	// 61 ~ 70
			151, 151, 151, 151, 153, 152, 156, 151, 156, 158,	// 71 ~ 80
			155, 157, 160, 160, 161, 160, 162, 163, 164, 165,	// 81 ~ 90
			166, 165, 166, 169, 168, 171, 171, 171, 173, 174,	// 91 ~ 100
		},
	},
	{
		{},
		{	// led B 100% (full_scale 7)
			58 , 74 , 85 , 93 , 99 , 108, 115, 121, 126, 131,	// 1 ~ 10
			136, 139, 143, 145, 147, 150, 152, 154, 156, 158,	// 11 ~ 20
			160, 163, 165, 168, 170, 172, 174, 176, 178, 180,	// 21 ~ 30
			181, 183, 185, 186, 188, 190, 191, 192, 193, 194,	// 31 ~ 40
			195, 196, 197, 198, 199, 200, 201, 202, 203, 204,	// 41 ~ 50
			205, 206, 207, 208, 209, 210, 211, 212, 213, 214,	// 51 ~ 60
			215, 216, 217, 218, 219, 220, 221, 222, 223, 224,	// 61 ~ 70
			225, 226, 227, 228, 229, 230, 231, 232, 233, 234,	// 71 ~ 80
			235, 236, 237, 238, 239, 240, 241, 242, 243, 244,	// 81 ~ 90
			245, 246, 247, 248, 249, 250, 251, 252, 253, 254,	// 91 ~ 100
		},
	},
};

/* i2c access */
static int lm3630a_read(struct lm3630a_chip *pchip, unsigned int reg)
{
	int rval;

	rval = i2c_smbus_read_byte_data(pchip->client, reg);
	if (rval < 0)
		return rval;
	return rval & 0xFF;
}

static int lm3630a_write(struct lm3630a_chip *pchip,
			 unsigned int reg, unsigned int data)
{
//	printk("[%s-%d] reg 0x%02X, val %02X\n", __func__, __LINE__, reg, data);
	return i2c_smbus_write_byte_data(pchip->client, reg, (uint8_t)data);
}

static int lm3630a_update(struct lm3630a_chip *pchip,
			  unsigned int reg, unsigned int mask,
			  unsigned int data)
{
	int rval = i2c_smbus_read_byte_data(pchip->client, reg);
	if (rval < 0)
		return rval;
//	printk("[%s-%d] reg 0x%02X, val %02X\n", __func__, __LINE__, reg, data);
	rval &= ~mask;
	rval |= (data & mask);
	return i2c_smbus_write_byte_data(pchip->client, reg, (uint8_t)rval);
}

/* initialize chip */
static int lm3630a_chip_init(struct lm3630a_chip *pchip)
{
	int rval;
	struct lm3630a_platform_data *pdata = pchip->pdata;
	struct regulator *fl_regulator;

	fl_regulator = regulator_get(&pchip->client->dev, "vdd_fl_lm3630a");
	if (IS_ERR(fl_regulator)) {
		printk("%s, regulator \"vdd_fl_lm3630a\" not registered.(%d)\n", __func__, fl_regulator);
		return -1;
	}
	else
			printk("%s, vdd_fl_lm3630a found on channel 0\n", __func__);
	regulator_enable (fl_regulator);
	msleep (200);

	usleep_range(1000, 2000);
	/* set Filter Strength Register */
	rval = lm3630a_write(pchip, 0x50, 0x03);
	/* set Cofig. register */
	rval |= lm3630a_update(pchip, REG_CONFIG, 0x07, pdata->pwm_ctrl);
	/* set boost control */
	rval |= lm3630a_write(pchip, REG_BOOST, 0x38);
	/* set current A */
	rval |= lm3630a_update(pchip, REG_I_A, 0x1F, 0x0);
	/* set current B */
	rval |= lm3630a_write(pchip, REG_I_B, 0x0);
	/* set control */
	rval |= lm3630a_update(pchip, REG_CTRL, 0x14, pdata->leda_ctrl);
	rval |= lm3630a_update(pchip, REG_CTRL, 0x0B, pdata->ledb_ctrl);
	usleep_range(1000, 2000);
	/* set brightness A and B */
	rval |= lm3630a_write(pchip, REG_BRT_A, pdata->leda_init_brt);
	rval |= lm3630a_write(pchip, REG_BRT_B, pdata->ledb_init_brt);

	if (rval < 0)
		dev_err(pchip->dev, "i2c failed to access register\n");
	return rval;
}

#if 0
/* interrupt handling */
static void lm3630a_delayed_func(struct work_struct *work)
{
	int rval;
	struct lm3630a_chip *pchip;

	pchip = container_of(work, struct lm3630a_chip, work.work);

	rval = lm3630a_read(pchip, REG_INT_STATUS);
	if (rval < 0) {
		dev_err(pchip->dev,
			"i2c failed to access REG_INT_STATUS Register\n");
		return;
	}

	dev_info(pchip->dev, "REG_INT_STATUS Register is 0x%x\n", rval);
}

static irqreturn_t lm3630a_isr_func(int irq, void *chip)
{
	int rval;
	struct lm3630a_chip *pchip = chip;
	unsigned long delay = msecs_to_jiffies(INT_DEBOUNCE_MSEC);

	queue_delayed_work(pchip->irqthread, &pchip->work, delay);

	rval = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (rval < 0) {
		dev_err(pchip->dev, "i2c failed to access register\n");
		return IRQ_NONE;
	}
	return IRQ_HANDLED;
}

static int lm3630a_intr_config(struct lm3630a_chip *pchip)
{
	int rval;

	rval = lm3630a_write(pchip, REG_INT_EN, 0x87);
	if (rval < 0)
		return rval;

	INIT_DELAYED_WORK(&pchip->work, lm3630a_delayed_func);
	pchip->irqthread = create_singlethread_workqueue("lm3630a-irqthd");
	if (!pchip->irqthread) {
		dev_err(pchip->dev, "create irq thread fail\n");
		return -ENOMEM;
	}
	if (request_threaded_irq
	    (pchip->irq, NULL, lm3630a_isr_func,
	     IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "lm3630a_irq", pchip)) {
		dev_err(pchip->dev, "request threaded irq fail\n");
		destroy_workqueue(pchip->irqthread);
		return -ENOMEM;
	}
	return rval;
}
#endif

static void lm3630a_pwm_ctrl(struct lm3630a_chip *pchip, int br, int br_max)
{
#if 0
	unsigned int period = pwm_get_period(pchip->pwmd);
	unsigned int duty = br * period / br_max;

	pwm_config(pchip->pwmd, duty, period);
	if (duty)
		pwm_enable(pchip->pwmd);
	else
		pwm_disable(pchip->pwmd);
#endif
}

/* update and get brightness */
static int lm3630a_bank_a_update_status(struct backlight_device *bl)
{
	int ret;
	struct lm3630a_chip *pchip = bl_get_data(bl);
#if 0
	enum lm3630a_pwm_ctrl pwm_ctrl = pchip->pdata->pwm_ctrl;

	/* pwm control */
	if ((pwm_ctrl & LM3630A_PWM_BANK_A) != 0) {
		lm3630a_pwm_ctrl(pchip, bl->props.brightness,
				 bl->props.max_brightness);
		return bl->props.brightness;
	}
#endif

	/* disable sleep */
	ret = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (ret < 0)
		goto out_i2c_err;
	usleep_range(1000, 2000);
//	printk ("[%s-%d] brightness %d, power %d\n", __func__,__LINE__,bl->props.brightness,bl->props.power);

	/* minimum brightness is 0x04 */
	ret = lm3630a_write(pchip, REG_BRT_A, bl->props.brightness);
	if (0x20 > bl->props.power)
		ret |= lm3630a_update(pchip, REG_I_A, 0x1F, bl->props.power);

	if (bl->props.brightness < 0x4)
		ret |= lm3630a_update(pchip, REG_CTRL, LM3630A_LEDA_ENABLE, 0);
	else
		ret |= lm3630a_update(pchip, REG_CTRL,
				      LM3630A_LEDA_ENABLE, LM3630A_LEDA_ENABLE);
	if (ret < 0)
		goto out_i2c_err;
	return bl->props.brightness;

out_i2c_err:
	dev_err(pchip->dev, "i2c failed to access\n");
	return bl->props.brightness;
}

static int lm3630a_bank_a_get_brightness(struct backlight_device *bl)
{
	int brightness, rval;
	struct lm3630a_chip *pchip = bl_get_data(bl);
#if 0
	enum lm3630a_pwm_ctrl pwm_ctrl = pchip->pdata->pwm_ctrl;

	if ((pwm_ctrl & LM3630A_PWM_BANK_A) != 0) {
		rval = lm3630a_read(pchip, REG_PWM_OUTHIGH);
		if (rval < 0)
			goto out_i2c_err;
		brightness = (rval & 0x01) << 8;
		rval = lm3630a_read(pchip, REG_PWM_OUTLOW);
		if (rval < 0)
			goto out_i2c_err;
		brightness |= rval;
		goto out;
	}
#endif
	/* disable sleep */
	rval = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (rval < 0)
		goto out_i2c_err;
	usleep_range(1000, 2000);
	rval = lm3630a_read(pchip, REG_BRT_A);
	if (rval < 0)
		goto out_i2c_err;
	brightness = rval;

out:
	bl->props.brightness = brightness;
	return bl->props.brightness;
out_i2c_err:
	dev_err(pchip->dev, "i2c failed to access register\n");
	return 0;
}

static const struct backlight_ops lm3630a_bank_a_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3630a_bank_a_update_status,
	.get_brightness = lm3630a_bank_a_get_brightness,
};

/* update and get brightness */
static int lm3630a_bank_b_update_status(struct backlight_device *bl)
{
	int ret;
	struct lm3630a_chip *pchip = bl_get_data(bl);
#if 0
	enum lm3630a_pwm_ctrl pwm_ctrl = pchip->pdata->pwm_ctrl;

	/* pwm control */
	if ((pwm_ctrl & LM3630A_PWM_BANK_B) != 0) {
		lm3630a_pwm_ctrl(pchip, bl->props.brightness,
				 bl->props.max_brightness);
		return bl->props.brightness;
	}
#endif
	/* disable sleep */
	ret = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (ret < 0)
		goto out_i2c_err;
	usleep_range(1000, 2000);
//	printk ("[%s-%d] brightness %d, power %d\n", __func__,__LINE__,bl->props.brightness,bl->props.power);
	/* minimum brightness is 0x04 */
	ret = lm3630a_write(pchip, REG_BRT_B, bl->props.brightness);

	if (0x20 > bl->props.power)
		ret |= lm3630a_write(pchip, REG_I_B, bl->props.power);

	if (bl->props.brightness < 0x4)
		ret |= lm3630a_update(pchip, REG_CTRL, LM3630A_LEDB_ENABLE, 0);
	else
		ret |= lm3630a_update(pchip, REG_CTRL,
				      LM3630A_LEDB_ENABLE, LM3630A_LEDB_ENABLE);
	if (ret < 0)
		goto out_i2c_err;
	return bl->props.brightness;

out_i2c_err:
	dev_err(pchip->dev, "i2c failed to access REG_CTRL\n");
	return bl->props.brightness;
}

static int lm3630a_bank_b_get_brightness(struct backlight_device *bl)
{
	int brightness, rval;
	struct lm3630a_chip *pchip = bl_get_data(bl);
#if 0
	enum lm3630a_pwm_ctrl pwm_ctrl = pchip->pdata->pwm_ctrl;

	if ((pwm_ctrl & LM3630A_PWM_BANK_B) != 0) {
		rval = lm3630a_read(pchip, REG_PWM_OUTHIGH);
		if (rval < 0)
			goto out_i2c_err;
		brightness = (rval & 0x01) << 8;
		rval = lm3630a_read(pchip, REG_PWM_OUTLOW);
		if (rval < 0)
			goto out_i2c_err;
		brightness |= rval;
		goto out;
	}
#endif
	/* disable sleep */
	rval = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (rval < 0)
		goto out_i2c_err;
	usleep_range(1000, 2000);
	rval = lm3630a_read(pchip, REG_BRT_B);
	if (rval < 0)
		goto out_i2c_err;
	brightness = rval;

out:
	bl->props.brightness = brightness;
	return bl->props.brightness;
out_i2c_err:
	dev_err(pchip->dev, "i2c failed to access register\n");
	return 0;
}

static const struct backlight_ops lm3630a_bank_b_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3630a_bank_b_update_status,
	.get_brightness = lm3630a_bank_b_get_brightness,
};

static ssize_t led_a_per_info(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int val = gpchip->bleda->props.brightness;
	if (val) {
		for (val=0;val < sizeof(bank_a_percent);val++) {
			if (gpchip->bleda->props.brightness == bank_a_percent[val])
				break;
		}
		val++;
	}
	sprintf (buf, "%d", val);
	return strlen(buf);
}

static ssize_t led_a_per_ctrl(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	int val = simple_strtoul (buf, NULL, 10);

	if (val == 0)
		gpchip->bleda->props.brightness = 0;
	else {
		if (val > sizeof (bank_a_percent))
			val = sizeof(bank_a_percent);
		gpchip->bleda->props.brightness = bank_a_percent[val-1];
	}
//	printk ("[%s-%d] set %d\%, brightness %d\n", __func__, __LINE__, val, gpchip->bleda->props.brightness);
	lm3630a_bank_a_update_status (gpchip->bleda);
	return count;
}

static DEVICE_ATTR (percent, 0644, led_a_per_info, led_a_per_ctrl);

static ssize_t led_b_per_info(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	int val = gpchip->bledb->props.brightness;
	if (val) {
		for (val=0;val < sizeof(bank_b_percent);val++) {
			if (gpchip->bledb->props.brightness == bank_b_percent[val])
				break;
		}
		val++;
	}
	sprintf (buf, "%d", val);
	return strlen(buf);
}

static ssize_t led_b_per_ctrl(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	int val = simple_strtoul (buf, NULL, 10);

	if (val == 0)
		gpchip->bledb->props.brightness = 0;
	else {
		if (val > sizeof (bank_a_percent))
			val = sizeof(bank_a_percent);
		gpchip->bledb->props.brightness = bank_b_percent[val-1];
	}
//	printk ("[%s-%d] set %d\%, brightness %d\n", __func__, __LINE__, val, gpchip->bledb->props.brightness);
	lm3630a_bank_b_update_status (gpchip->bledb);
	return count;
}

static struct device_attribute dev_attr_b_percent = __ATTR(percent, 0644,led_b_per_info, led_b_per_ctrl);

static void lm3630a_set_FL (unsigned char led_A_current, unsigned char led_A_brightness,
		unsigned char led_B_current, unsigned char led_B_brightness)
{
	int ret;
	printk ("[%s-%d] led A %d, led B %d\n", __func__,__LINE__, led_A_brightness, led_B_brightness);
	/* disable sleep */
	ret = lm3630a_update(gpchip, REG_CTRL, 0x80, 0x00);
	usleep_range(1000, 2000);

	/* minimum brightness is 0x04 */
	ret = lm3630a_write(gpchip, REG_BRT_A, led_A_brightness);
	if (0x20 > led_A_current)
		ret |= lm3630a_update(gpchip, REG_I_A, 0x1F, led_A_current);

	/* minimum brightness is 0x04 */
	ret = lm3630a_write(gpchip, REG_BRT_B, led_B_brightness);
	if (0x20 > led_B_current)
		ret |= lm3630a_write(gpchip, REG_I_B, led_B_current);

	lm3630a_update(gpchip, REG_CTRL,
			LM3630A_LEDA_ENABLE|LM3630A_LEDB_ENABLE, LM3630A_LEDA_ENABLE|LM3630A_LEDB_ENABLE);
}

int fl_lm3630a_percentage (int iFL_Percentage)
{
	int iFL_table = gpchip->frontlight_table;

	if (LM3630A_COLOR_TEMPERATURES <= iFL_table) {
		printk ("[%s-%d] Front light table %d out of range.\n", __func__,__LINE__, iFL_table);
		return -1;
	}
	if (0 == iFL_Percentage)
		lm3630a_set_FL (0, 0, 0, 0);
	else
		lm3630a_set_FL (4, lm3630a_fl_table[iFL_table][0][iFL_Percentage-1],
			7, lm3630a_fl_table[iFL_table][1][iFL_Percentage-1]);
	return 0;
}

int fl_lm3630a_set_color (int iFL_color)
{
	if (LM3630A_COLOR_TEMPERATURES <= iFL_color) {
		printk ("[%s-%d] Front light table %d out of range.\n", __func__,__LINE__, iFL_color);
		return -1;
	}
	gpchip->frontlight_table = iFL_color;
	fl_lm3630a_percentage (gpchip->bled->props.brightness);
}

/* update and get brightness */
static int lm3630a_update_status(struct backlight_device *bl)
{
	fl_lm3630a_percentage (bl->props.brightness);
	return bl->props.brightness;
}

static int lm3630a_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static const struct backlight_ops lm3630a_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3630a_update_status,
	.get_brightness = lm3630a_get_brightness,
};

static ssize_t led_color_get(struct device *dev, struct device_attribute *attr,
			char *buf)
{
	sprintf (buf, "%d", gpchip->frontlight_table);
	return strlen(buf);
}

static ssize_t led_color_set(struct device *dev, struct device_attribute *attr,
		       const char *buf, size_t count)
{
	int val = simple_strtoul (buf, NULL, 10);

	gpchip->frontlight_table = val;
	fl_lm3630a_percentage (gpchip->bled->props.brightness);

	return count;
}

static DEVICE_ATTR (color, 0644, led_color_get, led_color_set);

static int lm3630a_backlight_register(struct lm3630a_chip *pchip)
{
	struct backlight_properties props;
	struct lm3630a_platform_data *pdata = pchip->pdata;
	int rval;

	props.type = BACKLIGHT_RAW;
	if (pdata->leda_ctrl != LM3630A_LEDA_DISABLE) {
		props.brightness = pdata->leda_init_brt;
		props.max_brightness = pdata->leda_max_brt;
		props.power = pdata->leda_full_scale;
		pchip->bleda =
			backlight_device_register("lm3630a_leda", pchip->dev, pchip,
							       &lm3630a_bank_a_ops, &props);
		if (IS_ERR(pchip->bleda))
			return PTR_ERR(pchip->bleda);

		rval = device_create_file(&pchip->bleda->dev, &dev_attr_percent);
		if (rval < 0) {
			dev_err(&pchip->bleda->dev, "fail : backlight percent create.\n");
			return rval;
		}
	}

	if ((pdata->ledb_ctrl != LM3630A_LEDB_DISABLE) &&
	    (pdata->ledb_ctrl != LM3630A_LEDB_ON_A)) {
		props.brightness = pdata->ledb_init_brt;
		props.max_brightness = pdata->ledb_max_brt;
		props.power = pdata->ledb_full_scale;
		pchip->bledb =
			backlight_device_register("lm3630a_ledb", pchip->dev, pchip,
							       &lm3630a_bank_b_ops, &props);
		if (IS_ERR(pchip->bledb))
			return PTR_ERR(pchip->bledb);

		rval = device_create_file(&pchip->bledb->dev, &dev_attr_b_percent);
		if (rval < 0) {
			dev_err(&pchip->bledb->dev, "fail : backlight percent create.\n");
			return rval;
		}
	}
	props.brightness = 100;
	props.max_brightness = 100;
	pchip->bled =
			backlight_device_register("lm3630a_led", pchip->dev, pchip,
							       &lm3630a_ops, &props);
	if (IS_ERR(pchip->bled))
		return PTR_ERR(pchip->bled);

	rval = device_create_file(&pchip->bled->dev, &dev_attr_color);
	if (rval < 0) {
		dev_err(&pchip->bled->dev, "fail : backlight color create.\n");
		return rval;
	}

	return 0;
}

static int lm3630a_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct lm3630a_platform_data *pdata = dev_get_platdata(&client->dev);
	struct lm3630a_chip *pchip;
	int rval;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "fail : i2c functionality check\n");
		return -EOPNOTSUPP;
	}

	gpchip = pchip = devm_kzalloc(&client->dev, sizeof(struct lm3630a_chip),
			     GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;
	pchip->dev = &client->dev;

	pchip->client = client;

	i2c_set_clientdata(client, pchip);
	if (pdata == NULL) {
		pdata = devm_kzalloc(pchip->dev,
				     sizeof(struct lm3630a_platform_data),
				     GFP_KERNEL);
		if (pdata == NULL)
			return -ENOMEM;
		/* default values */
		pdata->leda_ctrl = LM3630A_LEDA_ENABLE;
		pdata->ledb_ctrl = LM3630A_LEDB_ENABLE;
		pdata->leda_max_brt = LM3630A_MAX_BRIGHTNESS;
		pdata->ledb_max_brt = LM3630A_MAX_BRIGHTNESS;
		pdata->leda_init_brt = 0;
		pdata->ledb_init_brt = 0;
		pdata->leda_full_scale = LM3630A_DEF_FULLSCALE;
		pdata->ledb_full_scale = LM3630A_DEF_FULLSCALE;
		pdata->pwm_ctrl = LM3630A_PWM_DISABLE;
	}
	pchip->pdata = pdata;

	/* chip initialize */
	rval = lm3630a_chip_init(pchip);
	if (rval < 0) {
		dev_err(&client->dev, "fail : init chip\n");
		return rval;
	}
	/* backlight register */
	rval = lm3630a_backlight_register(pchip);
	if (rval < 0) {
		dev_err(&client->dev, "fail : backlight register.\n");
		return rval;
	}

#if 0
	/* pwm */
	if (pdata->pwm_ctrl != LM3630A_PWM_DISABLE) {
		pchip->pwmd = devm_pwm_get(pchip->dev, "lm3630a-pwm");
		if (IS_ERR(pchip->pwmd)) {
			dev_err(&client->dev, "fail : get pwm device\n");
			return PTR_ERR(pchip->pwmd);
		}
	}

	pchip->pwmd->period = pdata->pwm_period;

	/* interrupt enable  : irq 0 is not allowed */
	pchip->irq = client->irq;
	if (pchip->irq) {
		rval = lm3630a_intr_config(pchip);
		if (rval < 0)
			return rval;
	}
#endif
	dev_info(&client->dev, "LM3630A backlight register OK.\n");
	return 0;
}

static int lm3630a_remove(struct i2c_client *client)
{
	int rval;
	struct lm3630a_chip *pchip = i2c_get_clientdata(client);

	rval = lm3630a_write(pchip, REG_BRT_A, 0);
	if (rval < 0)
		dev_err(pchip->dev, "i2c failed to access register\n");

	rval = lm3630a_write(pchip, REG_BRT_B, 0);
	if (rval < 0)
		dev_err(pchip->dev, "i2c failed to access register\n");

#if 0
	if (pchip->irq) {
		free_irq(pchip->irq, pchip);
		flush_workqueue(pchip->irqthread);
		destroy_workqueue(pchip->irqthread);
	}
#endif
	return 0;
}

static const struct i2c_device_id lm3630a_id[] = {
	{LM3630A_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lm3630a_id);

static struct i2c_driver lm3630a_i2c_driver = {
	.driver = {
		   .name = LM3630A_NAME,
		   },
	.probe = lm3630a_probe,
	.remove = lm3630a_remove,
	.id_table = lm3630a_id,
};

static int __init lm3630a_init(void)
{
	return i2c_add_driver(&lm3630a_i2c_driver);
}

static void __exit lm3630a_exit(void)
{
	i2c_del_driver(&lm3630a_i2c_driver);
}

module_init(lm3630a_init);
module_exit(lm3630a_exit);

MODULE_DESCRIPTION("Texas Instruments Backlight driver for LM3630A");
MODULE_AUTHOR("Daniel Jeong <gshark.jeong@gmail.com>");
MODULE_AUTHOR("LDD MLP <ldd-mlp@list.ti.com>");
MODULE_LICENSE("GPL v2");
