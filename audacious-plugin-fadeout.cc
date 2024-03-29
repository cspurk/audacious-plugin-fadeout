/*
 * Audacious FadeOut Plugin
 *
 * Provides a menu entry for smoothly fading out any playing song before
 * eventually stopping playback.
 *
 * Copyright (C) 2008–2022  Christian Spurk
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

// expected to be generated by the build system
#include "plugin_export.h"

#include <libaudcore/drct.h>
#include <libaudcore/i18n.h>
#include <libaudcore/interface.h>
#include <libaudcore/plugin.h>
#include <libaudcore/preferences.h>
#include <libaudcore/runtime.h>

#include <glib.h>
#include <math.h>

// PACKAGE should be defined by the build system
#ifndef PACKAGE
#define PACKAGE "audacious-plugin-fadeout"
#endif

// section name for the plugin in the Audacious config DB
#define AUD_CFG_SECTION "fadeout_plugin"
// config DB key for the duration
#define AUD_CFG_KEY_DURATION "duration"
// maximum possible duration for a fade-out (in seconds)
#define MAX_DURATION 10
// maximum volume reduction (200 roughly corresponds to silence)
#define MAX_VOL_REDUCTION 200

static const char fadeout_about[] =
    N_("FadeOut Plugin\n"
       "By Christian Spurk 2008–2022.\n\n"
       "Provides a menu entry for smoothly fading out any "
       "playing song before eventually stopping playback.");

// defaults for the configuration database
static const char * const fadeout_defaults[] = {
    AUD_CFG_KEY_DURATION, "4",
    nullptr
};

static const PreferencesWidget fadeout_widgets[] = {
    WidgetLabel (N_("<b>Fade out</b>")),
    WidgetSpin (N_("Duration:"),
        WidgetFloat (AUD_CFG_SECTION, AUD_CFG_KEY_DURATION),
        {1, MAX_DURATION, 0.1, N_("seconds")})
};

static const PluginPreferences fadeout_prefs = {{fadeout_widgets}};

class FadeoutPlugin : public EffectPlugin
{
public:
    static constexpr PluginInfo info = {
        N_("FadeOut"),
        PACKAGE,
        fadeout_about,
        & fadeout_prefs
    };

    constexpr FadeoutPlugin () : EffectPlugin (info, 9, true) {}

    bool init ();
    void cleanup ();

    void start (int & channels, int & rate);
    Index<float> & process (Index<float> & data);
    Index<float> & finish (Index<float> & data, bool end_of_playlist);
};

EXPORT FadeoutPlugin aud_plugin_instance;


// the volume reduction; 1 for inactive fading; >1 for active fading
static double vol_reduction = 1;
// workaround used to more or less detect if the plugin is enabled or not
static bool is_plugin_processing = false;

/* a GSourceFunc which stops the audio playback and our fading thread (if it
 * should still be running); to be used in g_idle_add() for thread-safety */
static gboolean stop_playback_and_fading_cb (gpointer data)
{
    aud_drct_stop ();
    vol_reduction = 1;

    return FALSE;
}

/* stops the audio playback and our fading thread (if it should still be
 * running) */
static void stop_playback_and_fading ()
{
    // make sure to run this in the main loop in order to be thread-safe
    g_idle_add (stop_playback_and_fading_cb, NULL);
}

/* Calculates the vol_reduction_amount from the configured number of seconds. */
static double calculate_vol_reduction_amount ()
{
    double duration = aud_get_double (AUD_CFG_SECTION, AUD_CFG_KEY_DURATION);
    // we have to multiply the seconds by 100 as we sleep 0,01 seconds in each
    // volume reduction step in the fading thread
    return pow (MAX_VOL_REDUCTION, (1 / (100 * duration)));
}

/* Smoothly increases the volume reduction and finally stops playback. */
static gpointer fading_thread (gpointer data)
{
    double vol_reduction_amount = calculate_vol_reduction_amount ();
    GTimer* timer = g_timer_new ();
    guint16 target = 10000;
    gint32 remaining = 0;

    // fade out smoothly
    for (vol_reduction *= vol_reduction_amount;
         vol_reduction < MAX_VOL_REDUCTION;
         vol_reduction *= vol_reduction_amount)
    {
        /* We are targeting a sleep time of 10000 microseconds per loop
         * iteration. For various reasons this time can be more or less,
         * though, so we always keep track of the remaining total sleep time
         * and adjust the next target accordingly. */
        g_timer_start (timer);
        g_usleep (target);
        /* the time difference between our target time and the actually uslept
         * time: if diff>0 then we have slept too long, if diff<0 then we have
         * slept too few */
        gint32 diff = (1000000.0 * g_timer_elapsed (timer, NULL)) - target;
        /* overall, how much sleep time is still remaining? */
        remaining = 10000 - diff + (remaining < 0 ? remaining : 0);
        /* thus, what is the sleep time target for the next iteration? */
        target = remaining < 0 ? 0 : remaining;

        /* if the volume reduction was set back outside this thread, then the
         * plugin has shut down or the song is over and we also have to exit
         * this thread */
        if (vol_reduction == 1)
        {
            g_timer_destroy (timer);
            g_thread_exit (NULL);
        }
    }
    g_timer_destroy (timer);

    // the volume is low enough now -- stop playback
    stop_playback_and_fading ();

    // the fading is inactive now
    vol_reduction = 1;

    return NULL;
}

/* Callback function for invoking the fade out menu item. */
static void fade_out_cb ()
{
    // only fade out if the plugin is processing and fading is not yet active
    if (is_plugin_processing && vol_reduction == 1)
    {
        GError * error = NULL;
        GThread * thread = g_thread_try_new (
            NULL, (GThreadFunc) fading_thread, NULL, &error);
        g_thread_unref (thread);

        if (error != NULL)
        {
            g_warning (_("Could not create the thread for fading out: %s"),
                error->message);
            g_error_free (error);
        }
    }

    return;
}

bool FadeoutPlugin::init ()
{
    aud_config_set_defaults (AUD_CFG_SECTION, fadeout_defaults);

    // create the menu item and connect it to a callback function
    aud_plugin_menu_add (AudMenuID::Main, fade_out_cb, _("Fade out"), NULL);

    return true;
}

void FadeoutPlugin::cleanup ()
{
    // switch off the fading thread
    vol_reduction = 1;

    aud_plugin_menu_remove (AudMenuID::Main, fade_out_cb);
}

void FadeoutPlugin::start (int & channels, int & rate)
{
    is_plugin_processing = true;
}

Index<float> & FadeoutPlugin::process (Index<float> & data)
{
    // adjust the volume only if fading is active
    if (vol_reduction != 1)
    {
        float * end = data.end ();
        for (float * f = data.begin (); f < end; f++)
        {
            * f = * f / vol_reduction;
        }
    }

    return data;
}

Index<float> & FadeoutPlugin::finish (Index<float> & data, bool end_of_playlist)
{
    // make sure to stop with the current song if fading is active
    if (vol_reduction != 1)
    {
        stop_playback_and_fading ();
    }

    is_plugin_processing = false;

    return process (data);
}

