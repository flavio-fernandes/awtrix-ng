# Home Assistant proverbs for AWTRIX NG

Replaces the scheduling side of `proverbs.sh`; the separately installed third-party
**AWTRIX NG Create Notification** blueprint owns sending/dismissing notifications.
No copy or reimplementation of that blueprint is included here.

## Install

1. Enable the MQTT integration and configure your AWTRIX NG display on the same
   broker. Install the notification blueprint yourself at
   `smarthomejunkie/awtrix_ng/awtrix_ng_create_notification.yaml`.
2. Copy the whole `packages/proverbs/` directory to
   `/config/packages/proverbs/`, so that `proverbs.yaml` and the `proverbs.txt`
   from step 3 sit together. Merge this into your existing
   `configuration.yaml` (do not duplicate `homeassistant:`):

   ```yaml
   homeassistant:
     packages:
       proverbs: !include packages/proverbs/proverbs.yaml
   automation: !include automations.yaml
   ```

   One explicit `!include` per package, rather than
   `packages: !include_dir_named packages`, for two reasons: it is what lets a
   package keep data files beside its yaml, and `!include_dir_named` finds a
   nested `packages/proverbs/proverbs.yaml` only because HA's loader happens
   to walk subdirectories, which is not documented. Add one line per package.

   If a top-level `template:` or `automation:` include already exists, the
   package merges into it: both domains are list-based, so entries are
   appended. `script:` is a mapping and would collide.

3. Download the source list from a shell that can write HA's config directory:

   ```sh
   curl --fail --location https://raw.githubusercontent.com/alltom/proverb/master/proverbs.txt \
     --output /config/packages/proverbs/proverbs.txt
   ```

   The command-line integration needs `shuf` and `tr`, both present in the
   official HA container (confirmed on 2026.9.4). The container cannot be probed
   from the SSH add-on beforehand, so if `sensor.proverb` comes up `unknown`,
   replace `shuf -n 1` with
   `awk 'BEGIN{srand()} {a[NR]=$0} END{if(NR>0) print a[int(rand()*NR)+1]}'`
   and call `homeassistant.reload_all`; no restart is needed, because
   `command_line` is reloadable. The sensor picks one random line, strips CR,
   and truncates to 250 Unicode characters, which also keeps the state under
   HA's 255-character limit. Its yearly scan interval disables practical periodic
   polling; HA still performs its initial read. Subsequent reads are requested by
   the pacing automation. Download again manually to refresh the source list.
4. In the package's ONE marked `motion_entity` variable, confirm or replace the
   supplied office motion entity. Missing/unknown/unavailable entities fail closed.
   Motion is allowed when on, or when its last state change was less than two
   hours ago. HA startup can reset `last_changed`; this intentionally follows the
   requested last-changed rule, not persistent motion history.
5. Choose ONE of two notification automations, never both (both would send
   every proverb twice):
   - the blueprint instance below (small font, text starts on the panel), or
   - the direct automation in `automation_direct.yaml` (large font, every
     proverb scrolls in from the right edge); see
     [Large font and scroll-in from the right](#large-font-and-scroll-in-from-the-right-direct-automation).

   For the blueprint: append the list item in `automation_example.yaml` to `automations.yaml`, or
   instantiate the installed blueprint and set equivalent inputs. Replace
   `REPLACE_WITH_AWTRIX_DEVICE_ID` with the HA device ID (not an entity ID), OR
   use `awtrix_displays: []` and `extra_prefixes: [your_mqtt_prefix]`.
   The colours are templates returning RGB lists, not literal template strings
   to send over MQTT. Confirm your installed blueprint supports templated colour
   inputs; its selectors/implementation are not distributed or inspected here.
6. Run HA's configuration check, then restart HA to load helpers and sensors.
   Confirm the generated entity IDs match those in the example (rename existing
   conflicting entities first). Watch the pacing automation's trace and the
   blueprint automation's trace. The interval helper starts at 96 seconds; each
   restart adds a uniformly random 0–15 seconds. The timer is restarted even if
   there is no motion. Restarting HA starts a fresh timer and checks motion once.

## Large font and scroll-in from the right (direct automation)

The notification blueprint has no font, entry or repeat inputs, and cannot be
given any from here. `automation_direct.yaml` is a small alternative that
publishes the same proverb notification straight to `<prefix>/cmd/notify`, with
these AWTRIX NG payload keys added:

| Key | Effect |
|---|---|
| `"font": "large"` | NG's 8-row font; the default `small` is 5 rows plus a descender. |
| `"scroll": {"entry": "offscreen"}` | Every proverb starts on a blank panel; the first column of its first letter appears at the right edge. |
| `"scroll": {"whenFits": "scroll"}` | Short proverbs enter the same way instead of standing still. |
| `"repeat": 1` | One full pass, ending when the last pixel leaves column 0. Without it NG ends a notification after `appDurationMs` (7 s by default), mid-text. |
| `"stack": true` | A new proverb queues behind one still scrolling instead of replacing it. |

Name, colours, `textCase` and `speed` 100 (about 21 px/s) match the blueprint
instance. The automation only reacts to the helper turning on; there is no
dismiss on off, because the pacing automation turns the helper off and on every
cycle and a dismiss would cut a proverb that is still scrolling.

To install it instead of the blueprint instance:

1. Append the list item in `automation_direct.yaml` to `automations.yaml`.
2. Set `prefix` to your display's MQTT prefix (its `mqtt_prefix` sensor in HA).
   The MQTT integration must be set up on the display's broker.
3. If the blueprint instance ("AWTRIX proverb notification") already exists,
   turn it off (or delete it); never run both.
4. Run the configuration check and reload automations.

One pass takes about (text width + panel width) / 21 seconds: roughly 9 s for an
average proverb and up to about 27 s for the longest. If the pacing interval is
shorter than a proverb's pass, proverbs queue and play back to back; with the
default interval (96 s or more) and any interval of 15 s or more, the average
pass fits and the queue does not grow.

## Behaviour and parity

- A trigger-based pair of template sensors redraws colours for every selected
  proverb, even if the same line is selected twice. Text is randomly selected
  from red, green, yellow, magenta, cyan and white: never black or blue.
  Background is black with probability 6/7, pure blue with probability 1/7.
- Colour sensor states contain JSON-compatible RGB lists. The example parses
  them back into lists. Pacing waits for both colour sensors before toggling.
- The toggle goes off (dismiss), waits one second, then on (notify). This keeps
  independent blueprint runs ordered in normal operation. The blueprint owns
  MQTT delivery; a config check cannot guarantee delivery timing or connectivity.
- Blueprint notifications use name `proverb`, no hold, one repeat, no stacking, scrolling
  at speed 100, no icon, no gradient/rainbow, no effect, no sound, duration 0.
  AWTRIX has no text outline; the old script's outline cannot be reproduced.
- This package does not stop an already scrolling notification when motion
  expires; it simply stops selecting new proverbs until motion qualifies again.
- The package itself does not publish MQTT (only the optional direct automation does). Do not run the old service and this
  automation together or they will compete for the display.

## Disable and purge

Nothing here creates a config entry, a device, a custom component, a pip
dependency or an add-on, so there is no integration page to clean up. The
package publishes no MQTT at all, and the optional direct automation publishes
to `<prefix>/cmd/notify` without `retain`, so nothing is left on the broker and
nothing is written to the display's flash. Notifications are transient and no
custom app is created.

**Turn it off** — instant, no restart, no file edits. Disable the `AWTRIX
proverb pacing` automation and whichever notification automation you chose, then
call `timer.cancel` on `timer.proverb_pacing`. Press the display's
`dismiss_notification` button if a proverb is still scrolling. Automation enable
state lives in the entity registry, so this survives a restart; re-enable with
the same toggles.

**Unload the package, keep the files.** Comment out the
`proverbs: !include packages/proverbs/proverbs.yaml` line, run `ha core check`,
and restart. The package's entities become unavailable. A notification
automation kept in `automations.yaml` survives but can no longer fire, because
the helper it triggers on is gone.

**Purge.**

1. Delete the notification automation. If you appended it to
   `automations.yaml`, the UI's delete does it; if you moved it into the
   package, it goes with the package.
2. Press the display's `dismiss_notification` button to clear the panel.
3. Remove the `homeassistant: packages:` entry from `configuration.yaml`.
4. `rm -rf /config/packages/proverbs`.
5. `ha core check`, then restart HA.
6. Settings -> Devices & Services -> Entities, search `proverb`, select all and
   remove. YAML-defined entities leave registry rows behind as unavailable
   entries once their config is gone.
7. Call `recorder.purge_entities` with `entity_globs: ["*proverb*"]` and
   `keep_days: 0`. `sensor.proverb` records a new ~250-character state every
   interval while motion qualifies, so this is worth doing. Then clear any
   remaining `input_number.proverb_interval_s` series under Developer Tools ->
   Statistics.

Adding or removing the package needs a full restart rather than a reload:
`command_line`, `input_boolean`, `input_number` and `timer` may not be loaded
at all beforehand, and `homeassistant.reload_all` cannot bootstrap an
integration that was never set up.

## Retire the shell loop

After testing HA, on the machine that actually owns the old system service:

```sh
sudo systemctl disable --now proverbs.service
systemctl is-active proverbs.service
systemctl is-enabled proverbs.service
```

Expect `inactive` and `disabled` (these queries may return nonzero). For a user
service use `systemctl --user` instead, without sudo. Keep the old script/unit as
rollback initially; stop the HA pacing automation and cancel `timer.proverb_pacing`
before re-enabling the service. Do not disable an unrelated unit on the HA host.

## Verification limits

A throwaway Home Assistant configuration can validate the package and blueprint
input wiring with a hand-written no-op stub declaring only the documented input
names. Such a stub is NOT the copyrighted blueprint and does not test its
selectors, template evaluation, or MQTT actions. Final verification with the
user-installed blueprint and real device remains a human installation step.
The simulator smoke test exercises the documented notification JSON contract
and the empty-payload dismissal, independently of that blueprint. It also renders the
direct automation's payload, publishes it, and checks on the screen that the
text enters from the right edge, moves left, and ends only after it has fully
left the panel (repeat 1).
