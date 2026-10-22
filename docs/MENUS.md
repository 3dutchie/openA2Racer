# Menus, player profiles and scores

OpenA2 0.4 loads the original Dutch menu backgrounds, animated buttons, dialog
panels, route cards, option controls, score tabs and loading image from the
player's CD. Dynamic text uses embedded Liberation Serif for consistent rendering
on Windows, macOS and Linux. It approximates the reference screenshots; original
Windows GDI font rendering is not reproduced.

## Players and campaign

A new installation opens the ten-row NIEUWE SPELER list. Select an empty row and
press Enter or Selecteren to enter a name. Select an existing row to continue its
campaign. Names support up to 31 UTF-8 characters. Mouse double-click selects a
row; keyboard Up/Down moves rows and Left/Right or Tab switches the action.
Verwijderen asks for confirmation. A new player starts at etappe 1 with 1000 geld.

The main menu provides Start, Bediening, Opties, Score, Opnieuw and Nieuwe speler.
Its card shows the active player's name, money, cumulative travel time and next
route. Start resumes the current in-memory race, or starts the player's next
stage. Opnieuw resets campaign stage, money and travel time after confirmation;
earned best scores remain. The six route cards are Amsterdam–Vianen,
Vianen–Eindhoven, Eindhoven–Maastricht, and the reversed return journey.

A successfully completed stage advances campaign progress once and adds its
elapsed time. Failed attempts update money without creating a completed-stage
score. Replaying an unlocked stage can improve its best time without adding a
second campaign completion. Duplicate or stale result tokens are ignored.

Score has an overall table and six stage tabs. Tables contain actual earned
records, up to ten each, rather than fabricated copies of the original demo
names. Overall ranking uses progress, time and name; stage ranking uses best
time. Recorded scores survive profile deletion/restart and retain the name used
when earned. Switch tabs by mouse or Left/Right.

## Controls and options

Bediening configures Left, Right, Brake and Accelerator. Click a field to replace
one key, or Instellen to capture all four sequentially. Escape cancels capture;
OK commits, Annuleren/Escape discards the draft. Game-function keys are reserved.
WASD aliases work with the default arrow bindings. SDL game controllers keep the
port's existing standard stick/trigger/button mapping; original joystick and
steering-wheel calibration dialogs are not implemented.

Opties offers separate five-step music/effect volumes, sound modes (effects,
music, both, silent), sky/horizon visibility and dashboard visibility. The
Dashboard option hides the cockpit overlay while retaining its camera. Changes
preview immediately and are committed with OK or discarded with Annuleren.

Recovered display tabs are mapped to portable settings: 3D Accelerator selects
OpenA2 enhanced graphics, 640×480 selects classic rendering, and 320×200 selects
a low-resolution presentation of the classic frame. The last mode downsamples
the presentation; it does not replace the internal renderer with a 320×200
pipeline. F4/G continues to switch enhanced/classic during a race.

Even wachten artwork is presented during initial resource loading and while
preparing the first race frame. Fast loads remain fast: there is no added delay.

## Storage and compatibility

`profiles.db` and `settings.cfg` live in SDL's per-user OpenA2/OpenA2 directory.
Use `--prefs DIRECTORY` to choose an isolated directory. Profiles use new portable
storage, separate from the original SLOT.A2R and SCORE.A2R files; original
Windows saves are not imported. Profile database replacements are atomic and
loads validate the entire document before replacing live state.

Each profile has ten simulation save slots named using its immutable ID. Main
menu S/L opens slots; F6/F9 saves/loads slot 1. The old port-wide slot files remain
available through Oud globaal slot laden. Race save version 2 adds airborne
roadwork state; old version 1 saves load with pristine roadwork zones and retain
their saved prior simulation/RNG fields. Headless demos never award profile
scores. Deleting a profile leaves its retired simulation files untouched and
never reuses its ID.

Menu C opens the port's car selector, T opens unlocked stages, and F1 opens help.
These retain access to features alongside the six original main-menu actions.
