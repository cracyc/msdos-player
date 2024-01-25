echo off

if exist "%ProgramFiles(x86)%" goto is_x64
set path="%ProgramFiles%\Microsoft Visual Studio 12.0\Common7\IDE";%PATH%
goto start

:is_x64
set path="%ProgramFiles(x86)%\Microsoft Visual Studio 12.0\Common7\IDE";%PATH%

:start
rem rmdir /s /q binary
mkdir binary
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_i86|Win32"
mkdir binary\i86_x86
copy Release\msdos.exe binary\i86_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_i86|x64"
mkdir binary\i86_x64
copy Release\msdos.exe binary\i86_x64\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_i186|Win32"
mkdir binary\i186_x86
copy Release\msdos.exe binary\i186_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_i186|x64"
mkdir binary\i186_x64
copy Release\msdos.exe binary\i186_x64\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_v30|Win32"
mkdir binary\v30_x86
copy Release\msdos.exe binary\v30_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_v30|x64"
mkdir binary\v30_x64
copy Release\msdos.exe binary\v30_x64\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_i286|Win32"
mkdir binary\i286_x86
copy Release\msdos.exe binary\i286_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_i286|x64"
mkdir binary\i286_x64
copy Release\msdos.exe binary\i286_x64\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_i386|Win32"
mkdir binary\i386_x86
copy Release\msdos.exe binary\i386_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_i386|x64"
mkdir binary\i386_x64
copy Release\msdos.exe binary\i386_x64\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_i486|Win32"
mkdir binary\i486_x86
copy Release\msdos.exe binary\i486_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_i486|x64"
mkdir binary\i486_x64
copy Release\msdos.exe binary\i486_x64\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_pentium|Win32"
mkdir binary\pentium_x86
copy Release\msdos.exe binary\pentium_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_pentium|x64"
mkdir binary\pentium_x64
copy Release\msdos.exe binary\pentium_x64\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_mediagx|Win32"
mkdir binary\mediagx_x86
copy Release\msdos.exe binary\mediagx_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_mediagx|x64"
mkdir binary\mediagx_x64
copy Release\msdos.exe binary\mediagx_x64\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_pentium_pro|Win32"
mkdir binary\pentium_pro_x86
copy Release\msdos.exe binary\pentium_pro_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_pentium_pro|x64"
mkdir binary\pentium_pro_x64
copy Release\msdos.exe binary\pentium_pro_x64\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_pentium_mmx|Win32"
mkdir binary\pentium_mmx_x86
copy Release\msdos.exe binary\pentium_mmx_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_pentium_mmx|x64"
mkdir binary\pentium_mmx_x64
copy Release\msdos.exe binary\pentium_mmx_x64\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_pentium2|Win32"
mkdir binary\pentium2_x86
copy Release\msdos.exe binary\pentium2_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_pentium2|x64"
mkdir binary\pentium2_x64
copy Release\msdos.exe binary\pentium2_x64\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_pentium3|Win32"
mkdir binary\pentium3_x86
copy Release\msdos.exe binary\pentium3_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_pentium3|x64"
mkdir binary\pentium3_x64
copy Release\msdos.exe binary\pentium3_x64\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_pentium4|Win32"
mkdir binary\pentium4_x86
copy Release\msdos.exe binary\pentium4_x86\.
call :clean

devenv.com msdos.vcxproj /Rebuild "Release_pentium4|x64"
mkdir binary\pentium4_x64
copy Release\msdos.exe binary\pentium4_x64\.
call :clean

devenv.com msdos_np21.vcxproj /Rebuild "Release|Win32"
mkdir binary\ia32_x86
copy Release\msdos.exe binary\ia32_x86\.
call :clean

devenv.com msdos_np21.vcxproj /Rebuild "Release|x64"
mkdir binary\ia32_x64
copy Release\msdos.exe binary\ia32_x64\.
call :clean

pause
echo on
exit /b

:clean
if not exist Release goto clean_x64
ren Release Release_tmp
rmdir /s /q Release_tmp

:clean_x64
if not exist x64 goto clean_exit
ren x64 x64_tmp
rmdir /s /q x64_tmp

:clean_exit
exit /b

