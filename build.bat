@echo off
setlocal

set WARN_FLAGS=/Wall /wd5045 /wd4701 /wd4820 /wd4996 /WX
if "%1" == "release" (
    set BASE_FLAGS=/O2
) else (
    set BASE_FLAGS=/Zi
)
set LINK_FLAGS=

cl %WARN_FLAGS% %BASE_FLAGS% src\mod.c %LINK_FLAGS% /Fe:dat_mod.exe
cl %WARN_FLAGS% %BASE_FLAGS% src\hmex.c %LINK_FLAGS% /Fe:hmex.exe

endlocal
