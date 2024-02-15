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

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i186;Platform="Win32"
mkdir binary\i186_x86
copy Release\msdos.exe binary\i186_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i186;Platform="x64"
mkdir binary\i186_x64
copy Release\msdos.exe binary\i186_x64\.

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

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i386;Platform="Win32"
mkdir binary\i386_x86
copy Release\msdos.exe binary\i386_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i386;Platform="x64"
mkdir binary\i386_x64
copy Release\msdos.exe binary\i386_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i486;Platform="Win32"
mkdir binary\i486_x86
copy Release\msdos.exe binary\i486_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_i486;Platform="x64"
mkdir binary\i486_x64
copy Release\msdos.exe binary\i486_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium;Platform="Win32"
mkdir binary\pentium_x86
copy Release\msdos.exe binary\pentium_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium;Platform="x64"
mkdir binary\pentium_x64
copy Release\msdos.exe binary\pentium_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_mediagx;Platform="Win32"
mkdir binary\mediagx_x86
copy Release\msdos.exe binary\mediagx_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_mediagx;Platform="x64"
mkdir binary\mediagx_x64
copy Release\msdos.exe binary\mediagx_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium_pro;Platform="Win32"
mkdir binary\pentium_pro_x86
copy Release\msdos.exe binary\pentium_pro_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium_pro;Platform="x64"
mkdir binary\pentium_pro_x64
copy Release\msdos.exe binary\pentium_pro_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium_mmx;Platform="Win32"
mkdir binary\pentium_mmx_x86
copy Release\msdos.exe binary\pentium_mmx_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium_mmx;Platform="x64"
mkdir binary\pentium_mmx_x64
copy Release\msdos.exe binary\pentium_mmx_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium2;Platform="Win32"
mkdir binary\pentium2_x86
copy Release\msdos.exe binary\pentium2_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium2;Platform="x64"
mkdir binary\pentium2_x64
copy Release\msdos.exe binary\pentium2_x64\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium3;Platform="Win32"
mkdir binary\pentium3_x86
copy Release\msdos.exe binary\pentium3_x86\.

msbuild.exe msdos.vcxproj /t:clean;rebuild /p:Configuration=Release_pentium3;Platform="x64"
mkdir binary\pentium3_x64
copy Release\msdos.exe binary\pentium3_x64\.

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
