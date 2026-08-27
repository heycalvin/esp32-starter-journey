# 2048 Game Experience Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add animated tile movement, win/loss modal screens, and a persistent local Top 5 leaderboard to Level 21's 2048 game.

**Architecture:** Keep the static board cells as background slots. On a valid move, derive source-to-destination paths from the previous and next board states, animate temporary foreground tiles, then commit the static board after the turn animation completes. Store only terminal loss scores in NVS and render ranking and terminal interfaces as modal overlays.

**Tech Stack:** ESP-IDF v6.0.2, LVGL v9, NVS Flash, C.

## Global Constraints

- Modify both `main/ui/ui_game_2048.c` and `code/21_final_station_hub/embedded/ui/ui_game_2048.c` identically.
- Retain the 240×280 display constraints and existing font manager.
- Use `ESP_LOG*` macros for new diagnostics; use NVS namespace `game2048` only for leaderboard storage.
- Preserve unrelated existing worktree changes.

---

### Task 1: Build a testable turn model and Top 5 persistence

**Files:**
- Modify: `main/ui/ui_game_2048.c`
- Modify: `code/21_final_station_hub/embedded/ui/ui_game_2048.c`

- [ ] Write a failing source-level regression contract requiring `TOP_SCORE_COUNT`, `nvs_get_i32`, `nvs_set_i32`, and sorted five-entry score insertion.
- [ ] Run the contract and confirm it fails because persistence is absent.
- [ ] Add Top 5 load, insert, and save helpers using NVS namespace `game2048`.
- [ ] Re-run the contract and confirm the persistence requirements pass.

### Task 2: Animate valid tile movements

**Files:**
- Modify: `main/ui/ui_game_2048.c`
- Modify: `code/21_final_station_hub/embedded/ui/ui_game_2048.c`

- [ ] Extend the regression contract to require animation duration, an input lock, movement completion callback, and spawned-tile animation.
- [ ] Run the contract and confirm it fails because the turn is still immediate.
- [ ] Capture the pre-move board, create temporary tile overlays, animate their positions for 140 ms, and commit the board only in the completion callback.
- [ ] Re-run the contract and confirm animation requirements pass.

### Task 3: Add success, failure, and leaderboard screens

**Files:**
- Modify: `main/ui/ui_game_2048.c`
- Modify: `code/21_final_station_hub/embedded/ui/ui_game_2048.c`

- [ ] Extend the contract to require separate win/loss overlays, continue/restart actions, and a Top 5 view.
- [ ] Run the contract and confirm it fails because only status text is present.
- [ ] Add modal overlays that block board input, route buttons to continue/restart/ranking actions, and save loss scores before showing the failure screen.
- [ ] Re-run the contract and confirm all UI requirements pass.

### Task 4: Synchronize and verify

**Files:**
- Modify: `main/ui/ui_game_2048.c`
- Modify: `code/21_final_station_hub/embedded/ui/ui_game_2048.c`

- [ ] Check both copies have identical hashes and run `git diff --check`.
- [ ] Run `idf.py build` from the ESP-IDF v6.0.2 environment.
- [ ] Confirm `build/esp32-start.bin` is generated and report the result without modifying unrelated files.
