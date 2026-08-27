# 2048 Game Experience Design

## Goal

Turn the Level 21 2048 demo into a complete, touch-friendly mini game with visible tile motion, clear terminal states, and a persistent local Top 5 leaderboard.

## Interaction

- A valid swipe is locked until its 140 ms movement animation completes.
- Each occupied source tile is represented by a temporary foreground tile that moves to its destination. Two tiles entering the same destination merge into one tile, which briefly scales up and returns to its normal size.
- A newly added tile fades and scales in after the movement phase.
- Reaching 2048 opens a success modal. The player can continue the same run or restart it.
- When no legal move remains, the game records the score and opens a failure modal with restart and leaderboard actions.

## Storage

- The `game2048` NVS namespace stores five signed 32-bit scores under `top0` through `top4`.
- Scores are sorted in descending order. A completed run is stored only when the game is lost, preventing duplicate entries when a player continues after reaching 2048.

## UI and Reliability

- The static 4×4 cells remain the board background. Temporary animated tiles are created only for one turn and deleted at its completion callback.
- Full-screen modal overlays block board input while visible.
- The game uses only LVGL v9 animation APIs and existing `nvs_flash` project support.
- Live source and `code/21_final_station_hub/embedded` remain byte-identical for `ui_game_2048.c`.

## Verification

- A source-level regression contract checks animation duration, input lock, terminal overlays, NVS Top 5 persistence, and copy synchronization.
- `idf.py build` must compile and link the full firmware and produce `build/esp32-start.bin`.
