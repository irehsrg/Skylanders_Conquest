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
- [ ] **Scoreboard / death-recap polish** — the widget exists; make it read well
  (KDA, gold, items per player).

## Tier 2 — Make it feel like a real 3v3
*(bigger, still bots)*

- [~] **Teams of three** — DONE (needs playtest): god AI is now team-aware
  (`ETowerTeam Team`), spawns a 3v3 (player + 2 blue AI allies vs 3 red AI
  enemies), with friendly-fire prevented for the player's shots/abilities and
  friendly towers. Allies lane toward the enemy base, blue; enemies red. TODO
  from playtest: tune ally aggressiveness, enemy towers ignore ally gods, minions
  don't yet target gods.
- [ ] **Better enemy AI + real character** — give the god(s) a real Skylander
  model (still colored cylinders) and smarter combat/objective AI.
- [ ] **Replace minion placeholders** with real minion models.

## Tier 3 — Presentation
- [ ] **Map set-dressing** — prop/structure models on the greybox (trees, rocks,
  real base structures); further map polish.
- [ ] **Menu / hub UI polish** — main menu, character select, shop, HUD styling.
- [ ] **Audio** — infrastructure is wired but no clips assigned; add SFX + music.

## Tier 4 — Heavy infrastructure *(optional given "won't get legs")*
- [ ] **Networking** — real multiplayer 3v3 (replication, server authority, lag
  comp). Largest single item; only worth it if people will actually play together.
- [ ] **Performance** — optimize once there's enough on screen to stress it.

---

## Tooling
- [x] **Telemetry** — per-match JSONL event log + 2.5s god snapshots in
  `Saved/Telemetry/` (`USkylandersTelemetrySubsystem`). Deaths, abilities,
  structures, match result, and movement/`stuck` flags for data-driven tuning.
- [x] **NavMesh pathing for gods** — DONE: runtime NavMesh (dynamic generation +
  bounds volume in Joust.umap) + AIController MoveTo. Telemetry-verified: gods
  now path 7483->3414 down the lane instead of freezing at their base structures.

## Also on the pile (fold into tiers as we go)
- [ ] Options / settings menu (audio, controls, video).
- [ ] Make jungle objectives pay off (Bull Demon King worth contesting).
- [ ] Finish ability telegraphs/targeters for every kit.
- [ ] Buff camps: confirm kill → buff → XP/gold loop end to end.
- [ ] Recall/back-to-base UX polish (channel bar, cancel feedback).

## Known bugs / debt
- Buff camp damage was flagged as not registering earlier; user reports camps are
  now killable — re-verify the full kill→buff loop when we touch Tier 1 economy.
- One Hex animation (`drive_modswap_pose02`) showed as deleted in the working
  tree; restore if it was accidental.
