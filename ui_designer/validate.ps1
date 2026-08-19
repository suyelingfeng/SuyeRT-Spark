param([string]$ProjectRoot = $PSScriptRoot)

$ErrorActionPreference = 'Stop'
$requiredFiles = @(
    'project.xml', 'globals.xml',
    'components/spark_header.xml', 'components/spark_menu_card.xml',
    'components/spark_status_row.xml', 'components/spark_action_button.xml',
    'screens/screen_boot.xml', 'screens/screen_home.xml',
    'screens/screen_environment.xml', 'screens/screen_attitude.xml',
    'screens/screen_sensors.xml', 'screens/screen_storage.xml',
    'screens/screen_network.xml', 'screens/screen_system.xml'
)

$failed = $false
foreach ($relativePath in $requiredFiles) {
    $fullPath = Join-Path $ProjectRoot $relativePath
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        Write-Error "Missing required file: $relativePath"
        $failed = $true
        continue
    }
    try {
        [xml](Get-Content -LiteralPath $fullPath -Raw -Encoding UTF8) | Out-Null
        Write-Host "[OK] $relativePath"
    }
    catch {
        Write-Error "Invalid XML in ${relativePath}: $($_.Exception.Message)"
        $failed = $true
    }
}

if (-not $failed) {
    [xml]$globals = Get-Content -LiteralPath (Join-Path $ProjectRoot 'globals.xml') -Raw -Encoding UTF8
    $subjectNames = @($globals.globals.subjects.ChildNodes | ForEach-Object { $_.name })
    $screenFiles = Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'screens') -Filter '*.xml'

    foreach ($screenFile in $screenFiles) {
        [xml]$screenXml = Get-Content -LiteralPath $screenFile.FullName -Raw -Encoding UTF8
        $nodes = $screenXml.SelectNodes('//*')
        foreach ($node in $nodes) {
            foreach ($attribute in @($node.Attributes)) {
                $isSubjectReference = $attribute.Name.StartsWith('bind_') -or
                                      $attribute.Name -eq 'value_subject' -or
                                      ($attribute.Name -eq 'subject' -and
                                       $node.Name.StartsWith('subject_'))
                if ($isSubjectReference -and
                    -not $attribute.Value.StartsWith('$') -and
                    $attribute.Value -notin $subjectNames) {
                    Write-Error "Unknown subject '$($attribute.Value)' in $($screenFile.Name)"
                    $failed = $true
                }
            }
        }
    }
}

if ($failed) { exit 1 }
Write-Host 'LVGL Editor project XML and subject checks passed.'
