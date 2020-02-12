/********************************************
 * Copyright (C) 2019-2020 Intel Corporation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 ********************************************/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/stringify.h>
#include <linux/acpi.h>

#include "bdat_version.h"

struct BDAT_HEADER_STRUCTURE {
	u8   BiosDataSignature[8];	// "BDATHEAD"
	u32  BiosDataStructSize;	// sizeof BDAT_STRUCTURE
	u16  Crc16;			// 16-bit CRC of BDAT_STRUCTURE (calculated with 0 in this field)
	u16  Reserved;
	u16  PrimaryVersion;		// Primary version
	u16  SecondaryVersion;	// Secondary version
	u32  OemOffset;		// Optional offset to OEM-defined structure
	u32  Reserved1;
	u32  Reserved2;
};

static void    *bdat_virt;
static uint32_t bdat_size;

static ssize_t bdat_read(struct file *filp,
			 struct kobject *kobj,
			 struct bin_attribute *attr,
			 char *buf,
			 loff_t offset,
			 size_t count)
{
	ssize_t ret;

	if (bdat_size > 0)
		ret = memory_read_from_buffer(buf,
					      count,
					      &offset,
					      bdat_virt,
					      bdat_size);
	else
		ret = 0;

	return ret;
}

static struct bin_attribute bdat_attr = {
	.attr = {
		.name = "bdat",
		.mode = 0400
	},
	.size = 0,
	.read = bdat_read,
	.write = NULL,
	.mmap = NULL,
	.private = (void *)0
};

int init_bdat_sysfs(void)
{
	struct acpi_table_header *table_header = NULL;
	int ret;
	uint64_t bdat_phys_addr;
	struct BDAT_HEADER_STRUCTURE *header;

	ret = acpi_get_table("BDAT", 0, &table_header);
	if (ACPI_FAILURE(ret)) {
		pr_err("Failed to find BDAT acpi table\n");
		return -ENODEV;
	}

	if (table_header->length != 48) {
		pr_err("Wrong BDAT table size %d instead of 48\n", table_header->length);
		ret = -EFAULT;
		goto done;
	}

	bdat_phys_addr = *((uint64_t *)(((uintptr_t)table_header) + 40));
	pr_err("Found BDAT acpi table at=0x%llx\n", bdat_phys_addr);

	bdat_virt = memremap(bdat_phys_addr, PAGE_SIZE, MEMREMAP_WB);
	if (!bdat_virt) {
		pr_err("Failed to map BDAT table\n");
		ret = -EFAULT;
		goto done;
	}

	header = (struct BDAT_HEADER_STRUCTURE *)bdat_virt;
	if (strncmp(header->BiosDataSignature, "BDATHEAD", 8) != 0) {
		pr_err("BDAT wrong signature\n");
		ret = -EFAULT;
		goto done;
	}
	bdat_size = header->BiosDataStructSize;

	if (bdat_size > PAGE_SIZE) {
		memunmap(bdat_virt);
		bdat_virt = memremap(bdat_phys_addr, PAGE_ALIGN(bdat_size), MEMREMAP_WB);
		if (!bdat_virt) {
			pr_err("Failed to map BDAT table size=%d\n", bdat_size);
			ret = -EFAULT;
			goto done;
		}
	}

	bdat_attr.size = bdat_size;

	ret = sysfs_create_bin_file(&THIS_MODULE->mkobj.kobj, &bdat_attr);
	if (ret) {
		pr_err("Failed to create bdat sysfs file\n");
		memunmap(bdat_virt);
		bdat_virt = NULL;
		bdat_size = 0;
	}

done:
	acpi_put_table(table_header);

	return ret;
}

int bdat_init_module(void)
{
	pr_debug("module (version %s) started\n", BDAT_VERSION);

	init_bdat_sysfs();

	return 0;
}

void bdat_cleanup(void)
{
	pr_debug("Cleaning Up the Module\n");

	if (bdat_virt) {
		sysfs_remove_bin_file(&THIS_MODULE->mkobj.kobj, &bdat_attr);
		memunmap(bdat_virt);
	}
}

module_init(bdat_init_module);
module_exit(bdat_cleanup);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("BDAT sysfs module");
MODULE_AUTHOR("Intel Corporation");
MODULE_VERSION(BDAT_VERSION);
//#ifdef DEBUG
//MODULE_INFO(git_hash, SPH_GIT_HASH);
//#endif
