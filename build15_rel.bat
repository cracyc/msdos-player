echo off

if exist "%ProgramFiles(x86)%" goto is_x64
set path="%ProgramFiles%\Microsoft Visual Studio\2017\WDExpress\MSBuild\15.0\Bin";%PATH%
goto start

:is_x64
set path="%ProgramFiles(x86)%\Microsoft Visual Studio\2017\WDExpress\MSBuild\15.0\Bin";%PATH%

:start
rem rmdir /s /q binary
mkdir binary

rmdir /s /q Release

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i86;Platform="Win32"
mkdir binary\i86_x86
copy Release\msdos.exe binary\i86_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i86;Platform="x64"
mkdir binary\i86_x64
copy Release\msdos.exe binary\i86_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_v30;Platform="Win32"
mkdir binary\v30_x86
copy Release\msdos.exe binary\v30_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_v30;Platform="x64"
mkdir binary\v30_x64
copy Release\msdos.exe binary\v30_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i286;Platform="Win32"
mkdir binary\i286_x86
copy Release\msdos.exe binary\i286_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i286;Platform="x64"
mkdir binary\i286_x64
copy Release\msdos.exe binary\i286_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i486;Platform="Win32"
mkdir binary\i486_x86
copy Release\msdos.exe binary\i486_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i486;Platform="x64"
mkdir binary\i486_x64
copy Release\msdos.exe binary\i486_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium4;Platform="Win32"
mkdir binary\pentium4_x86
copy Release\msdos.exe binary\pentium4_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium4;Platform="x64"
mkdir binary\pentium4_x64
copy Release\msdos.exe binary\pentium4_x64\.

rmdir /s /q Release

msbuild.exe msdos_np21.vcxproj /t:clean;rebuild /p:Configuration=Release;Platform="Win32"
mkdir binary\ia32_x86
copy Release\msdos.exe binary\ia32_x86\.

msbuild.exe msdos_np21.vcxproj /t:clean;rebuild /p:Configuration=Release;Platform="x64"
mkdir binary\ia32_x64
copy Release\msdos.exe binary\ia32_x64\.

pause
echo on
exit /b
