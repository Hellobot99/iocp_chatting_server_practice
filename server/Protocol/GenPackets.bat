@echo off
:: 프로토 컴파일러 절대 경로 지정 (본인 폴더 구조에 맞춤)
set PROTOC="..\..\vcpkg\installed\x64-windows\tools\protobuf\protoc.exe"

:: C++ 생성
%PROTOC% -I=. --cpp_out=. Protocol.proto

:: C# 생성
%PROTOC% -I=. --csharp_out=. Protocol.proto

echo Success!
pause