param(
    [string]$Source = "main/ui/ui_game_2048.c",
    [string]$Archive = "code/21_final_station_hub/embedded/ui/ui_game_2048.c"
)

$text = Get-Content -Raw $Source
$requirements = @(
    @{ Name = 'Top 5 score capacity'; Pattern = '#define\s+TOP_SCORE_COUNT\s+5' },
    @{ Name = 'NVS leaderboard load'; Pattern = 'nvs_get_i32' },
    @{ Name = 'NVS leaderboard save'; Pattern = 'nvs_set_i32' },
    @{ Name = 'turn input lock'; Pattern = 's_animating' },
    @{ Name = 'tile move duration'; Pattern = '#define\s+TILE_MOVE_DURATION\s+140' },
    @{ Name = 'turn completion callback'; Pattern = 'on_turn_animation_done' },
    @{ Name = 'win modal'; Pattern = 'show_win_modal' },
    @{ Name = 'loss modal'; Pattern = 'show_loss_modal' },
    @{ Name = 'leaderboard modal'; Pattern = 'show_leaderboard_modal' }
)

$missing = $requirements | Where-Object { $text -notmatch $_.Pattern }
if ($missing) {
    $missing | ForEach-Object { Write-Output "FAIL: Missing $($_.Name)" }
    exit 1
}

$modalTitle = [regex]::Match($text, '(?s)static void modal_title\(.*?\n\}').Value
if ($modalTitle -notmatch 'sys_font_manager_get_font\(14\)') {
    Write-Output 'FAIL: Missing Chinese modal font'
    exit 1
}

if ((Get-FileHash $Source).Hash -ne (Get-FileHash $Archive).Hash) {
    Write-Output 'FAIL: Archived Level 21 source is out of sync'
    exit 1
}

Write-Output 'PASS: 2048 animation, terminal screens, leaderboard, and archive contract satisfied'
