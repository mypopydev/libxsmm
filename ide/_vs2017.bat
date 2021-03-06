@ECHO OFF
SETLOCAL

cd ..
bash -c "make realclean ; make headers sources"
cd ide

SET LIBXSMMROOT=%~d0%~p0..
SETX PATH "%LIBXSMMROOT%\lib\intel64;%LIBXSMMROOT%\lib\ia32;%PATH%"

IF EXIST C:\cygwin64\usr\include\eigen3\Eigen\Dense (
  SET EIGENROOT=C:\cygwin64\usr\include\eigen3
)
IF EXIST C:\blaze\blaze\Blaze.h (
  SET BLAZEROOT=C:blaze
)

CALL %~d0"%~p0"_vs.bat vs2017
IF NOT "%VS_IDE%" == "" (
  START "" "%VS_IDE%" "%~d0%~p0_vs2017.sln"
) ELSE (
  START %~d0"%~p0"_vs2017.sln
)

ENDLOCAL