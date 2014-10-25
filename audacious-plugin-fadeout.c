/*
 * Audacious FadeOut Plugin
 *
 * Provides a menu entry for smoothly fading out any playing song before
 * eventually stopping playback.
 *
 * Copyright (C) 2008–2013  Christian Spurk
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <audacious/drct.h>
#include <audacious/misc.h>
#include <audacious/plugin.h>

/* both GETTEXT_PACKAGE and LOCALEDIR should be defined by the build system! */
#ifndef GETTEXT_PACKAGE
#define GETTEXT_PACKAGE "audacious-plugin-fadeout"
#endif

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <math.h>

/* section name for the plugin in the Audacious config DB */
#define AUD_CFG_SECTION "fadeout_plugin"

static gboolean init(void);
static void cleanup(void);
static void about(void);
static void configure(void);
static void fadeout_start(gint *channels, gint *rate);
static void fadeout_process(gfloat **d, gint *samples);
static void fadeout_flush(void);
static void fadeout_finish(gfloat **d, gint *samples);

/* whether the plugin is active or not */
static gboolean plugin_active = FALSE;
/* the volume reduction; 1 for inactive fading; >1 for active fading */
static gdouble vol_reduction = 1;
/* the amount of volume reduction during fading (this is just kind of a buffer;
 * the actual fadeout duration is in the duration variable) */
static gdouble vol_reduction_amount = 1;
/* the number of seconds that the fading takes */
static gdouble duration = 4;
/* whether the plugin has already performed one-time initializations or not */
static gboolean is_plugin_started_up = FALSE;
/* defaults for the configuration database */
static const gchar * const config_defaults[] =
{ "duration", "4", NULL };

AUD_EFFECT_PLUGIN
(
	.name = "FadeOut",
	.init = init,
	.cleanup = cleanup,
	.about = about,
	.configure = configure,
	.start = fadeout_start,
	.process = fadeout_process,
	.flush = fadeout_flush,
	.finish = fadeout_finish
)

/* a GSourceFunc which stops the audio playback and our fading thread (if it
 * should still be running); to be used in g_idle_add() for thread-safety */
static gboolean stop_playback_and_fading(gpointer data)
{
	aud_drct_stop();
	vol_reduction = 1;

	return FALSE;
}

/* Smoothly increases the volume reduction and finally stops playback. */
static gpointer fading_thread(gpointer data)
{
	GTimer* timer = g_timer_new();
	guint16 target = 10000;
	gint32 diff = 0;
	gint32 remaining = 0;

	/* fade out smoothly: a volume reduction of 200 roughly corresponds to
	 * silence */
	for (vol_reduction *= vol_reduction_amount; vol_reduction < 200; vol_reduction
			*= vol_reduction_amount)
	{
		/* We are targeting a sleep time of 10000 microseconds per loop
		 * iteration. For various reasons this time can be more or less,
		 * though, so we always keep track of the remaining total sleep time
		 * and adjust the next target accordingly. */
		g_timer_start(timer);
		usleep(target);
		/* the time difference between our target time and the actually uslept
		 * time: if diff>0 then we have slept too long, if diff<0 then we have
		 * slept too few */
		diff = (1000000.0 * g_timer_elapsed(timer, NULL)) - target;
		/* overall, how much sleep time is still remaining? */
		remaining = 10000 - diff + (remaining < 0 ? remaining : 0);
		/* thus, what is the sleep time target for the next iteration? */
		target = remaining < 0 ? 0 : remaining;

		/* if the volume reduction was set back outside this thread, then the
		 * plugin has shut down or the song is over and we also have to exit
		 * this thread */
		if (vol_reduction == 1)
		{
			g_timer_destroy(timer);
			g_thread_exit(NULL);
		}
	}
	g_timer_destroy(timer);

	/* the volume is low enough now -- stop playback; make sure to run this in
	 * the main loop in order to be thread-safe */
	g_idle_add(stop_playback_and_fading, NULL);

	/* the fading is inactive now */
	vol_reduction = 1;

	return NULL;
}

/* Callback function for invoking the fade out menu item. */
static void fade_out_cb(void)
{
	GError *error = NULL;
	GThread *thread = NULL;

	/* only fade out if playing and if fading is not active */
	if (aud_drct_get_playing() && vol_reduction == 1)
	{
		thread = g_thread_try_new(
				NULL, (GThreadFunc) fading_thread, NULL, &error);
		g_thread_unref(thread);

		if (error != NULL)
		{
			g_warning(_("Could not create the thread for fading out: %s"),
					error->message);
			g_error_free(error);
		}
	}

	return;
}

/* plugin hook: not needed here. */
static void fadeout_start(gint *channels, gint *rate)
{ }

/* plugin hook: adjusts the volume of each sample when the fading is active. */
static void fadeout_process(gfloat **d, gint *samples)
{
	gfloat *data = *d;
	gfloat *end = *d + *samples;

	/* adjust volume only if fading is active */
	if (vol_reduction != 1)
	{
		for (; data < end; data++) {
			*data = *data / vol_reduction;
		}
	}
}

/* plugin hook: not needed here. */
static void fadeout_flush(void)
{ }

/* plugin hook: when reaching the end of a song while fading out, stop playback
 * immediately. */
static void fadeout_finish(gfloat **d, gint *samples)
{
	fadeout_process(d, samples);

	/* if we are still fading out, then stop playback and also stop the fading
	 * thread */
	if (vol_reduction != 1)
	{
		/* make sure to run this in the main loop in order to be thread-safe */
		g_idle_add(stop_playback_and_fading, NULL);
	}
}

/* Calculates the vol_reduction_amount from the given number of seconds. */
static gdouble calculate_vol_reduction_amount(gdouble seconds)
{
	/* 200 is the maximum volume reduction in the fading thread; we have to
	 * multiply the seconds by 100 as we sleep 0,01 seconds in each volume
	 * reduction step in the fading thread */
	return pow(200, (1 / (100 * seconds)));
}

/* Performs one-time initializations for the plugin (whether it is active or
 * not). */
static void start_up(void)
{
	if (is_plugin_started_up)
		return;
	is_plugin_started_up = TRUE;

	bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
}

/* plugin hook: initializes and starts the plugin; afterwards the plugin is
 * active. */
static gboolean init(void)
{
	start_up();

	/* create the menu item and connect it to a callback function */
	aud_plugin_menu_add(AUD_MENU_MAIN, fade_out_cb,
			Q_("fade out menu action|Fade out"), NULL);

	/* load user preferences for the fadeout duration */
	aud_config_set_defaults(AUD_CFG_SECTION, config_defaults);
	duration = aud_get_double(AUD_CFG_SECTION, "duration");
	vol_reduction_amount = calculate_vol_reduction_amount(duration);

	plugin_active = TRUE;
	return TRUE;
}

/* plugin hook: shuts down the plugin and frees resources. */
static void cleanup(void)
{
	if (plugin_active)
	{
		/* switch off the fading thread */
		vol_reduction = 1;

		aud_plugin_menu_remove(AUD_MENU_MAIN, fade_out_cb);

		plugin_active = FALSE;
	}
}

/* plugin hook: shows an about dialog. */
static void about(void)
{
	static GtkWidget *about_dialog = NULL;

	start_up();

	audgui_simple_message (&about_dialog, GTK_MESSAGE_INFO,
			_("About FadeOut Plugin"), _("FadeOut Plugin\n"
					"By Christian Spurk 2008–2014.\n\n"
					"Provides a menu entry for smoothly fading out any "
					"playing song before eventually stopping playback."));
}

/* Callback for configuration dialog responses. */
static void dialog_response_cb(GtkWidget *dialog, gint responseID,
		GtkWidget *adjustment)
{
	switch (responseID)
	{
	case GTK_RESPONSE_OK:
		duration = gtk_adjustment_get_value(GTK_ADJUSTMENT(adjustment));
		vol_reduction_amount = calculate_vol_reduction_amount(duration);
		aud_set_double(AUD_CFG_SECTION, "duration", duration);
		break;
	case GTK_RESPONSE_CANCEL:
	default:
		break;
	}

	gtk_widget_destroy(dialog);
}

/* Sets up the given configuration dialog by adding necessary widgets. */
static void set_up_duration_config_dialog(GtkWidget *dialog)
{
	GtkWidget *label, *scale, *hbox, *spinButton, *spacerDummy, *adjustment;
	GtkBox *content_box =
			GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog)));

	gtk_box_set_spacing(content_box, 10);

	/* a spacer at the top */
	spacerDummy = gtk_alignment_new(0, 0, 0, 0);
	gtk_widget_set_size_request(spacerDummy, -1, 0);
	gtk_box_pack_start(content_box, spacerDummy, TRUE,
			TRUE, 0);

	/* then a left aligned label */
	label = gtk_label_new(_("FadeOut duration (seconds):"));
	hbox = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(content_box, hbox, TRUE, TRUE, 0);

	/* finally a hbox containing a spin button and a hscale at the bottom */
	hbox = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_start(content_box, hbox, TRUE, TRUE, 0);

	adjustment = GTK_WIDGET(
			gtk_adjustment_new(duration, 1.0, 10.0, 0.1, 1.0, 1.0));

	spinButton = gtk_spin_button_new(GTK_ADJUSTMENT(adjustment), 0.1, 1);
	gtk_box_pack_start(GTK_BOX(hbox), spinButton, FALSE, FALSE, 0);

	scale = gtk_hscale_new(GTK_ADJUSTMENT(adjustment));
	gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
	gtk_box_pack_start(GTK_BOX(hbox), scale, TRUE, TRUE, 0);

	g_signal_connect(dialog, "response", G_CALLBACK(dialog_response_cb),
			adjustment);
}

/* plugin hook: allows configuring the plugin. */
static void configure(void)
{
	GtkWidget *dialog;

	start_up();

	dialog = gtk_dialog_new_with_buttons(_("FadeOut Plugin Configuration"),
			NULL, 0, _("_Cancel"), GTK_RESPONSE_CANCEL, _("_OK"),
			GTK_RESPONSE_OK, NULL);
	set_up_duration_config_dialog(dialog);
	gtk_widget_show_all(dialog);
}
