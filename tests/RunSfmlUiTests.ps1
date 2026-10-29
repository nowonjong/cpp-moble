param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$projectRoot=Split-Path $PSScriptRoot -Parent
$executable=Join-Path $projectRoot "bin\$Configuration\prototype.exe"
if (-not (Test-Path -LiteralPath $executable)) { throw 'Build prototype.sln first (x64).' }
# This unique fixture keeps the real words and records untouched.
$testRoot=Join-Path $env:TEMP ('wordchain_sfml_ui_'+[guid]::NewGuid().ToString('N'))
$data=Join-Path $testRoot 'data'
$captures=Join-Path $testRoot 'captures'
New-Item -ItemType Directory -Path $data,$captures -Force | Out-Null
$words=@('사과','과자','자동차','차표','바다','다리','리본')
[IO.File]::WriteAllLines((Join-Path $data 'words.txt'),$words,[Text.UTF8Encoding]::new($false))
$branding=Join-Path $projectRoot 'data\branding'
if (Test-Path -LiteralPath $branding) { Copy-Item -LiteralPath $branding -Destination $data -Recurse }
$arguments=@('--data-dir',('"'+$data+'"'),'--capture-dir',('"'+$captures+'"'),'--ui-test')
$process=Start-Process -FilePath $executable -ArgumentList $arguments -WindowStyle Hidden -PassThru
if (-not $process.WaitForExit(30000)) { throw "UI test timed out; inspect its window (PID $($process.Id)). Artifacts: $testRoot" }
$process.Refresh()
if ($process.ExitCode -ne 0) { throw "UI test failed: $($process.ExitCode). Artifacts: $testRoot" }
$expectedCaptures=@(
    '01_home.png','01_profile_default.png','01_new_profile.png','02_game.png','03_validation.png',
    '03_pause.png','03_pause_close_confirmation.png','03_pause_restored.png',
    '04_round_continues.png','04_profile_selected.png','05_records.png','06_rules.png',
    '07_loss.png','08_confirmation.png'
)
foreach ($capture in $expectedCaptures) {
    if (-not (Test-Path -LiteralPath (Join-Path $captures $capture))) {
        throw "Expected UI capture is missing: $capture. Artifacts: $testRoot"
    }
}
$profileFile=Join-Path $data 'profiles.txt'
if (-not (Test-Path -LiteralPath $profileFile)) { throw "Profile file was not created. Artifacts: $testRoot" }
$profileLines=[IO.File]::ReadAllLines($profileFile,[Text.UTF8Encoding]::new($false))
if ($profileLines.Count -lt 4 -or $profileLines[0] -ne 'WORDCHAIN_PROFILES_V1' -or
    -not ($profileLines -contains 'ACTIVE "테스터"') -or
    -not ($profileLines -contains 'PROFILE "테스터"') -or
    -not ($profileLines -contains 'PROFILE "플레이어"')) {
    throw "Profile create/select persistence check failed. Artifacts: $testRoot"
}
$resultFile=Join-Path $captures 'ui_test_results.txt'
if (-not (Test-Path -LiteralPath $resultFile)) { throw "UI result file is missing. Artifacts: $testRoot" }
Get-Content -LiteralPath $resultFile
Write-Output "Verified $($expectedCaptures.Count) UI captures and isolated profile persistence."
Write-Output "Screenshots and isolated records: $testRoot"
