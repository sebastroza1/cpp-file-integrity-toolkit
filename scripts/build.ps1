$ErrorActionPreference = 'Stop'
$cmake = if ($env:CMAKE_EXE) { $env:CMAKE_EXE } else { 'cmake' }
& $cmake -S . -B build
& $cmake --build build --config Release
& $cmake --build build --target test --config Release

