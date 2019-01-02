Audacious FadeOut Plugin
========================

A plugin for the [Audacious](http://www.audacious-media-player.org/) media
player which provides a menu entry for smoothly fading out the currently playing
song before eventually stopping playback. The fade out duration is configurable.

The plugin currently requires at least Audacious 3.2.x to work.


Installation
------------

Choose an appropriate release for your Audacious version, e.g., 3.4.x-1.0 if you
use Audacious 3.4.x.
Download and unpack the plugin distribution into a folder (e.g.,
`~/audacious-plugin-fadeout`).

Make sure to have the Audacious development libraries and their dependencies
installed. On Ubuntu, for example, you can achieve this by running:
`sudo apt install audacious-dev`

In a terminal, you can run the following commands to build and install the
plugin along with a typical Audacious configuration:

    cd ~/audacious-plugin-fadeout
    cmake . -DCMAKE_INSTALL_PREFIX=/usr
    make
    sudo make install

Instead of the `/usr` prefix, you can also use `$HOME`, for example;
however, this will only work with older Audacious versions (probably Audacious
3.5 or earlier), see “Known Issues”.

Run Audacious and go to “Preferences” (`Ctrl+P`) → “Plugins” → “Effect” and
activate “FadeOut”. If desired, change the default fade out duration via the
“Preferences” button. Close the “Audacious Preferences” window and enjoy: you
can find the fade out function in Audacious’ main menu under “Plugin Services”.


Known Issues
------------

 * fading should be triggerable by some keyboard shortcut from within Audacious,
   e.g., with `Shift+V` as in Winamp
 * recent versions of the plugin need to be installed in the same place as the
   official Audacious plugins (which usually requires `root` access);
   that seems to be a limitation of the current plugin system in Audacious
 * the “Fade out” menu entry is shown even if the plugin is not enabled


Development
-----------

The plugin was created by Christian Spurk. It is maintained as far as time
permits and as far as personally deemed necessary.

If you would like to see the plugin improved, then please file an issue at
GitHub – or ideally issue a pull request with a patch.

