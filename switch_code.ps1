<#
.SYNOPSIS
    ESP32 物联网实战闯关 —— 示例代码一键切换工具 (Windows PowerShell 原生版)
.DESCRIPTION
    用法:
      .\switch_code.ps1 list                     # 列出所有可用的关卡与实验
      .\switch_code.ps1 <关卡号> [实验号]         # 切换指定实验代码到 main/app_main.c
      .\switch_code.ps1 <关卡号> [实验号] --build # 切换并立即编译
      .\switch_code.ps1 <关卡号> [实验号] --flash # 切换并立即烧录监控
#>

param (
    [Parameter(Position=0)]
    [string]$Arg1,

    [Parameter(Position=1)]
    [string]$Arg2,

    [Parameter(Position=2)]
    [string]$Arg3
)

$ErrorActionPreference = "Stop"

$ProjectRoot = $PSScriptRoot
$CodeDir = Join-Path $ProjectRoot "code"
$TargetMain = Join-Path $ProjectRoot "main\app_main.c"

function Print-Banner {
    Write-Host "=================================================================" -ForegroundColor Cyan
    Write-Host "   🛠️  ESP32 实战教程 —— 示例代码一键切换工具 (switch_code)" -ForegroundColor Cyan
    Write-Host "=================================================================" -ForegroundColor Cyan
}

function Print-Usage {
    Print-Banner
    Write-Host "📖 用法:" -ForegroundColor Yellow
    Write-Host "  .\switch_code.ps1 list                     # 列出所有可用的关卡与实验"
    Write-Host "  .\switch_code.ps1 <关卡号> [实验号]         # 切换指定实验代码到 main/app_main.c"
    Write-Host "  .\switch_code.ps1 <关卡号> [实验号] --build # 切换并立即编译"
    Write-Host "  .\switch_code.ps1 <关卡号> [实验号] --flash # 切换并立即烧录监控"
    Write-Host ""
    Write-Host "💡 示例:" -ForegroundColor Green
    Write-Host "  .\switch_code.ps1 1                        # 切换到 Level 01: 串口通信"
    Write-Host "  .\switch_code.ps1 8 1                      # 切换到 Level 08 实验 1: I2C Scanner"
    Write-Host "  .\switch_code.ps1 8 1 --flash              # 切换并立即烧录"
    Write-Host "=================================================================" -ForegroundColor Cyan
}

function List-Experiments {
    Print-Banner
    Write-Host "📂 当前可供切换运行的示例代码列表：`n" -ForegroundColor Green

    $chapters = @(
        @{ Dir="01_hello_world"; Title="第 01 关: 串口通信与 Hello World" },
        @{ Dir="02_gpio_pwm"; Title="第 02 关: GPIO 输出与 PWM 呼吸灯" },
        @{ Dir="03_gpio_input"; Title="第 03 关: 按键输入检测与人体红外感应" },
        @{ Dir="04_gpio_interrupt"; Title="第 04 关: GPIO 外部中断与事件驱动" },
        @{ Dir="05_freertos_queue"; Title="第 05 关: FreeRTOS 多任务与双核队列" },
        @{ Dir="06_ws2812_rmt"; Title="第 06 关: RMT 硬件脉冲与 WS2812 幻彩 RGB" },
        @{ Dir="07_adc_ultrasonic"; Title="第 07 关: ADC 模拟量采集与超声波测距" },
        @{ Dir="08_i2c_dht11"; Title="第 08 关: I2C 通信总线与 DHT11 温湿度" },
        @{ Dir="09_nvs_storage"; Title="第 09 关: NVS 存储与 Flash 偏好设置" },
        @{ Dir="10_st7789_display"; Title="第 10 关: ST7789 彩屏与几何图形渲染" },
        @{ Dir="11_lvgl_touch"; Title="第 11 关: LVGL 图形框架、电容触摸与字库适配" },
        @{ Dir="12_wifi_weather"; Title="第 12 关: WiFi 联网与 HTTP 天气时钟" },
        @{ Dir="13_mqtt_iot"; Title="第 13 关: MQTT 物联网通信与手机远程控制" },
        @{ Dir="14_ble_gatt"; Title="第 14 关: BLE 蓝牙广播与手机透传遥控" },
        @{ Dir="15_espnow_remote"; Title="第 15 关: ESP-NOW 超低延迟私有通信与双机遥控" },
        @{ Dir="16_web_server_portal"; Title="第 16 关: Web Server 网页中控与 AP 门户配网" },
        @{ Dir="17_ota_firmware"; Title="第 17 关: OTA 空中无线升级与 A/B 分区防变砖" },
        @{ Dir="18_sdcard_fatfs"; Title="第 18 关: TF 卡 SDIO 驱动与 FatFS 文件系统" },
        @{ Dir="19_low_power_deepsleep"; Title="第 19 关: 低功耗电源管理与 Deep-sleep 休眠" },
        @{ Dir="20_software_architecture"; Title="第 20 关: 嵌入式分层架构、事件总线与看门狗" },
        @{ Dir="21_final_station_hub"; Title="第 21 关: 桌面智能气象站与物联网超级中控台" }
    )

    foreach ($ch in $chapters) {
        $chPath = Join-Path $CodeDir $ch.Dir
        Write-Host "▶ $($ch.Title)" -ForegroundColor Cyan -NoNewline
        Write-Host "  (目录: code/$($ch.Dir))" -ForegroundColor Gray

        if (Test-Path $chPath) {
            # 1. 检查是否是全栈单一大工程 (如 21_final_station_hub 包含 embedded/)
            $embDir = Join-Path $chPath "embedded"
            if (Test-Path $embDir) {
                $shortPrefix = $ch.Dir.Substring(0, 2)
                Write-Host "   [" -NoNewline
                Write-Host "$shortPrefix" -ForegroundColor Yellow -NoNewline
                Write-Host "] 🏆 embedded/ (桌面智能气象站与中控台嵌入式端)  " -NoNewline
                Write-Host "🌟 全栈一体化大工程 [embedded/ + server/ + app/]" -ForegroundColor DarkGray
            }
            # 2. 检查是否包含子工程目录 (如 01_bsp_decoupling, 02_fsm_state_machine)
            elseif ($subExpDirs = Get-ChildItem -Path $chPath -Directory | Where-Object { $_.Name -match '^\d{2}_' } | Sort-Object Name) {
                $idx = 1
                foreach ($sd in $subExpDirs) {
                    $firstLine = ""
                    $entryFile = Join-Path $sd.FullName "app_main.c"
                    if (Test-Path $entryFile) {
                        $content = Get-Content -Path $entryFile -TotalCount 10
                        $match = $content | Where-Object { $_ -match "🌟|🚀|📁" } | Select-Object -First 1
                        if ($match) { $firstLine = $match.Trim() }
                    }
                    $shortPrefix = $ch.Dir.Substring(0, 2)
                    Write-Host "   [" -NoNewline
                    Write-Host "$shortPrefix $idx" -ForegroundColor Yellow -NoNewline
                    Write-Host "] 📁 $($sd.Name)/  " -NoNewline
                    Write-Host "$firstLine" -ForegroundColor DarkGray
                    $idx++
                }
            } else {
                # 3. 标准单文件列表
                $files = Get-ChildItem -Path $chPath -Filter "*.c" | Sort-Object Name
                $idx = 1
                foreach ($f in $files) {
                    $firstLine = ""
                    if (Test-Path $f.FullName) {
                        $content = Get-Content -Path $f.FullName -TotalCount 10
                        $match = $content | Where-Object { $_ -match "🌟|🚀|📁" } | Select-Object -First 1
                        if ($match) { $firstLine = $match.Trim() }
                    }
                    $shortPrefix = $ch.Dir.Substring(0, 2)
                    Write-Host "   [" -NoNewline
                    Write-Host "$shortPrefix $idx" -ForegroundColor Yellow -NoNewline
                    Write-Host "] $($f.Name)  " -NoNewline
                    Write-Host "$firstLine" -ForegroundColor DarkGray
                    $idx++
                }
            }
        }
        Write-Host ""
    }
    Write-Host "=================================================================" -ForegroundColor Cyan
}

# 1. 判断是否是 list 或 help
if ($Arg1 -eq "list" -or $Arg1 -eq "--list" -or $Arg1 -eq "-l" -or $Arg1 -eq "-list") {
    List-Experiments
    exit 0
}

if ([string]::IsNullOrWhiteSpace($Arg1) -or $Arg1 -eq "help" -or $Arg1 -eq "--help" -or $Arg1 -eq "-h") {
    Print-Usage
    exit 0
}

# 2. 解析关卡号与实验号
$chNum = $Arg1
if ($chNum -match '^\d+$') {
    $chNum = "{0:D2}" -f [int]$chNum
}

$expNum = 1
$actionFlag = ""

if (-not [string]::IsNullOrWhiteSpace($Arg2)) {
    if ($Arg2 -match '^\d+$') {
        $expNum = [int]$Arg2
    } elseif ($Arg2.StartsWith("-")) {
        $actionFlag = $Arg2
    }
}

if (-not [string]::IsNullOrWhiteSpace($Arg3)) {
    if ($Arg3.StartsWith("-")) {
        $actionFlag = $Arg3
    }
}

# 查找对应关卡目录
$matchDirs = Get-ChildItem -Path $CodeDir -Directory | Where-Object { $_.Name -like "${chNum}_*" }
if (-not $matchDirs -or $matchDirs.Count -eq 0) {
    Write-Host "`n❌ 错误：未找到第 $chNum 关对应的代码目录！" -ForegroundColor Red
    Write-Host "使用 '.\switch_code.ps1 list' 查看全部可用关卡。" -ForegroundColor Yellow
    exit 1
}

$targetDir = $matchDirs[0].FullName
$paddedExp = "{0:D2}" -f $expNum

# 3. 先彻底清理 main 目录下的历史残留子文件夹 (确保工作区 100% 纯净)
$subDirsToClean = @("bsp", "services", "app", "ui", "components", "events")
foreach ($d in $subDirsToClean) {
    $p = Join-Path $ProjectRoot "main\$d"
    if (Test-Path $p) {
        Remove-Item -Path $p -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# 4. 判断目标实验类型
$embeddedPath = Join-Path $targetDir "embedded"
if (-not (Test-Path $embeddedPath)) {
    $embeddedPath = Join-Path $targetDir "firmware"
}
$matchSubDir = Get-ChildItem -Path $targetDir -Directory | Where-Object { $_.Name -like "${paddedExp}_*" }

if (Test-Path $embeddedPath) {
    # ── 模式 A1: 关卡 21 从 embedded/ 子目录拉取固件代码到 main/ ──
    Get-ChildItem -Path $embeddedPath | ForEach-Object {
        if ($_.Name -eq "partitions.csv") {
            Copy-Item -Path $_.FullName -Destination (Join-Path $ProjectRoot "partitions.csv") -Force
        } elseif ($_.PSIsContainer) {
            Copy-Item -Path $_.FullName -Destination (Join-Path $ProjectRoot "main\$($_.Name)") -Recurse -Force
        } else {
            Copy-Item -Path $_.FullName -Destination (Join-Path $ProjectRoot "main\$($_.Name)") -Force
        }
    }
    $relSrc = $embeddedPath.Replace($ProjectRoot, "").TrimStart("\/")
    Write-Host "=================================================================" -ForegroundColor Green
    Write-Host "🏆 成功切换第 21 关毕业设计全栈大工程！" -ForegroundColor Green
    Write-Host "   嵌入式源目录: " -NoNewline
    Write-Host "$relSrc" -ForegroundColor Yellow
    Write-Host "   目标工作区:   " -NoNewline
    Write-Host "main/ [已拉取完整的 BSP + Services + UI + App 模块]" -ForegroundColor Cyan
    Write-Host "=================================================================" -ForegroundColor Green
} elseif ($matchSubDir -and $matchSubDir.Count -gt 0) {
    # ── 模式 A2: 复制子工程目录到 main/ ──
    $expDir = $matchSubDir[0].FullName
    Get-ChildItem -Path $expDir | ForEach-Object {
        if ($_.Name -eq "partitions.csv") {
            Copy-Item -Path $_.FullName -Destination (Join-Path $ProjectRoot "partitions.csv") -Force
        } elseif ($_.PSIsContainer) {
            Copy-Item -Path $_.FullName -Destination (Join-Path $ProjectRoot "main\$($_.Name)") -Recurse -Force
        } else {
            Copy-Item -Path $_.FullName -Destination (Join-Path $ProjectRoot "main\$($_.Name)") -Force
        }
    }
    $relSrc = $expDir.Replace($ProjectRoot, "").TrimStart("\/")
    Write-Host "=================================================================" -ForegroundColor Green
    Write-Host "✅ 成功切换多文件模块化工程！" -ForegroundColor Green
    Write-Host "   源工程: " -NoNewline
    Write-Host "$relSrc" -ForegroundColor Yellow
    Write-Host "   目标位: " -NoNewline
    Write-Host "main/ [包含独立分层组件]" -ForegroundColor Cyan
    Write-Host "=================================================================" -ForegroundColor Green
} else {
    # ── 模式 B: 标准单文件切换 ──
    $expFiles = Get-ChildItem -Path $targetDir -Filter "*.c" | Sort-Object Name
    if (-not $expFiles -or $expFiles.Count -eq 0) {
        Write-Host "`n❌ 错误：第 $chNum 关未找到编号为 $expNum 的实验或子工程！" -ForegroundColor Red
        exit 1
    }

    $matchedByPrefix = $expFiles | Where-Object { $_.Name -like "${paddedExp}_*.c" }
    if ($matchedByPrefix -and $matchedByPrefix.Count -gt 0) {
        $targetSrc = $matchedByPrefix[0].FullName
    } elseif ($expNum -le $expFiles.Count -and $expNum -ge 1) {
        $targetSrc = $expFiles[$expNum - 1].FullName
    } else {
        Write-Host "`n❌ 错误：第 $chNum 关未找到编号为 $expNum 的实验！" -ForegroundColor Red
        exit 1
    }

    Copy-Item -Path $targetSrc -Destination $TargetMain -Force
    $relSrc = $targetSrc.Replace($ProjectRoot, "").TrimStart("\/")
    Write-Host "=================================================================" -ForegroundColor Green
    Write-Host "✅ 成功切换单文件示例代码！" -ForegroundColor Green
    Write-Host "   源文件: " -NoNewline
    Write-Host "$relSrc" -ForegroundColor Yellow
    Write-Host "   目标位: " -NoNewline
    Write-Host "main/app_main.c" -ForegroundColor Cyan
    Write-Host "=================================================================" -ForegroundColor Green
}

# 5. 检查是否需要编译或烧录
if ($actionFlag -eq "--build" -or $actionFlag -eq "-b") {
    Write-Host "`n🔨 正在执行编译 (idf.py build)..." -ForegroundColor Cyan
    idf.py build
} elseif ($actionFlag -eq "--flash" -or $actionFlag -eq "-f") {
    Write-Host "`n⚡ 正在执行烧录与串口监视 (idf.py flash monitor)..." -ForegroundColor Cyan
    idf.py flash monitor
}

