set PYTHON_HOME=%CD%\BuildTools\Python27
set PYTHON_FREEZER_PATH=%CD%\BuildTools\Python27\Scripts
@REM NASM_PREFIX最後必須加上 "\"
set NASM_PREFIX=%CD%\BuildTools\NASM\
set IASL_PREFIX=%CD%\BuildTools\ASL\

set CYGWIN_HOME=%CD%\BuildTools\cygwin64

set EDK_TOOLS_PATH=%CD%\BaseTools

cd /D BaseTools
call toolsetup.bat Rebuild
cd /D ..

echo Run edksetup.bat
@REM call edksetup.bat --nt32
call edksetup.bat
@REM call build cleanall
call build 
@REM call build -q -s
@REM call build -DDEBUG_ON_SERIAL_PORT=TRUE -DSOURCE_DEBUG_ENABLE
@REM call build run
del /F z:\MySpdRead.efi 
del /F f:\MySpdRead.efi 

copy E:\EDK2\UDK2018\Build\AppPkg\RELEASE_VS2015x86\X64\MySpdRead.efi  z:\
copy E:\EDK2\UDK2018\Build\AppPkg\RELEASE_VS2015x86\X64\MySpdRead.efi  f:\
