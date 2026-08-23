# Level 12 Beginner Tutorial and Code Refresh Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make Level 12 a beginner-friendly, internally consistent Wi-Fi, SNTP, and HTTP weather lesson whose archived examples build under ESP-IDF 6.0.2.

**Architecture:** Keep the existing three-example progression. Each example owns the smallest additional concept: bounded Wi-Fi connection, confirmed SNTP synchronization, then an HTTP weather request with defensive JSON parsing. The Markdown teaches the same progression, with runnable commands, expected output, and troubleshooting.

**Tech Stack:** ESP-IDF 6.0.2, FreeRTOS Event Groups, ESP-NETIF SNTP, `esp_http_client`, cJSON, Markdown.

## Global Constraints

- Target is ESP32-D0WD-V3 / ESP32-WROOM-32E on 2.4 GHz Wi-Fi only.
- Do not touch `main/app_main.c`; it contains an unrelated user change.
- Keep examples switchable with `./switch_code.sh 12 <1|2|3>`.
- Use `ESP_LOGI` / `ESP_LOGW` / `ESP_LOGE` and `vTaskDelay(pdMS_TO_TICKS(...))`.
- Build checks prove source integration only; physical Wi-Fi and Internet checks require flash and serial-monitor evidence.

---

### Task 1: Establish a static regression check and repair Wi-Fi example

**Files:**
- Create: `tests/level12_static_check.sh`
- Modify: `code/12_wifi_weather/01_wifi_sta_connect.c`

**Interfaces:**
- Produces `wifi_init_sta(void)`, which logs either a valid STA IP or an explicit retry-limit failure.
- Test exits 0 only when all three examples and the tutorial contain the required current interfaces and teaching anchors.

- [ ] **Step 1: Write the failing static check**

Create an executable shell test that checks for all of the following exact tokens: `esp_netif_sntp_init`, `esp_netif_sntp_sync_wait`, `current=temperature_2m%2Cwind_speed_10m%2Cweather_code`, `cJSON_IsNumber`, `12.10 常见问题`, and `不等于已经在真机上联网成功`.

- [ ] **Step 2: Run the test to verify it fails**

Run: `./tests/level12_static_check.sh`

Expected: nonzero exit and a message naming the first absent token.

- [ ] **Step 3: Implement bounded STA connection handling**

Add the missing `esp_netif.h` include. Retain STA mode and the event group, reset retry count after `IP_EVENT_STA_GOT_IP`, and keep the existing `WIFI_CONNECTED_BIT` / `WIFI_FAIL_BIT` wait so the application only continues after IP assignment or an explicit failure.

- [ ] **Step 4: Compile the example in a disposable copy**

Run: `validation_dir=$(mktemp -d)`, copy the repository into that directory while excluding `.git` and `build`, replace only `"$validation_dir/main/app_main.c"` with `01_wifi_sta_connect.c`, then run `source /Users/calvin/.espressif/v6.0.2/esp-idf/export.sh` and `idf.py -C "$validation_dir" build`.

Expected: `Project build complete.`

### Task 2: Repair SNTP synchronization example

**Files:**
- Modify: `code/12_wifi_weather/02_sntp_time_sync.c`

**Interfaces:**
- Consumes: completed Wi-Fi IP assignment.
- Produces: `sntp_sync_init(void)` returning `bool`, which returns true only after `esp_netif_sntp_sync_wait()` reports success.

- [ ] **Step 1: Extend the static check expectation**

Ensure the check requires `#include "esp_netif_sntp.h"`, `ESP_NETIF_SNTP_DEFAULT_CONFIG`, and `esp_netif_sntp_sync_wait` in experiment 2.

- [ ] **Step 2: Run the test to verify it fails**

Run: `./tests/level12_static_check.sh`

Expected: nonzero exit because experiment 2 still uses the legacy direct SNTP calls.

- [ ] **Step 3: Implement confirmed synchronization**

Use `esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.aliyun.com")`, call `esp_netif_sntp_init(&config)`, and wait up to 10 seconds with `esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000))`. Set the China timezone before formatting with `localtime_r`. On timeout, log a warning and avoid printing a misleading success line.

- [ ] **Step 4: Compile the example in a disposable copy**

Run: `validation_dir=$(mktemp -d)`, copy the repository into that directory while excluding `.git` and `build`, replace only `"$validation_dir/main/app_main.c"` with `02_sntp_time_sync.c`, then run `source /Users/calvin/.espressif/v6.0.2/esp-idf/export.sh` and `idf.py -C "$validation_dir" build`.

Expected: `Project build complete.`

### Task 3: Update the weather API and defensive JSON handling

**Files:**
- Modify: `code/12_wifi_weather/03_http_weather_clock.c`

**Interfaces:**
- Consumes: a Wi-Fi connection with a DHCP-assigned IP.
- Produces: `fetch_weather_task(void *)`, which reports a current temperature, wind speed, and Chinese weather description only after an HTTP 200 response and valid JSON numbers.

- [ ] **Step 1: Run the static test to verify the API expectation fails**

Run: `./tests/level12_static_check.sh`

Expected: nonzero exit because the old URL uses `current_weather=true` and parsing does not use `cJSON_IsNumber`.

- [ ] **Step 2: Implement current Open-Meteo request and parsing**

Use the URL `http://api.open-meteo.com/v1/forecast?latitude=39.9042&longitude=116.4074&current=temperature_2m%2Cwind_speed_10m%2Cweather_code`. Parse `root.current.temperature_2m`, `root.current.wind_speed_10m`, and `root.current.weather_code`; check all three with `cJSON_IsNumber`; report buffer overflow; map a small WMO-code subset to Chinese; always call `cJSON_Delete(root)` after a successful parse.

- [ ] **Step 3: Compile the example in a disposable copy**

Run: `validation_dir=$(mktemp -d)`, copy the repository into that directory while excluding `.git` and `build`, replace only `"$validation_dir/main/app_main.c"` with `03_http_weather_clock.c`, then run `source /Users/calvin/.espressif/v6.0.2/esp-idf/export.sh` and `idf.py -C "$validation_dir" build`.

Expected: `Project build complete.`

### Task 4: Rewrite the beginner tutorial and validate alignment

**Files:**
- Modify: `book/12_WiFi连接管理与HTTP天气时钟.md`
- Modify: `tests/level12_static_check.sh`

**Interfaces:**
- Consumes: the exact code interfaces and logs from Tasks 1–3.
- Produces: a step-by-step lesson with command links, success criteria, safe claims, troubleshooting, active recall, and exercises.

- [ ] **Step 1: Replace the tutorial structure**

Write these sections in order: what the reader will make; preparation and safety; the full `Wi-Fi → IP → DNS → HTTP → JSON` map; STA and event-driven connection; experiment 1; SNTP and timezones; experiment 2; HTTP and JSON; experiment 3; troubleshooting; active recall; exercises; scope boundary and next lesson.

- [ ] **Step 2: Keep all claims testable**

Include expected serial logs for each experiment. State that obtaining an IP does not prove the API is reachable, and that a build does not prove a real device completed Wi-Fi or Internet access. Explain HTTP is a teaching transport and HTTPS certificate validation is required for production.

- [ ] **Step 3: Run static and structural checks**

Run: `./tests/level12_static_check.sh && rg -n 'code/12_wifi_weather/(01_wifi_sta_connect|02_sntp_time_sync|03_http_weather_clock)\\.c' book/12_WiFi连接管理与HTTP天气时钟.md && git diff --check`

Expected: zero exit status and all three source paths found.

- [ ] **Step 4: Preserve the user-selected main program**

Builds run only in disposable repository copies. Verify the workspace `main/app_main.c` hash is unchanged from the value captured before validation; do not write this file.

- [ ] **Step 5: Commit the Level 12 changes**

Run: `git add book/12_WiFi连接管理与HTTP天气时钟.md code/12_wifi_weather tests/level12_static_check.sh && git commit -m "feat(level-12): improve Wi-Fi weather tutorial and examples"`

## Final Verification

- [ ] Source `export.sh` and run all three builds in disposable repository copies.
- [ ] Run `./tests/level12_static_check.sh` and `git diff --check`.
- [ ] Verify the exact pre-task contents of `main/app_main.c` are unchanged relative to its captured hash.
- [ ] Report separately: static test result, build result for each example, and absence of real-device verification.
