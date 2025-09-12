/******************************************************************************
 *
 *   This program is free software;  you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; version 2 of the License.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY;  without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 *   the GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program;  if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 *****************************************************************************/

#include <linux/device.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/regmap.h>
#include <linux/of.h>

#define DRIVER_NAME				"spisensdrv"

#define SPISENS_REG_SHIFT		4

#define SPISENS_REG_ID			(0 << SPISENS_REG_SHIFT)
#define SPISENS_REG_CTRL		(1 << SPISENS_REG_SHIFT)
#define SPISENS_REG_DATA		(2 << SPISENS_REG_SHIFT)

#define CTRL_EN_MASK				0x1

#define SPISENS_ID				0x5Au

/**
 * struct spisens - spisens device private data structure
 * regmap - register map of the SPI peripheral
 */
struct spisens {
	struct regmap *regmap;
};

/* SYSFS attributes */

/**
 * Enable device getter.
 */
static ssize_t enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spisens *data = dev_get_drvdata(dev);
	unsigned int enable;
	int err;

	err = regmap_read(data->regmap, SPISENS_REG_CTRL, &enable);
	enable &= CTRL_EN_MASK;

	return sprintf(buf, "%d\n", !!enable);
}

/**
 * Enable device setter.
 */
static ssize_t enable_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	struct spisens *data = dev_get_drvdata(dev);
	unsigned int ctrl;
	int enable;
	int err;

	err = regmap_read(data->regmap, SPISENS_REG_CTRL, &ctrl);

	sscanf(buf, "%d", &enable);

	if (!enable) {
		ctrl &= ~CTRL_EN_MASK;
	} else {
		ctrl |= CTRL_EN_MASK;
	}

	err = regmap_write(data->regmap, SPISENS_REG_CTRL, ctrl);
	if (err < 0)
		return err;

	return count;
}
static DEVICE_ATTR_RW(enable);

/**
 * Return temperature in milicelsius
 */
static ssize_t data_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct spisens *data = dev_get_drvdata(dev);
	unsigned int temperature;
	int err;

	err = regmap_read(data->regmap, SPISENS_REG_DATA, &temperature);
	temperature *= 1000;
	temperature >>= 1;

	return sprintf(buf, "%d\n", temperature);
}
static DEVICE_ATTR_RO(data);

static struct attribute *spisens_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_data.attr,
	NULL,
};

static const struct attribute_group spisens_group = {
	.attrs = spisens_attrs,
};

/**
 * Device tree match table.
 */
static const struct of_device_id spisens_of_match[] = {
	{ .compatible = "mistra,spisens", },
	{ /* end of table */ }
};
MODULE_DEVICE_TABLE(of, spisens_of_match);

static bool spisens_regmap_is_writeable(struct device *dev, unsigned int reg)
{
	return reg == SPISENS_REG_CTRL;
}

static const struct regmap_config spisens_regmap_config = {
	.reg_bits = 8,
	.reg_stride = SPISENS_REG_SHIFT,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
	.max_register = SPISENS_REG_DATA,
	.writeable_reg = spisens_regmap_is_writeable,
	.write_flag_mask = BIT(7),
};

/**
 * Driver probe function.
 * Configure interrupt and set up driver.
 */
static int spisens_probe(struct spi_device *spi)
{
	struct spisens *data;
	unsigned int regval;
	int status;

	data = devm_kzalloc(&spi->dev, sizeof(struct spisens), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	spi_set_drvdata(spi, data);

	spi->bits_per_word = 8;
	spi_setup(spi);

	data->regmap = devm_regmap_init_spi(spi, &spisens_regmap_config);
	if (IS_ERR(data->regmap)) {
		dev_err(&spi->dev, "failed to allocate register map\n");
		return PTR_ERR(data->regmap);
	}

	status = regmap_read(data->regmap, SPISENS_REG_ID, &regval);
	if (status < 0) {
		dev_err(&spi->dev, "error reading ID register\n");
		return status;
	}

	if (regval != SPISENS_ID) {
		dev_err(&spi->dev, "unexpected ID\n");
		return -ENODEV;
	}

	status = sysfs_create_group(&spi->dev.kobj, &spisens_group);
	if (status) {
		dev_info(&spi->dev, "Cannot create sysfs\n");
	}

	return 0;
}

static const struct spi_device_id spisens_id[] = {
	{ "spisens", 0 },
	{},
};
MODULE_DEVICE_TABLE(spi, spisens_id);

static struct spi_driver spisens_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = spisens_of_match,
	},
	.probe = spisens_probe,
	.id_table = spisens_id,
};
module_spi_driver(spisens_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Custom QEMU SPI sensor Driver and Device");
MODULE_AUTHOR("Strahinja Jankovic");
