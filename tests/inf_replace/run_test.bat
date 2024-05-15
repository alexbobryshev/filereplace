@echo off

set OUT_FILE=usbdev_test_out.tmp
set TEST_NAME=INF_Replace
set TOOL=filereplace.exe

set CUR_DIR=%0\..
echo [%TEST_NAME% TEST]

del /f /q %CUR_DIR%\%OUT_FILE% >NUL 2>NUL
%TOOL% %CUR_DIR%\usbdev.inx %CUR_DIR%\%OUT_FILE% !(DEVICES_NAMES)=@%CUR_DIR%\devices_names.inx !(DEVICES_VID_PID)=@%CUR_DIR%\devices_vidpid.inx

if NOT %ERRORLEVEL%==0 goto error

fc %CUR_DIR%\%OUT_FILE% %CUR_DIR%\expected_result.txt >NUL
if NOT %ERRORLEVEL%==0 goto error

echo Test PASSED
exit /b 0

:error
echo Test FAILED
exit /b 255
