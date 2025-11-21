@echo off
set PROTOC=protoc.exe

echo Using Local Protoc: %PROTOC%

:: C++ 파일 생성
%PROTOC% -I=. --cpp_out=. Protocol.proto

:: C# 파일 생성
%PROTOC% -I=. --csharp_out=. Protocol.proto

echo.
echo ---------------------------------------
echo Protocol Compile Finished! (Manual Mode)
echo ---------------------------------------
pause