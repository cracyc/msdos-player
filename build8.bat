echo off
if exist "%ProgramFiles(x86)%" goto is_x64
set path="%ProgramFiles%\Microsoft Visual Studio 9.0\Common7\IDE";%PATH%
goto start
:is_x64
set path="%ProgramFiles(x86)%\Microsoft Visual Studio 9.0\Common7\IDE";%PATH%
:start
mkdir binary

devenv.com msdos.vcproj /Rebuild "Release_i86|Win32"
mkdir binary\i86_x86
copy Release\msdos.exe binary\i86_x86\.

devenv.com msdos.vcproj /Rebuild "Release_i86|x64"
mkdir binary\i86_x64
copy Release\msdos.exe binary\i86_x64\.

devenv.com msdos.vcproj /Rebuild "Release_i286|Win32"
mkdir binary\i286_x86
copy Release\msdos.exe binary\i286_x86\.

devenv.com msdos.vcproj /Rebuild "Release_i286|x64"
mkdir binary\i286_x64
copy Release\msdos.exe binary\i286_x64\.

devenv.com msdos.vcproj /Rebuild "Release_i386|Win32"
mkdir binary\i386_x86
copy Release\msdos.exe binary\i386_x86\.

devenv.com msdos.vcproj /Rebuild "Release_i386|x64"
mkdir binary\i386_x64
copy Release\msdos.exe binary\i386_x64\.

devenv.com msdos.vcproj /Rebuild "Release_i486|Win32"
mkdir binary\i486_x86
copy Release\msdos.exe binary\i486_x86\.

devenv.com msdos.vcproj /Rebuild "Release_i486|x64"
mkdir binary\i486_x64
copy Release\msdos.exe binary\i486_x64\.

pause
echo on
