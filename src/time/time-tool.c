/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* main.c: this file is part of time-admin, a ximian-setup-tool frontend 
 * for system time configuration.
 * 
 * Copyright (C) 2005 Carlos Garnacho.
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
 * Authors: Carlos Garnacho Parro <carlosg@gnome.org>
 */

#include <glib.h>
#include <glib/gi18n.h>
#include "time-tool.h"
#include "gst.h"

#define GST_TIME_TOOL_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GST_TYPE_TIME_TOOL, GstTimeToolPrivate))

typedef struct _GstTimeToolPrivate GstTimeToolPrivate;

struct _GstTimeToolPrivate {
	OobsObject *time_config;
	OobsObject *ntp_config;
	OobsObject *services_config;

	guint clock_timeout;
	guint apply_timeout;
};


static void  gst_time_tool_class_init     (GstTimeToolClass *class);
static void  gst_time_tool_init           (GstTimeTool      *tool);
static void  gst_time_tool_finalize       (GObject          *object);

static GObject *gst_time_tool_constructor (GType                  type,
					   guint                  n_construct_properties,
					   GObjectConstructParam *construct_params);
static void  gst_time_tool_update_gui     (GstTool *tool);

G_DEFINE_TYPE (GstTimeTool, gst_time_tool, GST_TYPE_TOOL);

static void
gst_time_tool_class_init (GstTimeToolClass *class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (class);
	GstToolClass *tool_class = GST_TOOL_CLASS (class);
	
	object_class->constructor = gst_time_tool_constructor;
	object_class->finalize = gst_time_tool_finalize;
	tool_class->update_gui = gst_time_tool_update_gui;

	g_type_class_add_private (object_class,
				  sizeof (GstTimeToolPrivate));
}

static void
get_ntp_service (GstTimeTool *tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);
	GObject *service;
	OobsList *list;
	OobsListIter iter;
	gboolean valid;
	const gchar *role;

	list = oobs_services_config_get_services (priv->services_config);
	valid = oobs_list_get_iter_first (list, &iter);

	while (valid) {
		service = oobs_list_get (list, &iter);
		role = oobs_service_get_role (OOBS_SERVICE (service));

		if (strcmp (role, "NTP_SERVER") == 0)
			tool->ntp_service = g_object_ref (service);

		g_object_unref (service);
		valid = oobs_list_iter_next (list, &iter);
	}
}

static void
gst_time_tool_init (GstTimeTool *tool)
{
	OobsList *list;
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);

	priv->time_config = oobs_time_config_get (GST_TOOL (tool)->session);
	priv->ntp_config = oobs_ntp_config_get (GST_TOOL (tool)->session);
	priv->services_config = oobs_services_config_get (GST_TOOL (tool)->session);

	get_ntp_service (tool);
}

static gboolean
on_apply_timeout (GstTimeTool *tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);
	guint year, month, day, hour, minute, second;

	gtk_calendar_get_date (GTK_CALENDAR (tool->calendar), &year, &month, &day);
	hour   = (guint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (tool->hours));
	minute = (guint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (tool->minutes));
	second = (guint) gtk_spin_button_get_value (GTK_SPIN_BUTTON (tool->seconds));

	oobs_time_config_set_time (OOBS_TIME_CONFIG (priv->time_config),
				   (gint) year, (gint) month, (gint) day,
				   (gint) hour, (gint) minute, (gint)second);

	oobs_object_commit (priv->time_config);
	gst_time_tool_start_clock (tool);

	return FALSE;
}

static void
update_apply_timeout (GstTimeTool *tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);

	gst_time_tool_stop_clock (tool);

	if (priv->apply_timeout) {
		g_source_remove (priv->apply_timeout);
		priv->apply_timeout = 0;
	}

	priv->apply_timeout = g_timeout_add (2000, (GSourceFunc) on_apply_timeout, tool);
}

static void
on_value_changed (GtkWidget *widget, gpointer data)
{
	gint value;
	gchar *str;

	value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));
	str = g_strdup_printf ("%02d", (gint) value);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), value);
	gtk_entry_set_text (GTK_ENTRY (widget), str);
	g_free (str);
}

static void
on_editable_changed (GtkWidget *widget, gpointer data)
{
	update_apply_timeout (GST_TIME_TOOL (data));
}

#define is_leap_year(yyy) ((((yyy % 4) == 0) && ((yyy % 100) != 0)) || ((yyy % 400) == 0));

static void
change_calendar (GtkWidget *calendar, gint increment)
{
	gint day, month, year;
	gint days_in_month;
	gboolean leap_year;

	static const gint month_length[2][13] = {
		{ 0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
		{ 0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
	};

	gtk_calendar_get_date (GTK_CALENDAR (calendar),
			       (guint*) &year, (guint*) &month, (guint*) &day);

	leap_year = is_leap_year (year);
	days_in_month = month_length [leap_year][month+1];

	if (increment != 0) {
		day += increment;

		if (day < 1) {
			day = month_length [leap_year][month] + day;
			month--;
		} else if (day > days_in_month) {
			day -= days_in_month;
			month++;
		}

		if (month < 0) {
			year--;
			leap_year = is_leap_year (year);
			month = 11;
			day = month_length [leap_year][month+1];
		} else if (month > 11) {
			year++;
			leap_year = is_leap_year (year);
			month = 0;
			day = 1;
		}

		gtk_calendar_select_month (GTK_CALENDAR (calendar),
					   month, year);
		gtk_calendar_select_day (GTK_CALENDAR (calendar),
					 day);
	}
}

static void
on_spin_button_wrapped (GtkWidget *widget, gpointer data)
{
	GstTimeTool *tool = GST_TIME_TOOL (data);
	gint value = gtk_spin_button_get_value (GTK_SPIN_BUTTON (widget));

	if (widget == tool->seconds)
		gtk_spin_button_spin (GTK_SPIN_BUTTON (tool->minutes),
				      (value == 0) ? GTK_SPIN_STEP_FORWARD : GTK_SPIN_STEP_BACKWARD, 1);
	else if (widget == tool->minutes)
		gtk_spin_button_spin (GTK_SPIN_BUTTON (tool->hours),
				      (value == 0) ? GTK_SPIN_STEP_FORWARD : GTK_SPIN_STEP_BACKWARD, 1);
	else if (widget == tool->hours)
		change_calendar (tool->calendar, (value == 0) ? 1 : -1);
}

static void
on_calendar_day_selected (GtkWidget *widget, gpointer data)
{
	update_apply_timeout (GST_TIME_TOOL (data));
}

static GtkWidget*
prepare_spin_button (GstTool *tool, const gchar *widget_name)
{
	GtkWidget *widget;

	widget = gst_dialog_get_widget (tool->main_dialog, widget_name);

	g_signal_connect (G_OBJECT (widget), "changed",
			  G_CALLBACK (on_editable_changed), tool);
	g_signal_connect (G_OBJECT (widget), "wrapped",
			  G_CALLBACK (on_spin_button_wrapped), tool);
	/*
	g_signal_connect (G_OBJECT (widget), "value-changed",
			  G_CALLBACK (on_value_changed), tool);
	*/

	return widget;
}

void
init_timezone (GstTimeTool *time_tool)
{
	GstTool *tool = GST_TOOL (time_tool);
	GtkWidget *w;
	GPtrArray *locs;
	GList *combo_locs = NULL;
	int i;

	time_tool->tzmap = e_tz_map_new (tool);
	g_return_if_fail (time_tool->tzmap != NULL);
	
	w = gst_dialog_get_widget (tool->main_dialog, "map_window");
	gtk_container_add (GTK_CONTAINER (w), GTK_WIDGET (time_tool->tzmap->map));
	gtk_widget_show (GTK_WIDGET (time_tool->tzmap->map));

	w = gst_dialog_get_widget (tool->main_dialog, "location_combo");
	locs = tz_get_locations (e_tz_map_get_tz_db (time_tool->tzmap));

	for (i = 0; g_ptr_array_index (locs, i); i++)
		gtk_combo_box_append_text (GTK_COMBO_BOX (w),
					   tz_location_get_zone (g_ptr_array_index (locs, i)));

	time_tool->timezone_dialog = gst_dialog_get_widget (tool->main_dialog, "time_zone_window");
}

static GObject*
gst_time_tool_constructor (GType                  type,
			   guint                  n_construct_properties,
			   GObjectConstructParam *construct_params)
{
	GObject *object;
	GstTimeTool *time_tool;

	object = (* G_OBJECT_CLASS (gst_time_tool_parent_class)->constructor) (type,
									       n_construct_properties,
									       construct_params);
	time_tool = GST_TIME_TOOL (object);
	time_tool->map_hover_label = gst_dialog_get_widget (GST_TOOL (time_tool)->main_dialog, "location_label");
	time_tool->hours    = prepare_spin_button (GST_TOOL (time_tool), "hours");
	time_tool->minutes  = prepare_spin_button (GST_TOOL (time_tool), "minutes");
	time_tool->seconds  = prepare_spin_button (GST_TOOL (time_tool), "seconds");

	time_tool->calendar = gst_dialog_get_widget (GST_TOOL (time_tool)->main_dialog, "calendar");
	g_signal_connect (G_OBJECT (time_tool->calendar), "day-selected",
			  G_CALLBACK (on_calendar_day_selected), time_tool);

	init_timezone (time_tool);

	return object;
}

static void
gst_time_tool_finalize (GObject *object)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (object);

	g_object_unref (priv->time_config);

	(* G_OBJECT_CLASS (gst_time_tool_parent_class)->finalize) (object);
}

static void
gst_time_tool_update_gui (GstTool *tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);
	GtkWidget *timezone;

	gst_time_tool_start_clock (GST_TIME_TOOL (tool));
	timezone = gst_dialog_get_widget (tool->main_dialog, "tzlabel");

	gtk_label_set_text (GTK_LABEL (timezone),
			    oobs_time_config_get_timezone (OOBS_TIME_CONFIG (priv->time_config)));

	/* FIXME: missing NTP active button state */
	/* FIXME: missing NTP servers list */
}

GstTool*
gst_time_tool_new (void)
{
	return g_object_new (GST_TYPE_TIME_TOOL,
			     "name", "time",
			     "title", _("Time and Date Settings"),
			     NULL);
}

static void
freeze_clock (GstTimeTool *tool)
{
	g_signal_handlers_block_by_func (tool->hours,   on_editable_changed, tool);
	g_signal_handlers_block_by_func (tool->minutes, on_editable_changed, tool);
	g_signal_handlers_block_by_func (tool->seconds, on_editable_changed, tool);
	g_signal_handlers_block_by_func (tool->calendar, on_calendar_day_selected, tool);
}

static void
thaw_clock (GstTimeTool *tool)
{
	g_signal_handlers_unblock_by_func (tool->hours,   on_editable_changed, tool);
	g_signal_handlers_unblock_by_func (tool->minutes, on_editable_changed, tool);
	g_signal_handlers_unblock_by_func (tool->seconds, on_editable_changed, tool);
	g_signal_handlers_unblock_by_func (tool->calendar, on_calendar_day_selected, tool);
}

void
gst_time_tool_update_clock (GstTimeTool *tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);
	gint year, month, day, hour, minute, second;

	oobs_time_config_get_time (OOBS_TIME_CONFIG (priv->time_config),
				   &year, &month,  &day,
				   &hour, &minute, &second);

	freeze_clock (tool);

	gtk_calendar_select_month (GTK_CALENDAR (tool->calendar), month, year);
	gtk_calendar_select_day   (GTK_CALENDAR (tool->calendar), day);

	gtk_spin_button_set_value (GTK_SPIN_BUTTON (tool->hours),   (gfloat) hour);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (tool->minutes), (gfloat) minute);
	gtk_spin_button_set_value (GTK_SPIN_BUTTON (tool->seconds), (gfloat) second);

	thaw_clock (tool);
}

static gboolean
clock_tick (gpointer data)
{
	GstTimeTool *tool = (GstTimeTool *) data;

	gst_time_tool_update_clock (tool);

	return TRUE;
}

void
gst_time_tool_start_clock (GstTimeTool *tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);

	if (!priv->clock_timeout) {
		/* Do a preliminary update, just for showing
		   something with sense in the gui immediatly */
		gst_time_tool_update_clock (tool);

		priv->clock_timeout = g_timeout_add (1000, (GSourceFunc) clock_tick, tool);
	}
}

void
gst_time_tool_stop_clock (GstTimeTool *tool)
{
	GstTimeToolPrivate *priv = GST_TIME_TOOL_GET_PRIVATE (tool);

	if (priv->clock_timeout) {
		g_source_remove (priv->clock_timeout);
		priv->clock_timeout = 0;
	}
}

void
gst_time_tool_run_timezone_dialog (GstTimeTool *time_tool)
{
	GstTimeToolPrivate *priv;
	GstTool *tool;
	GtkWidget *label;
	gchar *timezone;
	gchar *tz_name = NULL;
	gchar *old_tz_name = NULL;
	TzLocation *tz_location;
	gint correction;

	priv  = GST_TIME_TOOL_GET_PRIVATE (time_tool);
	tool  = GST_TOOL (time_tool);
	label = gst_dialog_get_widget (tool->main_dialog, "tzlabel");

	timezone = oobs_time_config_get_timezone (OOBS_TIME_CONFIG (priv->time_config));
	e_tz_map_set_tz_from_name (time_tool->tzmap, timezone);

	gtk_window_set_transient_for (GTK_WINDOW (time_tool->timezone_dialog),
				      GTK_WINDOW (tool->main_dialog));

	while (gtk_dialog_run (GTK_DIALOG (time_tool->timezone_dialog)) == GTK_RESPONSE_HELP);

	tz_name     = e_tz_map_get_selected_tz_name (time_tool->tzmap);
	tz_location = e_tz_map_get_location_by_name (time_tool->tzmap, tz_name);

	if (!timezone || strcmp (tz_name, timezone) != 0) {
		oobs_time_config_set_timezone (OOBS_TIME_CONFIG (priv->time_config), tz_name);
		oobs_object_commit (priv->time_config);
		gtk_label_set_text (GTK_LABEL (label), tz_name);
	}

	gtk_widget_hide (time_tool->timezone_dialog);
}