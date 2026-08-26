# Level 21 Pomodoro Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the unclear Level 21 Pomodoro screen with a readable focus/rest timer and a working date display.

**Architecture:** Keep the existing once-per-second business tick, but make `ui_pomodoro.c` own a two-phase state machine: focus (25 minutes) and rest (5 minutes). Use one fixed-width `MM:SS` label for the timer, an explicit phase label, a progress bar, and three named controls; pass the existing date string into the large-clock mode through a small UI API.

**Tech Stack:** ESP-IDF 6.0.2, FreeRTOS, LVGL v9, C, shell static regression checks.

## Global Constraints

- Preserve the fixed ST7789/CST816S pin and LVGL rotation configuration from `AGENTS.md`.
- Keep GPIO39 input-only and do not change SW3 navigation semantics.
- Keep LVGL access behind `bsp_lvgl_port_lock()` / `bsp_lvgl_port_unlock()`.
- Do not claim physical display behavior without a connected-board test.

### Task 1: Add the failing Pomodoro regression check

**Files:**
- Create: `tests/level21_pomodoro_check.sh`

- [x] **Step 1: Write the failing static test**

Assert the desired contract: one timer label, `MM:SS` formatting, focus/rest phase state, explicit controls, date update API, and a business-layer call to that API. Also forbid the old split `s_lbl_clock_big`/`s_lbl_clock_sec` timer display and the low-contrast placeholder date.

- [x] **Step 2: Run the test and verify it fails**

Run `bash tests/level21_pomodoro_check.sh`. It should fail against the current implementation because it has no rest phase, still splits the large clock seconds label, and never passes the date into the Pomodoro module.

### Task 2: Implement the Pomodoro state machine and screen

**Files:**
- Modify: `main/ui/ui_pomodoro.c`
- Modify: `main/ui/ui_pomodoro.h`

- [x] **Step 1: Add the phase model and UI update helpers**

Add `POMO_PHASE_FOCUS` and `POMO_PHASE_REST`, durations of 25×60 and 5×60 seconds, one `s_lbl_pomo_time` label, phase/status labels, and a progress bar. The formatter must always use `snprintf(buf, sizeof(buf), "%02d:%02d", minutes, seconds)`.

- [x] **Step 2: Build the readable layout**

Use a visible page title, a phase label, one centered timer, a status description, a progress bar, and three equally sized controls labelled “开始/暂停”, “跳过”, and “重置”. Make the clock-mode date visible with the Chinese font and a brighter color.

- [x] **Step 3: Implement focus/rest transitions**

Start/pause changes only the running flag. Skip advances focus→rest or rest→focus. Reset stops the timer and restores the current phase default duration. When a running phase reaches zero, advance automatically and keep the next phase paused so the user sees the new purpose before starting it.

- [x] **Step 4: Add the date update API**

Expose `void ui_pomodoro_update_date(const char *date_str);`, store the latest date under the LVGL lock, and have the large-clock panel display it.

### Task 3: Connect real date data and archive the source

**Files:**
- Modify: `main/app/app_business.c`
- Modify: `code/21_final_station_hub/embedded/app/app_business.c`
- Modify: `code/21_final_station_hub/embedded/ui/ui_pomodoro.c`
- Modify: `code/21_final_station_hub/embedded/ui/ui_pomodoro.h`

- [x] **Step 1: Pass the existing `date_str` to the Pomodoro UI**

Call `ui_pomodoro_update_date(date_str)` in the same telemetry cycle that already calls `ui_hub_update_time_and_date()`.

- [x] **Step 2: Synchronize the final active sources into `code/21_final_station_hub/embedded/`**

Copy the active Pomodoro source, header, and business source after implementation and verify each pair with `cmp`.

### Task 4: Verify and document

**Files:**
- Modify: `book/21_桌面智能气象站与物联网超级中控.md`

- [x] **Step 1: Run the static checks**

Run `bash tests/level21_pomodoro_check.sh && bash tests/level21_navigation_check.sh && git diff --check`.

- [x] **Step 2: Build the firmware**

Run `source /Users/calvin/.espressif/tools/activate_idf_v6.0.2.sh && idf.py build`.

- [x] **Step 3: Update the Level 21 tutorial**

Document the visible phase label, fixed `MM:SS` format, three controls, automatic focus/rest transition, and date source.

- [x] **Step 4: Verify the final diff**

Confirm the active/archive Pomodoro sources match, the static checks pass, and report that no board was flashed unless a real-device run was performed.
