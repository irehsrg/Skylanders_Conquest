# Skylanders Conquest — Roadmap

A SMITE-style 3v3 MOBA with Skylanders characters (UE 5.8). This is a personal
prototype — the guiding priority is **"a single match that feels like a real
game,"** not shipping. That means we optimize for a polished solo-vs-bots
experience and treat networking as optional/last.

Status legend: `[ ]` todo · `[~]` in progress · `[x]` done

---

## Tier 1 — Make one match feel complete
*(all doable solo-vs-bots, no networking; highest value-per-effort)*

- [ ] **Win/lose condition + end-of-match screen** — destroy the enemy Titan =
  victory, lose yours = defeat; show a result screen and stop the match.
- [ ] **Match timer on the HUD** — real widget element (replaces debug text).
- [ ] **Kill feed** — on-screen feed of kills (replaces debug messages).
- [ ] **Team gold** — shared team economy, tracked + displayed.
- [ ] **Flesh out items + first balance pass** — enough meaningful items that
  shopping matters; a first numbers balance pass.
- [ ] **Scoreboard / death-recap polish** — the widget exists; make it read well
  (KDA, gold, items per player).

## Tier 2 — Make it feel like a real 3v3
*(bigger, still bots)*

- [ ] **Teams of three** — AI allies + enemies (3 per side), not just the player.
- [ ] **Better enemy AI + real character** — give the enemy god(s) a real
  Skylander model and smarter combat/objective AI.
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
