echo off

rem rmdir /s /q binary
mkdir binary

setlocal

if exist "%ProgramFiles(x86)%" goto is_x64
set path="%ProgramFiles%\Microsoft Visual Studio\Common\MSDev98\Bin";%PATH%
goto start

:is_x64
set path="%ProgramFiles(x86)%\Microsoft Visual Studio\Common\MSDev98\Bin";%PATH%
:start
rmdir /s /q Release

msdev.com msdos.dsw /Make "msdos - Win32 Release_i86" /Rebuild
mkdir binary\i86_vc6
copy Release\msdos.exe binary\i86_vc6\.

msdev.com msdos.dsw /Make "msdos - Win32 Release_i186" /Rebuild
mkdir binary\i186_vc6
copy Release\msdos.exe binary\i186_vc6\.

msdev.com msdos.dsw /Make "msdos - Win32 Release_v30" /Rebuild
mkdir binary\v30_vc6
copy Release\msdos.exe binary\v30_vc6\.

msdev.com msdos.dsw /Make "msdos - Win32 Release_i286" /Rebuild
mkdir binary\i286_vc6
copy Release\msdos.exe binary\i286_vc6\.

msdev.com msdos.dsw /Make "msdos - Win32 Release_i386" /Rebuild
mkdir binary\i386_vc6
copy Release\msdos.exe binary\i386_vc6\.

msdev.com msdos.dsw /Make "msdos - Win32 Release_i486" /Rebuild
mkdir binary\i486_vc6
copy Release\msdos.exe binary\i486_vc6\.

msdev.com msdos.dsw /Make "msdos - Win32 Release_pentium" /Rebuild
mkdir binary\pentium_vc6
copy Release\msdos.exe binary\pentium_vc6\.

msdev.com msdos.dsw /Make "msdos - Win32 Release_mediagx" /Rebuild
mkdir binary\mediagx_vc6
copy Release\msdos.exe binary\mediagx_vc6\.

msdev.com msdos.dsw /Make "msdos - Win32 Release_pentium_pro" /Rebuild
mkdir binary\pentium_pro_vc6
copy Release\msdos.exe binary\pentium_pro_vc6\.

msdev.com msdos.dsw /Make "msdos - Win32 Release_pentium_mmx" /Rebuild
mkdir binary\pentium_mmx_vc6
copy Release\msdos.exe binary\pentium_mmx_vc6\.

msdev.com msdos.dsw /Make "msdos - Win32 Release_pentium2" /Rebuild
mkdir binary\pentium2_vc6
copy Release\msdos.exe binary\pentium2_vc6\.

msdev.com msdos.dsw /Make "msdos - Win32 Release_pentium3" /Rebuild
mkdir binary\pentium3_vc6
copy Release\msdos.exe binary\pentium3_vc6\.

msdev.com msdos.dsw /Make "msdos - Win32 Release_pentium4" /Rebuild
mkdir binary\pentium4_vc6
copy Release\msdos.exe binary\pentium4_vc6\.

endlocal

rmdir /s /q Release

pause
echo on
