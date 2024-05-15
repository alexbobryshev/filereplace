@echo off

set TEST_NAME=IFNOTSET condition
set TOOL=filereplace.exe

set CUR_DIR=%0\..
echo [%TEST_NAME% TEST]

set OUT_FILE=test_out1.tmp

del /f /q %CUR_DIR%\%OUT_FILE% >NUL 2>NUL
%TOOL% %CUR_DIR%\input.txt %CUR_DIR%\%OUT_FILE% TESTCOND=1
if NOT %ERRORLEVEL%==0 goto error

fc %CUR_DIR%\%OUT_FILE% %CUR_DIR%\expected_result1.txt >NUL
if NOT %ERRORLEVEL%==0 goto error

set OUT_FILE=test_out2.tmp

del /f /q %CUR_DIR%\%OUT_FILE% >NUL 2>NUL
%TOOL% %CUR_DIR%\input.txt %CUR_DIR%\%OUT_FILE%
if NOT %ERRORLEVEL%==0 goto error

fc %CUR_DIR%\%OUT_FILE% %CUR_DIR%\expected_result2.txt >NUL
if NOT %ERRORLEVEL%==0 goto error

echo Test PASSED
exit /b 0

:error
echo Test FAILED
exit /b 255
