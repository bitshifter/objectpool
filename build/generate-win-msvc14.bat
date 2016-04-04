@echo off

set DIR32=win32-msvc14
set DIR64=win64-msvc14

md %DIR32%
md %DIR64%

cd %DIR32%
cmake -G "Visual Studio 14" ..\..
cd ..

cd %DIR64%
cmake -G "Visual Studio 14 Win64" ..\..
cd ..

