# PowerShell build script for BlackJack-OS
# Usage: .\build.ps1 [clean|all|server|client]

param(
    [Parameter(Position=0)]
    [string]$Target = "all"
)

$CC = "gcc"
$CFLAGS = "-std=c11 -Wall -Wextra -pthread -g"
$LDFLAGS = ""

function Build-Server {
    Write-Host "Building server..." -ForegroundColor Green
    & $CC -std=c11 -Wall -Wextra -pthread -g -o server.exe server.c
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Server built successfully!" -ForegroundColor Green
    } else {
        Write-Host "Server build failed!" -ForegroundColor Red
        exit 1
    }
}

function Build-Client {
    Write-Host "Building client..." -ForegroundColor Green
    & $CC -std=c11 -Wall -Wextra -pthread -g -o client.exe client.c
    if ($LASTEXITCODE -eq 0) {
        Write-Host "Client built successfully!" -ForegroundColor Green
    } else {
        Write-Host "Client build failed!" -ForegroundColor Red
        exit 1
    }
}

function Clean-Build {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Yellow
    Remove-Item -Path server.exe,client.exe,*.o -ErrorAction SilentlyContinue
    Write-Host "Clean complete!" -ForegroundColor Green
}

switch ($Target.ToLower()) {
    "clean" {
        Clean-Build
    }
    "server" {
        Build-Server
    }
    "client" {
        Build-Client
    }
    "all" {
        Clean-Build
        Build-Server
        Build-Client
        Write-Host "`nBuild complete! You can now run:" -ForegroundColor Cyan
        Write-Host "  .\server.exe 12345" -ForegroundColor White
        Write-Host "  .\client.exe 127.0.0.1 12345 YourName" -ForegroundColor White
    }
    default {
        Write-Host "Unknown target: $Target" -ForegroundColor Red
        Write-Host "Usage: .\build.ps1 [clean|all|server|client]" -ForegroundColor Yellow
        exit 1
    }
}

