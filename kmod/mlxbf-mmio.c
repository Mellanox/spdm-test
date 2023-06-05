// SPDX-License-Identifier: GPL-2.0-only or BSD-3-Clause

/*
 * Copyright (C) 2023 NVIDIA CORPORATION & AFFILIATES
 */

#include <linux/acpi.h>
#include <linux/bitfield.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/types.h>

#define DRV_VERSION "1.0"

static void __iomem *mlxbf_mmio_base;
static resource_size_t mlxbf_mmio_size;

static ssize_t mlxbf_mmio_read(struct file *filp, struct kobject *kobj,
			       struct bin_attribute *bin_attr,
			       char *buf, loff_t pos, size_t count)
{
	u32 data;

	if (count != 4 || (pos + count) > mlxbf_mmio_size)
		return -EINVAL;

	data = readl(mlxbf_mmio_base + pos);
	memcpy(buf, &data, sizeof(data));

	return count;
}

static ssize_t mlxbf_mmio_write(struct file *filp, struct kobject *kobj,
			        struct bin_attribute *bin_attr,
			        char *buf, loff_t pos, size_t count)
{
	u32 data;

	if (count != 4 || (pos + count) > mlxbf_mmio_size)
		return -EINVAL;

	memcpy(&data, buf, sizeof(data));
	writel(data, mlxbf_mmio_base + pos);

	return 0;
}

static struct bin_attribute mlxbf_mmio_sysfs_attr = {
	.attr = { .name = "psc_mbox", .mode = 0600 },
	.read = mlxbf_mmio_read,
	.write = mlxbf_mmio_write
};

static int mlxbf_mmio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *iomem;
	int rc;

	iomem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!iomem)
		return -ENXIO;
	mlxbf_mmio_size = resource_size(iomem);

	mlxbf_mmio_base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(mlxbf_mmio_base))
		return PTR_ERR(mlxbf_mmio_base);

	rc = sysfs_create_bin_file(&dev->kobj, &mlxbf_mmio_sysfs_attr);
	if (rc) {
		pr_err("Unable to create sysfs file, error %d\n", rc);
		return rc;
	}

	return 0;
}

/* Device remove function. */
static int mlxbf_mmio_remove(struct platform_device *pdev)
{
	sysfs_remove_bin_file(&pdev->dev.kobj, &mlxbf_mmio_sysfs_attr);

	return 0;
}

static const struct acpi_device_id mlxbf_mmio_acpi_match[] = {
	{ "MLNXBF3A", 0 },
	{}
};
MODULE_DEVICE_TABLE(acpi, mlxbf_mmio_acpi_match);

static struct platform_driver mlxbf_mmio_driver = {
	.probe = mlxbf_mmio_probe,
	.remove = mlxbf_mmio_remove,
	.driver = {
		.name = "mlxbf-mmio",
		.acpi_match_table = mlxbf_mmio_acpi_match,
	},
};

module_platform_driver(mlxbf_mmio_driver);

MODULE_DESCRIPTION("Mellanox BlueField-3 MMIO Driver");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_VERSION(DRV_VERSION);
