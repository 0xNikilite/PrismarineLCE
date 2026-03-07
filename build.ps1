param([switch]$Clean, [switch]$Package)

if ($Clean -or !(Test-Path build/CMakeCache.txt)) {
    Remove-Item -Recurse -Force build -ErrorAction SilentlyContinue
    mkdir build | Out-Null
    cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DVCPKG_APPLOCAL_DEPS=OFF
}

cmake --build build --config Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "`n BUILD FAILED" -ForegroundColor Red
    exit 1
}

Write-Host "`n BUILD OK" -ForegroundColor Green

if ($Package) {
    $out = "Prismarine-win64"
    Remove-Item -Recurse -Force $out -ErrorAction SilentlyContinue
    Remove-Item -Force "Prismarine-win64.zip" -ErrorAction SilentlyContinue
    mkdir $out | Out-Null
    mkdir "$out/assets" | Out-Null
    Copy-Item "build/Release/Prismarine.exe" "$out/"
    Copy-Item "assets/Minecraft.ttf" "$out/assets/"
    Copy-Item "assets/dirt.png" "$out/assets/"
    Compress-Archive -Path "$out/*" -DestinationPath "Prismarine-win64.zip" -Force
    Remove-Item -Recurse -Force $out
    Write-Host " Prismarine-win64.zip created" -ForegroundColor Green
}

Write-Host " Run: .\build\Release\Prismarine.exe" -ForegroundColor Cyan