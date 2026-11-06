# Playtest checklist (after the Nov 2026 polish pass)

Everything below passed the headless harness. None of it has been judged by a
human. Play one match per kit (Trigger Happy, Hex, Tree Rex, Spyro); the
first match covers most items, the others are mainly kit feel.

Write findings as one line each: `[kit] what you saw -> what you expected`.

## Never seen on screen (look first)

- [ ] **Scoreboard** (hold Tab): two team sections, player row gold, K/D/A,
      CS, gold, items. Dead heroes dimmed. Refreshes while held.
- [ ] **Character select** (CHARACTERS on the main menu): four tiles, Spyro
      selectable, accent color on the selected tile and the preview backdrop,
      PLAY · NAME button. Lock in Spyro and confirm you spawn as Spyro.
- [ ] **Spyro stands on the floor** at spawn and while running; capsule not
      visibly too big or small (he's ~110 units tall).
- [ ] **Ruined phoenix**: kill the enemy phoenix (or let the match run); it
      should turn grey with crystals hidden, then restore on respawn.
- [ ] **Sounds**: 12 synthesized placeholders. Are any painful? Which are
      too loud, too quiet, or wrong for the event (hit, cast, ult, death,
      level up, buy, kill, tower shot, respawn, click, recall)?

## Combat feel

- [ ] Hit flash on every target you hit, and on you when hit. Too strong?
- [ ] Camera shake: light on hits and casts, heavy on ults and death. Nausea?
- [ ] Spyro kit: Flame Breath cone size, Charge distance and the speed burst,
      Comet Dash arc and landing AoE, Daybringer channel (1 s root) and cone.
- [ ] Each ult reads as an ult (VFX, shake, sound).
- [ ] Announcer: 3-2-1-FIGHT at start, FIRST BLOOD, god slain, tower
      destroyed, titan vulnerable. Readable, not in the way?
- [ ] Respawn and recall countdowns show as the status line under the banner.

## HUD and shop

- [ ] HUD: portrait, level badge, bars, four slots with cooldown numbers, key
      badges, rank pips, item cells, gold counter. Anything overlapping the
      minimap or kill feed at your resolution?
- [ ] Shop (B): filters, RECOMMENDED shows six items in buy order for your
      kit, buy only in spawn, sell from the inventory row, Escape/B closes.
- [ ] Item descriptions mostly repeat the stat line. Decide whether to drop
      one.

## Economy and balance (the real unknowns)

- [ ] 500 starting gold feels right? Capstone items reachable around minute
      8–12, not 3?
- [ ] AI gods: do they feel like opponents? Do they retreat sensibly, back off
      towers, take jungle buffs, focus the lowest-health hero? Do allies join
      your fights without diving towers?
- [ ] Respawn timers (scale with match time, cap 45 s) too long late?
- [ ] Base protections 10 + 1/level: does early damage feel too low now?

## Map

- [ ] Towers/phoenixes/titans read as structures from the lane; crystals are
      team-colored, not white.
- [ ] Border cliffs: they read as dark floating rock masses. Keep, lower, or
      lighten?
- [ ] Any dressing prop inside the walkable area or blocking a path.
- [ ] Gods reach the center Damage Buff camp by walking (no stalls).

## End of match

- [ ] Victory/defeat screen: both teams' rows, match tiles, PLAY AGAIN and
      MAIN MENU work and unpause.

## Harness

`python Scripts\smoke_test.py --all-characters` and `--items` before and
after any fix; both should pass with 0 errors.
