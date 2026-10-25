# Installing RSFX on the MPC

Needs root SSH access to the MPC (the same access you used for the JV-880 and GlueBus).

1. Upload `RSFX-1.1.0-mpc-armv7.zip` to `/tmp` on the MPC (Termius SFTP panel).
2. In the terminal: `cd /tmp && unzip -o RSFX-1.1.0-mpc-armv7.zip && sh RSFX-1.1.0/install.sh`
3. Save your project first, then answer `y`. MPC stops, the plugin is registered, and MPC restarts.
4. Check it registered: `grep rsfx /media/az01-internal/Settings/*/MPC.settings` should print one line.
5. On the MPC, open a track or bus insert slot and pick **RSFX** in the plugin browser.

The installer copies `rsfx.so` to `/sdcard/vst/` and the screen skin to `/sdcard/Synths/RSFX - VST - RSFX/`, adds the
plugin to `pluginList-arm` in `MPC.settings`, and leaves a
backup next to the settings file (`MPC.settings.bak-rsfx-*`). If `unzip` isn't on the MPC, unzip on your phone and
upload the `RSFX-1.1.0` folder instead.

**Uninstall:** `sh /tmp/RSFX-1.1.0/uninstall.sh` (keep the folder; `/tmp` may be cleared on reboot).

**If MPC misbehaves after installing:** restore the backup with
`cp /media/az01-internal/Settings/*/MPC.settings.bak-rsfx-<date> <same folder>/MPC.settings`, then restart.

**Upgrading from 1.0:** just run the 1.1 installer; projects keep their settings.

**Skin doesn't show (plain parameter list instead):** check that `/sdcard/Synths` is listed in `SynthContentLocations` in
`MPC.settings` (the installer warns if it isn't). A skin-only change needs no restart: remove and re-insert the plugin.
