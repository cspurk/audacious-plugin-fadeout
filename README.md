Audacious FadeOut Plugin
========================

A plugin for the [Audacious](http://www.audacious-media-player.org/) media
player which provides a menu entry for smoothly fading out the currently playing
song before eventually stopping playback. The fade out duration is configurable.

The plugin currently requires at least Audacious 3.2.x to work.


Installation
------------

Download and unpack the plugin distribution into a folder (e.g.,
`~/audacious-plugin-fadeout`). In a terminal, run the following commands to
build and install the plugin in your local Audacious configuration folder:

    cd ~/audacious-plugin-fadeout
    cmake . -DCMAKE_INSTALL_PREFIX=$HOME
    make && make install

Run Audacious and go to “Preferences” (`Ctrl+P`) → “Plugins” → “Effect” and
activate “FadeOut”. If desired, change the default fade out duration via the
“Preferences” button. Close the “Audacious Preferences” window and enjoy: you
can find the fade out function in Audacious’ main menu under “Plugin Services”.


Known Issues
------------

 * fading should be triggerable by some keyboard shortcut from within Audacious,
   e.g., with `Shift+V` as in Winamp
 * when requesting another song while fading out, the playback stops immediately


Development
-----------

The plugin was created by Christian Spurk. It is maintained as far as time
permits and as far as personally deemed necessary.

If you would like to see the plugin improved, then please file an issue at
GitHub – or ideally issue a pull request with a patch.

