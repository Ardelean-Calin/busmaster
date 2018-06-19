@echo off

:DOTNET_SEARCH
set DOTNET=%SystemRoot%\Microsoft.NET\Framework\v4.0.30319
if exist "%DOTNET%\MSBuild.exe" goto DOTNET_FOUND

set DOTNET=%SystemRoot%\Microsoft.NET\Framework\v3.5
if exist "%DOTNET%\MSBuild.exe" goto DOTNET_FOUND

:DOTNET_NOT_FOUND
echo .NET Framework not found. Build failed!
goto END

:DOTNET_FOUND
echo Using MSBuild found in %DOTNET%

:CMAKE_SEARCH
set CMAKE=C:\Users\arc4clj\scoop\shims
if exist "%CMAKE%\cmake.exe" goto CMAKE_FOUND

:CMAKE_NOT_FOUND
echo CMake not found. Build failed!
goto END

:CMAKE_FOUND
echo Using CMake found in %CMAKE%

:BUILD
set PATH=%CMAKE%;%DOTNET%;%PATH%
REM define your build directory here:
mkdir build
cd build

REM define your compiler/IDE here:
REM cmake -G "Visual Studio 10 2010" ..
REM cmake -G "Visual Studio 11 2012" -T "v110_xp" ..
cmake -G "Visual Studio 12 2013" -T "v120"

REM automatically compile solution:
MSBuild "C:\Users\arc4clj\Documents\Projects\busmaster\Sources\BUSMASTER\BUSMASTER.sln" /property:Configuration=Release

:END
REM pause
exit 0
