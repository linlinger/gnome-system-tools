/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  
 * Copyright (C) 2001 Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 *
 * Authors: Alvaro Pe�a Gonzalez <apg@esware.com>
 *          Carlos Garcia Campos <elkalmail@yahoo.es>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <libgnome/gnome-i18n.h>

#include "disks-partition.h"

#define PARENT_TYPE G_TYPE_OBJECT


enum {
	PROP_0,
	PROP_NAME,
	PROP_DEVICE,
	PROP_TYPE,
	PROP_POINT,
	PROP_SIZE,
	PROP_FREE,
	PROP_BOOTABLE,
	PROP_INTEGRITYCHECK,
	PROP_MOUNTED,
	PROP_LISTED,
	PROP_DETECTED
};

struct _GstDisksPartitionPriv
{
	gchar    *name;
	gchar    *device;
	GstPartitionTypeFs type;
	gchar    *point;
	gulong   size;
	gulong   free;
	gboolean bootable;
	gboolean integritycheck;
	gboolean mounted;
	gboolean listed;
	gboolean detected;
};

static void partition_init       (GstDisksPartition      *storage);
static void partition_class_init (GstDisksPartitionClass *klass);
static void partition_finalize   (GObject                       *object);

static void partition_set_property (GObject  *object, guint prop_id,
				    const GValue *value, GParamSpec *spec);
static void partition_get_property (GObject  *object, guint prop_id,
				    GValue *value, GParamSpec *spec);

static GObjectClass *parent_class = NULL;

#define GST_PARTITION_TYPE (gst_partition_typefs_get_type ())

static GType
gst_partition_typefs_get_type (void)
{
	static GType partition_typefs_type = 0;
	static GEnumValue partition_typefs[] = {
		{ PARTITION_TYPE_EXT2,     "0", NULL },
		{ PARTITION_TYPE_EXT3,     "1", NULL },
		{ PARTITION_TYPE_REISERFS, "2", NULL },
		{ PARTITION_TYPE_XFS,      "3", NULL },
		{ PARTITION_TYPE_JFS,      "4", NULL },
		{ PARTITION_TYPE_VFAT,     "5", NULL },
		{ PARTITION_TYPE_NTFS,     "6", NULL },
		{ PARTITION_TYPE_SWAP,     "7", NULL },
		{ PARTITION_TYPE_FREE,     "8", NULL },
		{ PARTITION_TYPE_UNKNOWN,  "9", NULL },
	};
	if (!partition_typefs_type) {
		partition_typefs_type = g_enum_register_static ("GstPartitionTypeFs", partition_typefs);
	}
	return partition_typefs_type;
}


GType
gst_disks_partition_get_type (void)
{
	static GType type = 0;
	
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GstDisksPartitionClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) partition_class_init,
			NULL,
			NULL,
			sizeof (GstDisksPartition),
			0,
			(GInstanceInitFunc) partition_init
		};
		type = g_type_register_static (PARENT_TYPE, "GstDisksPartition",
					       &info, 0);
	   }
	   return type;
}

static void
partition_init (GstDisksPartition *part)
{
	g_return_if_fail (GST_IS_DISKS_PARTITION (part));
	
	part->priv = g_new0 (GstDisksPartitionPriv, 1);
	part->priv->name = g_strdup (_("Unknown"));
	part->priv->size = 0;
}

static void
partition_class_init (GstDisksPartitionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->set_property = partition_set_property;
	object_class->get_property = partition_get_property;

	g_object_class_install_property (object_class, PROP_NAME,
					 g_param_spec_string ("name", NULL, NULL,
							      NULL, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_DEVICE,
					 g_param_spec_string ("device", NULL, NULL,
							      NULL, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_TYPE,
					 g_param_spec_enum ("type", NULL, NULL, 
							    GST_PARTITION_TYPE,
							    0,
							    G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_POINT,
					 g_param_spec_string ("point", NULL, NULL,
							      NULL, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_SIZE,
					 g_param_spec_ulong ("size", NULL, NULL,
							     0, G_MAXULONG, 0,
							     G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_FREE,
					 g_param_spec_ulong ("free", NULL, NULL,
							     0, G_MAXULONG, 0,
							     G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_BOOTABLE,
					 g_param_spec_boolean ("bootable", NULL, NULL, 
							      FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_INTEGRITYCHECK,
					 g_param_spec_boolean ("integritycheck", NULL, NULL, 
							      FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_MOUNTED,
					 g_param_spec_boolean ("mounted", NULL, NULL, 
							      FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_LISTED,
					 g_param_spec_boolean ("listed", NULL, NULL, 
							      FALSE, G_PARAM_READWRITE));
	g_object_class_install_property (object_class, PROP_DETECTED,
					 g_param_spec_boolean ("detected", NULL, NULL, 
							      FALSE, G_PARAM_READWRITE));
	
	object_class->finalize = partition_finalize;
}

static void
partition_finalize (GObject *object)
{
	GstDisksPartition *part = GST_DISKS_PARTITION (object);
	g_return_if_fail (GST_IS_DISKS_PARTITION (part));

	if (part->priv) {
		g_free (part->priv);
		part->priv = NULL;
	}
	
	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}

GstDisksPartition*
gst_disks_partition_new (void)
{
	GstDisksPartition *part;
	
	part = g_object_new (GST_TYPE_DISKS_PARTITION, NULL);

	return part;
}

static gchar *
get_partition_name (const gchar *device)
{
	gchar *last;

	last = g_strrstr (device, "/") + 4;
	if (last)
		return _(g_strdup_printf ("Partition %s", last));
	else
		return _(g_strdup ("Unknown"));
}
		

static void
partition_set_property (GObject  *object, guint prop_id, const GValue *value,
				GParamSpec *spec)
{
	GstDisksPartition *part;

	g_return_if_fail (GST_IS_DISKS_PARTITION (object));

	part = GST_DISKS_PARTITION (object);

	switch (prop_id) {
	case PROP_NAME:
		if (part->priv->name) g_free (part->priv->name);
		part->priv->name = g_value_dup_string (value);
		break;
	case PROP_DEVICE:
		if (part->priv->device) g_free (part->priv->device);
		part->priv->device = g_value_dup_string (value);
		g_object_set (G_OBJECT (part), "name",
			      get_partition_name (part->priv->device),
			      NULL);
		break;
	case PROP_TYPE:
		part->priv->type = g_value_get_enum (value);
		if (part->priv->type == PARTITION_TYPE_SWAP) {
			g_object_set (G_OBJECT (part), "name",
				      _("Swap Partition"),
				      NULL);
		}
		break;
	case PROP_POINT:
		if (part->priv->point) g_free (part->priv->point);
		part->priv->point = g_value_dup_string (value);
		break;
	case PROP_SIZE:
		part->priv->size = g_value_get_ulong (value);
		break;
	case PROP_FREE:
		part->priv->free = g_value_get_ulong (value);
		break;
	case PROP_BOOTABLE:
		part->priv->bootable = g_value_get_boolean (value);
		break;
	case PROP_INTEGRITYCHECK:
		part->priv->integritycheck = g_value_get_boolean (value);
		break;
	case PROP_MOUNTED:
		part->priv->mounted = g_value_get_boolean (value);
		break;
	case PROP_LISTED:
		part->priv->listed = g_value_get_boolean (value);
		break;
	case PROP_DETECTED:
		part->priv->detected = g_value_get_boolean (value);
		break;
	default:
		break;
	}
}

static void
partition_get_property (GObject  *object, guint prop_id, GValue *value,
				GParamSpec *spec)
{
	GstDisksPartition *part;

	g_return_if_fail (GST_IS_DISKS_PARTITION (object));

	part = GST_DISKS_PARTITION (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, part->priv->name);
		break;
	case PROP_DEVICE:
		g_value_set_string (value, part->priv->device);
		break;
	case PROP_TYPE:
		g_value_set_enum (value, part->priv->type);
		break;
	case PROP_POINT:
		g_value_set_string (value, part->priv->point);
		break;
	case PROP_SIZE:
		g_value_set_ulong (value, part->priv->size);
		break;
	case PROP_FREE:
		g_value_set_ulong (value, part->priv->free);
		break;
	case PROP_BOOTABLE:
		g_value_set_boolean (value, part->priv->bootable);
		break;
	case PROP_INTEGRITYCHECK:
		g_value_set_boolean (value, part->priv->integritycheck);
		break;
	case PROP_MOUNTED:
		g_value_set_boolean (value, part->priv->mounted);
		break;
	case PROP_LISTED:
		g_value_set_boolean (value, part->priv->listed);
		break;
	case PROP_DETECTED:
		g_value_set_boolean (value, part->priv->detected);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, spec);
	}
}

gchar *
gst_disks_partition_get_human_readable_typefs (GstPartitionTypeFs type)
{
	gchar *filesystems[] = {
		"Extended 2", "Extended 3",
		"Reiser FS", "XFS", "JFS",
		"FAT", "Memory Swap",
		"vfat", "NTFS"
		"Free Space",
		"Unknown"
	};

	return g_strdup (filesystems[type]);
}

GstPartitionTypeFs
gst_disks_partition_get_typefs_from_name (const gchar *name)
{
	if (g_ascii_strcasecmp (name, "ext2") == 0)
		return PARTITION_TYPE_EXT2;
	else if (g_ascii_strcasecmp (name, "ext3") == 0)
		return PARTITION_TYPE_EXT3;
	else if (g_ascii_strcasecmp (name, "reiserfs") == 0)
		return PARTITION_TYPE_REISERFS;
	else if (g_ascii_strcasecmp (name, "xfs") == 0)
		return PARTITION_TYPE_XFS;
	else if (g_ascii_strcasecmp (name, "jfs") == 0)
		return PARTITION_TYPE_JFS;
	else if ((g_ascii_strcasecmp (name, "vfat") == 0) ||
		 (g_ascii_strcasecmp (name, "fat32") == 0))
		return PARTITION_TYPE_VFAT;
	else if (g_ascii_strcasecmp (name, "ntfs") == 0)
		return PARTITION_TYPE_NTFS;
	else if (g_ascii_strcasecmp (name, "swap") == 0)
		return PARTITION_TYPE_SWAP;
	else
		return PARTITION_TYPE_UNKNOWN;
}

gchar *
gst_disks_partition_get_typefs (GstPartitionTypeFs type)
{
	switch (type) {
	case PARTITION_TYPE_EXT2:
		return g_strdup ("ext2");
	case PARTITION_TYPE_EXT3:
		return g_strdup ("ext3");
	case PARTITION_TYPE_REISERFS:
		return g_strdup ("reiserfs");
	case PARTITION_TYPE_XFS:
		return g_strdup ("xfs");
	case PARTITION_TYPE_JFS:
		return g_strdup ("jfs");
	case PARTITION_TYPE_VFAT:
		return g_strdup ("vfat");
	case PARTITION_TYPE_NTFS:
		return g_strdup ("ntfs");
	case  PARTITION_TYPE_UNKNOWN:
		return g_strdup ("auto");
	default:
		/* Partition type not mountable */
		return NULL;
	}
}
		