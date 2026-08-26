# Level 21 App Launcher Navigation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Keep only four frequently used shortcut pages around the home dashboard, move photo album, novel reader, and 2048 into a touch-selected app launcher, and make SW3 provide consistent home/launcher navigation.

**Architecture:** Retain a five-tile LVGL TileView containing home, control center, sensor analytics, pomodoro, and system status. Create a full-screen app layer above the TileView; its launcher and app containers are mutually exclusive, so app gestures never reach the TileView. A dedicated button polling task converts SW3 short/long presses into UI navigation or the existing Wi-Fi reset action.

**Tech Stack:** ESP-IDF 6.0.2, FreeRTOS, LVGL v9, C, shell-based static regression checks, `idf.py build`.

## Global Constraints

- Preserve the fixed ST7789/CST816S pin and LVGL rotation configuration from `AGENTS.md`.
- Keep GPIO39 as input-only and preserve the existing 3-second Wi-Fi reset behavior.
- Use `ESP_LOGI`/`ESP_LOGW`/`ESP_LOGE` and `vTaskDelay(pdMS_TO_TICKS(...))`.
- Do not modify `code/` until the active `main/` implementation and build verification succeed.
- Do not claim physical touch or button behavior without a connected-board test.

### Task 1: Add a failing Level 21 navigation regression check

**Files:**
- Create: `tests/level21_navigation_check.sh`

- [x] **Step 1: Write the failing static test**

The test must assert that the active UI has exactly the five retained TileView pages, has a launcher, does not initialize the three app pages directly as TileView tiles, and does not bubble 2048 gestures.

- [x] **Step 2: Run the test and verify it fails**

Run: `bash tests/level21_navigation_check.sh`
Expected: FAIL because the current code still creates photo, novel, and 2048 TileView tiles and still contains `LV_OBJ_FLAG_GESTURE_BUBBLE`.

### Task 2: Refactor the UI navigation and app launcher

**Files:**
- Modify: `main/ui/ui_hub.c`
- Modify: `main/ui/ui_hub.h`
- Modify: `main/ui/ui_game_2048.c`

- [x] **Step 1: Keep only home plus four shortcut TileView pages**

Retain control center, analytics, pomodoro, and system status. Remove photo, novel, and game TileView creation and remove the home-page directional hint text.

- [x] **Step 2: Add a full-screen launcher/app layer**

Create a touch-selectable list for `2048`, `电子相册`, and `小说阅读器`; create one full-screen parent container per app, initialize each existing app UI inside its parent, and switch visibility while hiding the TileView.

- [x] **Step 3: Add public SW3 navigation handling**

Expose `ui_hub_handle_sw3_short_press()`. On home it opens the launcher; on a shortcut page or running app it returns to home. App selection opens only the selected app container.

- [x] **Step 4: Isolate 2048 gestures**

Remove `LV_OBJ_FLAG_GESTURE_BUBBLE` from the 2048 board so its four-way gesture callback cannot switch the surrounding TileView.

### Task 3: Make SW3 navigation independent from telemetry

**Files:**
- Modify: `main/app/app_business.c`

- [x] **Step 1: Remove the blocking SW3 polling block from `sensor_telemetry_task`**

The sensor task must continue sampling once per second without entering a 3.2-second button wait loop.

- [x] **Step 2: Add a dedicated button task**

Poll GPIO39 every 20 ms, debounce the press, dispatch a short press to `ui_hub_handle_sw3_short_press()`, and dispatch a long press of at least 3 seconds to `net_manager_reset_credentials()`.

- [x] **Step 3: Start the button task after UI initialization**

Create it from `app_business_start()` with a FreeRTOS stack and priority appropriate for responsive UI navigation.

### Task 4: Verify and document the new interaction contract

**Files:**
- Modify: `book/21_桌面智能气象站与物联网超级中控.md`
- Modify: `README.md` only if the visible Level 21 navigation description is present there.

- [x] **Step 1: Run the static regression check**

Run: `bash tests/level21_navigation_check.sh`
Expected: PASS.

- [x] **Step 2: Build the active firmware**

Run: `idf.py build`
Expected: successful ESP-IDF build with no new compiler errors.

- [x] **Step 3: Update the Level 21 tutorial**

Document the five shortcut pages, the three launcher apps, SW3 short/long press behavior, and the rule that app gestures are isolated from the outer TileView.

- [x] **Step 4: Re-run the static check and inspect the diff**

Run: `bash tests/level21_navigation_check.sh && git diff --check && git status --short`
Expected: PASS, no whitespace errors, and only intended files changed.

### Task 5: Restore the home dashboard with a full-card lighting control

**Files:**
- Modify: `main/ui/ui_hub.c`
- Modify: `book/21_桌面智能气象站与物联网超级中控.md`
- Modify: `tests/level21_navigation_check.sh`

- [x] **Step 1: Restore the existing title/status widgets on the home tile**

Restore the existing Wi-Fi, SD-card, and heap status labels in the title bar. Keep detailed network/IP and storage information in the control center and system status shortcut page.

- [x] **Step 2: Keep the original lower home layout and make lighting one full-card button**

Keep the original lower card sizes and the temperature/humidity/comfort overview. Convert the right lighting card itself into the LED2 button, with no nested button, IP label, or distance measurement.

- [x] **Step 3: Restore the existing home data bindings**

Keep the existing location/weather pointers and restore their update paths. Keep the title status refresh and the control-center Wi-Fi status update in sync.

- [x] **Step 4: Verify the home layout**

Run: `bash tests/level21_navigation_check.sh && idf.py build && git diff --check`
Expected: PASS, successful ESP-IDF build, and no whitespace errors.
