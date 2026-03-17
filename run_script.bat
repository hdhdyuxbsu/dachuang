@echo off
cd /d E:\ESP32\dachuang
echo === CJK 12x12 Bitmap Generator ===
echo.

REM Check Python
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found. Install from https://python.org
    pause
    exit /b 1
)

REM Run generator  (add --preview to see ASCII art of every character)
python gen_cjk_12x12.py --preview

echo.
echo Done. char_bitmap.h has been updated.
pause
