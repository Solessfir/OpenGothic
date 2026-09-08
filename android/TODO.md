# Android TODO

## Custom D-pad quick slots

- Allow assigning weapons, spells, maps, food and potions to individual D-pad directions.
- When an assigned potion runs out, use another available potion of the same type (health or mana), not permanent-stat potions.
- Keep two-finger left/right swipes as health/mana shortcuts, independent of D-pad assignments, using the same potion-selection logic.
- Decide how assignment works in inventory and how hold-to-open radial menus interact with assigned slots before implementing.

## Input brainstorming (not implemented)

- Consider an optional auto-draw-on-attack setting, enabled by default: with a valid hostile target focused, Attack would draw the last usable weapon before attacking.
- Decide how shared touch Use/Attack distinguishes attacking from talking, looting or interacting, especially with neutral NPCs and unconscious targets.
- Decide whether the initial press only draws or also queues a strike, and how fists, bows without ammunition and spells behave.
- Keep auto-draw disabled in menus, dialogue and other interactions; avoid surprising aggression from an ordinary Use tap.
- With custom D-pad slots, decide whether selecting an assigned weapon draws it immediately and whether an unassigned player still has a dedicated draw action.
- Consider LT for draw/sheathe, but first resolve conflicts with its existing exploration modifier and inventory spell-slot chords.
- Revisit two-finger potion swipes and D-pad actions together after the three game editions are set up; the current proposal keeps health/mana swipes independent of assignments.

## Small upstream OpenGothic build PR

- After the Tempest Android changes are accepted upstream, extract a small PR for Try/OpenGothic containing only the Android build setup and integration with Tempest's CMake packaging helper.
- This integration already works in this fork; the task is to prepare it for upstream review, not merge the entire Android branch.
- Keep touch/gamepad controls, the game-data importer, UI and gameplay changes separate. The build-only PR would not provide this fork's full Android experience.
