# Skylanders Conquest — Roadmap

A SMITE-style 3v3 MOBA with Skylanders characters (UE 5.8). This is a personal
prototype — the guiding priority is **"a single match that feels like a real
game,"** not shipping. That means we optimize for a polished solo-vs-bots
experience and treat networking as optional/last.

Status legend: `[ ]` todo · `[~]` in progress · `[x]` done

---

## Tier 1 — Make one match feel complete
*(all doable solo-vs-bots, no networking; highest value-per-effort)*

- [x] **Win/lose condition + end-of-match screen** — already implemented
  (Titan `Die()` -> `ShowEndScreen`, VICTORY/DEFEAT + stats + pause); verified.
- [x] **Match timer on the HUD** — `SkylandersMatchStatusWidget` (top-center bar).
- [x] **Kill feed** — top-right auto-expiring feed (`SkylandersKillFeedWidget`);
  posts god kills, player deaths, structures, and camps.
- [x] **Team gold** — cumulative earned gold per team (blue = player
  `TotalGoldEarned`, red = enemy god gold) shown in the status bar; sums real
  teammates once Tier 2 adds them.
- [x] **Flesh out items + first balance pass** — 20 items (8 off / 6 def / 6 utl),
  basic -> capstone tiers, tuned cost/stat ratios.
- [x] **Scoreboard / death-recap polish** — two-team table with level, K/D/A,
  CS, gold and items for every hero (Nov 2026).

## Tier 2 — Make it feel like a real 3v3
*(bigger, still bots)*

- [~] **Teams of three** — DONE (needs playtest): god AI is now team-aware
  (`ETowerTeam Team`), spawns a 3v3 (player + 2 blue AI allies vs 3 red AI
  enemies), with friendly-fire prevented for the player's shots/abilities and
  friendly towers. Allies lane toward the enemy base, blue; enemies red. Towers
  now shoot gods on both sides, minions target opposing gods, gods can actually
  auto-attack each other, and kills/farm are credited to whoever landed the blow
  (see `docs/AUDIT-2026-08-02.md`). Allies now join the player's fights (Nov 2026).
- [~] **Better enemy AI + real character** — real models DONE: all 5 AI gods now
  wear their own distinct animated Skylander (Spyro, Stealth Elf, Kaos Sensei,
  Chompy Mage, Cynder) + team-colored ground rings. NOTE: the earlier "only Hex
  and TreeRex have animations" limit was wrong — it was a sparse-checkout artifact
  of the asset repo; 227 characters have full anim sets upstream (`git
  sparse-checkout add Assets/Skylanders/Animations/<Name>`). Nov 2026: tower back-off hysteresis, jungling for
  buff camps, focus-the-squishy targeting. Remaining: titan/boss objective play.
- [x] **Replace minion placeholders** — melee = Terrabite, ranged = Drobit, with
  the old tinted capsule flattened into a team-coloured foot ring. Jungle camps
  too: Shaman (mana x2), GraveClobberer (damage), GoldenQueen (boss), BadJuju.
- [ ] **Towers/titans still geometric** — the rip has NO structure/environment
  geometry (characters only), so these can't become real Skylanders buildings.
  Options: better authored geometry (base + column + team crystal), or an
  environment pack from Fab.

## Tier 3 — Presentation
- [ ] **Map set-dressing** — prop/structure models on the greybox (trees, rocks,
  real base structures); further map polish.
- [~] **Menu / hub UI polish** — HUD rebuilt in C++ with one palette, announcer,
  scoreboard (Nov 2026). Remaining: menu/select/shop art, ability tooltips,
  Spyro portrait and icons.
- [~] **Audio** — 12 synthesized placeholder cues cover every event (Nov 2026);
  replace with real SFX and add music.

## Tier 4 — Heavy infrastructure *(optional given "won't get legs")*
- [ ] **Networking** — real multiplayer 3v3 (replication, server authority, lag
  comp). Largest single item; only worth it if people will actually play together.
- [~] **Performance** — the per-actor-per-tick `GetAllActorsOfClass` scans are
  gone (`USkylandersActorRegistry`, Nov 2026). Further work waits until there's
  enough on screen to stress it.

---

## Tooling
- [x] **Headless smoke test** — `Scripts/smoke_test.py` runs a bot-driven match
  under -nullrhi per playable kit and gates every change (Nov 2026).
- [x] **Telemetry** — per-match JSONL event log + 2.5s god snapshots in
  `Saved/Telemetry/` (`USkylandersTelemetrySubsystem`). Deaths, abilities,
  structures, match result, and movement/`stuck` flags for data-driven tuning.
- [x] **NavMesh pathing for gods** — DONE: runtime NavMesh (dynamic generation +
  bounds volume in Joust.umap) + AIController MoveTo. Telemetry-verified: gods
  now path 7483->3414 down the lane instead of freezing at their base structures.

## Also on the pile (fold into tiers as we go)
- [x] Options / settings menu (audio, controls, video) — `SkylandersSettingsWidget`
  shared by the front-end and the pause menu; plus an Escape/P pause menu and
  PLAY AGAIN / MAIN MENU on the end screen (PR #2).
- [ ] Make jungle objectives pay off (Bull Demon King worth contesting).
- [ ] Finish ability telegraphs/targeters for every kit.
- [x] Buff camps: kill → buff → gold loop works for the player and AI gods.
- [ ] Recall/back-to-base UX polish (channel bar, cancel feedback).

## Known bugs / debt
- Buff camp damage was flagged as not registering earlier; user reports camps are
  now killable — re-verify the full kill→buff loop when we touch Tier 1 economy.
- ~~One Hex animation (`drive_modswap_pose02`) showed as deleted in the working
  tree~~ — present and tracked; resolved.
