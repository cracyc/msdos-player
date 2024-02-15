echo off

if exist "%ProgramFiles(x86)%" goto is_x64
set path="%ProgramFiles%\Microsoft Visual Studio 9.0\Common7\IDE";%PATH%
set path="%ProgramFiles%\Windows Kits\8.1\bin\x86";%PATH%
goto start

:is_x64
set path="%ProgramFiles(x86)%\Microsoft Visual Studio 9.0\Common7\IDE";%PATH%
set path="%ProgramFiles(x86)%\Windows Kits\8.1\bin\x86";%PATH%

:start
rem rmdir /s /q binary
mkdir binary

devenv.com msdos.vcproj /Rebuild "Release_i86|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\i86_x86
copy Release\msdos.exe binary\i86_x86\.

devenv.com msdos.vcproj /Rebuild "Release_i86|x64"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\i86_x64
copy Release\msdos.exe binary\i86_x64\.

devenv.com msdos.vcproj /Rebuild "Release_v30|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\v30_x86
copy Release\msdos.exe binary\v30_x86\.

devenv.com msdos.vcproj /Rebuild "Release_v30|x64"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\v30_x64
copy Release\msdos.exe binary\v30_x64\.

devenv.com msdos.vcproj /Rebuild "Release_i286|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\i286_x86
copy Release\msdos.exe binary\i286_x86\.

devenv.com msdos.vcproj /Rebuild "Release_i286|x64"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\i286_x64
copy Release\msdos.exe binary\i286_x64\.

devenv.com msdos.vcproj /Rebuild "Release_i486|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\i486_x86
copy Release\msdos.exe binary\i486_x86\.

devenv.com msdos.vcproj /Rebuild "Release_i486|x64"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\i486_x64
copy Release\msdos.exe binary\i486_x64\.

devenv.com msdos.vcproj /Rebuild "Release_pentium4|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\pentium4_x86
copy Release\msdos.exe binary\pentium4_x86\.

devenv.com msdos.vcproj /Rebuild "Release_pentium4|x64"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\pentium4_x64
copy Release\msdos.exe binary\pentium4_x64\.

devenv.com msdos_np21.vcproj /Rebuild "Release|Win32"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\ia32_x86
copy Release\msdos.exe binary\ia32_x86\.

devenv.com msdos_np21.vcproj /Rebuild "Release|x64"
mt.exe /manifest vista.manifest -outputresource:Release\msdos.exe;1
mkdir binary\ia32_x64
copy Release\msdos.exe binary\ia32_x64\.

pause
echo on
