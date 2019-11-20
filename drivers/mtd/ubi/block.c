// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 Juha Sarlin
 * Copyright (c) 2014 Ezequiel Garcia
 * Copyright (c) 2011 Free Electrons
 */

/*
 * Read-only block devices on top of UBI volumes
 *
 * A simple implementation to allow a block device to be layered on top of a
 * UBI volume. The implementation is provided by creating a static 1-to-1
 * mapping between the block device and the UBI volume.
 *
 * The addressed byte is obtained from the addressed block sector, which is
 * mapped linearly into the corresponding LEB:
 *
 *   LEB number = addressed byte / LEB size
 */

#include <div64.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <ubi_uboot.h>
#include "ubi.h"

static struct udevice *ubiblock_find(int ubi_num, const char *volume)
{
	struct udevice *dev;
	char name[UBI_VOL_NAME_MAX + 8];

	snprintf(name, sizeof name, "ubi%d.%s", ubi_num, volume);
	for (blk_first_device(IF_TYPE_UBI, &dev); dev; blk_next_device(&dev)) {
		if (!strcmp(dev->name, name)) {
			return dev;
		}
	}
	return NULL;
}

static unsigned long ubiblock_read(struct udevice *dev, lbaint_t start,
				   lbaint_t blkcnt, void *buffer)
{
	struct ubi_volume_desc *desc = dev->priv;
	struct ubi_volume *vol = desc->vol;
	struct blk_desc *blk = dev_get_uclass_platdata(dev);
	int ret, leb, offset, bytes_left, to_read;
	int leb_size = vol->ubi->leb_size;
	int blksz = blk->blksz;
	u64 pos;

	to_read = blkcnt * blksz;
	pos = start * blksz;

	/* Get LEB:offset address to read from */
	offset = do_div(pos, leb_size);
	leb = pos;
	bytes_left = to_read;

	while (bytes_left) {
		/*
		 * We can only read one LEB at a time. Therefore if the read
		 * length is larger than one LEB size, we split the operation.
		 */
		if (offset + to_read > leb_size)
			to_read = leb_size - offset;

		ret = ubi_read(desc, leb, buffer, offset, to_read);
		if (ret < 0) {
			pr_err("%s: ubi_read failed (%d)\n", __func__, ret);
			return ret;
		}
		bytes_left -= to_read;
		to_read = bytes_left;
		leb += 1;
		offset = 0;
	}
	return blkcnt;
}

/**
 * Create BLK device on UBI volume.
 *
 * @return devnum or -errno
 */
int ubiblock_create(int ubi_num, const char *volume)
{
	struct udevice *dev, *parent;
	struct ubi_volume_desc *desc;
	struct blk_desc *blk;
	struct ubi_volume *vol;
	int ret, leb_size, blksz, size;

	ret = ubi_get(ubi_num, &parent);
	if (ret)
		return ret;
	desc = ubi_open_volume_nm(ubi_num, volume, UBI_READONLY);
	if (IS_ERR(desc)) {
		return PTR_ERR(desc);
	}
	dev = ubiblock_find(ubi_num, volume);
	if (!dev) {
		vol = desc->vol;
		leb_size = vol->ubi->leb_size;
		/* Block size must be power of 2 */
		blksz = leb_size & ~(leb_size - 1);
		size = vol->reserved_pebs * (leb_size / blksz);
		ret = blk_create_devicef(parent, "ubiblock", volume,
					 IF_TYPE_UBI, -1, blksz, size, &dev);
		if (ret)
			return ret;
		dev->priv = desc;
	}
	blk = dev_get_uclass_platdata(dev);
	return blk->devnum;
}

static int ubiblock_unbind(struct udevice *dev)
{
	if (dev->priv) {
		ubi_close_volume(dev->priv);
		dev->priv = NULL;
	}
	return 0;
}

static const struct blk_ops ubiblock_ops = {
	.read	= ubiblock_read,
};

U_BOOT_DRIVER(ubiblock) = {
	.name			= "ubiblock",
	.id			= UCLASS_BLK,
	.unbind			= ubiblock_unbind,
	.ops			= &ubiblock_ops,
};
