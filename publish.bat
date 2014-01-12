@echo off
cd C:\Far\Dev
set TARGET=C:\Far\Plugins\BlockPro\Src.zip
set WWW=D:\Project\WWW\WWW
set WWWTARGET=%WWW%\download\blockpro.zip
if exist %TARGET% del %TARGET%
zip %TARGET% -@ <C:\Far\Dev\Plugins\BlockPro\publish.lst >nul
cd C:\Far\Plugins\BlockPro

if not exist %WWWTARGET% exit
del %WWWTARGET%
zip -R -o %WWWTARGET% *.* >nul
copy Doc\readme.txt %WWW%\download\blockpro.txt>nul
del %TARGET%
ls -l %WWWTARGET%
