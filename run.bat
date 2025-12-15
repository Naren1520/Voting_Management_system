@echo off
REM ============================================================================
REM Voting System - Start Both Backend and Frontend
REM ============================================================================
REM This script runs both the backend server (Python) and frontend together
REM Usage: run.bat
REM ============================================================================

setlocal enabledelayedexpansion

cd /d "%~dp0"
set PROJECT_ROOT=%cd%
set BACKEND_DIR=%PROJECT_ROOT%\backend
set FRONTEND_DIR=%PROJECT_ROOT%\frontend

REM ============================================================================
REM Step 1: Start Backend Server (C++)
REM ============================================================================
echo Step 1: Starting Backend Server...
echo.

pushd "%BACKEND_DIR%"
start "Voting System Backend" /MIN voting_server.exe
popd
timeout /t 3 /nobreak > nul

echo [OK] Backend server started on http://localhost:8080
echo.

REM ============================================================================
REM Step 2: Start Frontend Server
REM ============================================================================
echo Step 2: Starting Frontend Server...
echo.

cd /d "%FRONTEND_DIR%"
start "Voting System Frontend" /MIN python -m http.server 3000
timeout /t 2 /nobreak > nul

echo [OK] Frontend server started on http://localhost:3000
echo.

REM ============================================================================
REM Step 3: Open Browser
REM ============================================================================
echo ==========================================
echo [SUCCESS] Both Servers Running!
echo ==========================================
echo.
echo  Frontend: http://localhost:3000
echo  Backend:  http://localhost:8080
echo.
echo Opening browser...
echo.

start http://localhost:3000/loader.html

echo ==========================================
echo  Ready to Vote! [OK]
echo ==========================================
echo.
echo Admin Password: admin123
echo.
echo NOTE: Two windows opened:
echo   - "Voting System Backend" (Backend API)
echo   - "Voting System Frontend" (Frontend Server)
echo.
echo To stop the servers, close both windows or
echo press Ctrl+C in each window.
echo ==========================================
echo.

timeout /t 3 /nobreak > nul
