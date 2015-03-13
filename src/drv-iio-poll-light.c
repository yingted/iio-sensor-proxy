/*
 * Copyright (c) 2014 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include "drivers.h"

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define DEFAULT_POLL_TIME 0.8

typedef struct DrvData {
	ReadingsUpdateFunc  callback_func;
	gpointer            user_data;

	char               *input_path;
	guint               timeout_id;
} DrvData;

static DrvData *drv_data = NULL;

static gboolean
iio_poll_light_discover (GUdevDevice *device)
{
	char *path;
	gboolean ret;

	if (g_strcmp0 (g_udev_device_get_subsystem (device), "iio") != 0)
		return FALSE;

	path = g_build_filename (g_udev_device_get_sysfs_path (device),
				 "in_illuminance_input",
				 NULL);
	ret = g_file_test (path, G_FILE_TEST_IS_REGULAR);
	g_free (path);

	if (ret)
		g_debug ("Found IIO poll light at %s", g_udev_device_get_sysfs_path (device));
	return ret;
}

static gboolean
light_changed (void)
{
	LightReadings readings;
	gdouble level;
	char *contents;
	GError *error = NULL;

	if (g_file_get_contents (drv_data->input_path, &contents, NULL, &error)) {
		level = g_ascii_strtod (contents, NULL);
		g_free (contents);
	} else {
		g_warning ("Failed to read input level at %s: %s",
			   drv_data->input_path, error->message);
		g_error_free (error);
		return G_SOURCE_CONTINUE;
	}

	readings.level = level;
	readings.uses_lux = TRUE;
	drv_data->callback_func (&iio_poll_light, (gpointer) &readings, drv_data->user_data);

	return G_SOURCE_CONTINUE;
}

static guint
get_interval (GUdevDevice *device)
{
	gdouble time;
	char *path, *contents;

	path = g_build_filename (g_udev_device_get_sysfs_path (device),
				 "in_illuminance_integration_time",
				 NULL);
	if (g_file_get_contents (path, &contents, NULL, NULL)) {
		time = g_ascii_strtod (contents, NULL);
		g_free (contents);
	} else {
		time = DEFAULT_POLL_TIME;
	}
	g_free (path);

	return (time * 1000);
}

static gboolean
iio_poll_light_open (GUdevDevice        *device,
		     ReadingsUpdateFunc  callback_func,
		     gpointer            user_data)
{
	guint interval;

	drv_data = g_new0 (DrvData, 1);
	drv_data->callback_func = callback_func;
	drv_data->user_data = user_data;

	interval = get_interval (device);
	drv_data->input_path = g_build_filename (g_udev_device_get_sysfs_path (device),
						 "in_illuminance_input",
						 NULL);
	drv_data->timeout_id = g_timeout_add (interval,
					      (GSourceFunc) light_changed,
					      NULL);

	return TRUE;
}

static void
iio_poll_light_close (void)
{
	if (drv_data->timeout_id != 0) {
		g_source_remove (drv_data->timeout_id);
		drv_data->timeout_id = 0;
	}
	g_clear_pointer (&drv_data->input_path, g_free);
	g_clear_pointer (&drv_data, g_free);
}

SensorDriver iio_poll_light = {
	.name = "IIO Buffer Light sensor",
	.type = DRIVER_TYPE_LIGHT,
	.specific_type = DRIVER_TYPE_LIGHT_IIO,

	.discover = iio_poll_light_discover,
	.open = iio_poll_light_open,
	.close = iio_poll_light_close,
};
