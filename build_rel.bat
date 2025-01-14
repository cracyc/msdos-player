echo off

rem rmdir /s /q binary
mkdir binary

setlocal

if exist "%ProgramFiles(x86)%" goto is_x64_v9
set path="%ProgramFiles%\Microsoft Visual Studio 9.0\Common7\IDE";%PATH%
set path="%ProgramFiles%\Windows Kits\8.1\bin\x86";%PATH%
goto start_v9

:is_x64_v9
set path="%ProgramFiles(x86)%\Microsoft Visual Studio 9.0\Common7\IDE";%PATH%
set path="%ProgramFiles(x86)%\Windows Kits\8.1\bin\x86";%PATH%

:start_v9
rmdir /s /q Release
rmdir /s /q x64

devenv.com msdos.vcproj /Rebuild "Release_i86|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\i86_x86
copy Release\msdos.exe binary\i86_x86\.

devenv.com msdos.vcproj /Rebuild "Release_v30|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\v30_x86
copy Release\msdos.exe binary\v30_x86\.

devenv.com msdos.vcproj /Rebuild "Release_i286|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\i286_x86
copy Release\msdos.exe binary\i286_x86\.

devenv.com msdos.vcproj /Rebuild "Release_i386|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\i386_x86
copy Release\msdos.exe binary\i386_x86\.

devenv.com msdos.vcproj /Rebuild "Release_i486|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\i486_x86
copy Release\msdos.exe binary\i486_x86\.

devenv.com msdos.vcproj /Rebuild "Release_pentium4|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\pentium4_x86
copy Release\msdos.exe binary\pentium4_x86\.

devenv.com msdos_np21.vcproj /Rebuild "Release|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\ia32_x86
copy Release\msdos.exe binary\ia32_x86\.

devenv.com ntvdm.vcproj /Rebuild "Release|Win32"
copy Release\ntvdm.exe binary\i386_x86\.
copy Release\ntvdm.exe binary\i486_x86\.
copy Release\ntvdm.exe binary\pentium4_x86\.
copy Release\ntvdm.exe binary\ia32_x86\.

endlocal

setlocal

if exist "%ProgramFiles(x86)%" goto is_x64_v15
set path="%ProgramFiles%\Microsoft Visual Studio\2017\WDExpress\MSBuild\15.0\Bin";%PATH%
goto start_v15

:is_x64_v15
set path="%ProgramFiles(x86)%\Microsoft Visual Studio\2017\WDExpress\MSBuild\15.0\Bin";%PATH%

:start_v15
rmdir /s /q Release
rmdir /s /q x64

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i86;Platform="x64"
mkdir binary\i86_x64
copy Release\msdos.exe binary\i86_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_v30;Platform="x64"
mkdir binary\v30_x64
copy Release\msdos.exe binary\v30_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i286;Platform="x64"
mkdir binary\i286_x64
copy Release\msdos.exe binary\i286_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i386;Platform="x64"
mkdir binary\i386_x64
copy Release\msdos.exe binary\i386_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i486;Platform="x64"
mkdir binary\i486_x64
copy Release\msdos.exe binary\i486_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium4;Platform="x64"
mkdir binary\pentium4_x64
copy Release\msdos.exe binary\pentium4_x64\.

rmdir /s /q Release
rmdir /s /q x64

msbuild.exe msdos_np21.vcxproj /t:clean;rebuild /p:Configuration=Release;Platform="x64"
mkdir binary\ia32_x64
copy Release\msdos.exe binary\ia32_x64\.

endlocal

rmdir /s /q Release
rmdir /s /q x64

pause
echo on
