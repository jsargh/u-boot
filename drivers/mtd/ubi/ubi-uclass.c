// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 Juha Sarlin
 */
#include <common.h>
#include <dm.h>
#include <dm/lists.h>
#include <dm/uclass-internal.h>
#include <dm/device-internal.h>
#include <dm/root.h>
#include "ubi.h"

/** Get udevice for a UBI device. */
int ubi_get(int ubi_num, struct udevice **devp)
{
	struct ubi_device *ubi;
	struct udevice *dev;
	int ret;

	ubi = ubi_get_device(ubi_num);
	if (!ubi)
		return -ENODEV;
	ret = uclass_find_device_by_name(UCLASS_UBI, ubi->ubi_name, &dev);
	if (ret) {
		ret = device_bind_driver(dm_root(), "ubi",
					 ubi->ubi_name, &dev);
		if (ret) {
			ubi_put_device(ubi);
			return ret;
		}
		dev->priv = ubi;
	} else {
		ubi_put_device(ubi);
	}
	*devp = dev;
	return 0;
}

/** Remove udevice for a UBI device. */
int ubi_put(int ubi_num)
{
	struct ubi_device *ubi;
	struct udevice *dev;
	int ret;

	ubi = ubi_get_device(ubi_num);
	if (!ubi)
		return 0;
	ubi_put_device(ubi);
	ret = uclass_find_device_by_name(UCLASS_UBI, ubi->ubi_name, &dev);
	if (!ret) {
		device_unbind(dev);
	}
	return 0;
}

static int ubi_unbind(struct udevice *dev)
{
	if (dev->priv) {
		ubi_put_device(dev->priv);
		dev->priv = NULL;
	}
	return 0;
}

U_BOOT_DRIVER(ubi) = {
	.id		= UCLASS_UBI,
	.name		= "ubi",
	.unbind		= ubi_unbind,
};

UCLASS_DRIVER(ubi) = {
	.id		= UCLASS_UBI,
	.name		= "ubi",
};
