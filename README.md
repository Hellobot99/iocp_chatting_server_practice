# IOCP Chatting Server Practice

Windows IOCP(Input/Output Completion Port)를 활용한 비동기 소켓 채팅 서버와 Unity 클라이언트 구현 실습 프로젝트입니다.

## 프로젝트 개요

이 프로젝트는 고성능 네트워크 서버 모델인 IOCP를 학습하고 적용하여 실시간 채팅 서버를 구축하는 것을 목표로 합니다. 서버는 C++로 작성되었으며, 클라이언트는 Unity(C#)를 사용하여 채팅 UI 및 시각적 요소를 구현했습니다. Mysql과 Redis를 통한 데이터 영속성을 지원합니다.

## 기술 스택

### Server

* **Language**: C++
* **Network Model**: Windows IOCP (Asynchronous I/O)
* **Database/Persistence**: Mysql, Redis (hiredis)
* **IDE**: Visual Studio 2022

### Client

* **Engine**: Unity (C#)
* **Test Client**: C# Console Application (DummyClient)

## 주요 기능

1. **IOCP 네트워크 엔진**
* IOCPWorker를 통한 효율적인 스레드 풀 및 비동기 입출력 처리
* 확장 가능한 세션 및 커넥션 관리


2. **패킷 처리**
* 자체 정의 패킷 구조를 통한 데이터 통신
* 직렬화 및 역직렬화 처리


3. **게임 로직 및 룸 시스템**
* Lobby(로비)와 Room(방) 구조 구현
* 방 생성, 입장, 퇴장 및 방 내부 채팅 기능
* RoomManager를 통한 방 생명주기 관리


4. **데이터 영속성**
* Redis를 연동하여 유저 정보 또는 채팅 로그 등의 데이터 저장/로드 구현 (Persistence 클래스)
* Mysql 데이터베이스 연동



## 프로젝트 구조

* **server/**: C++ 서버 소스 코드
* `main.cpp`: 서버 진입점
* `Server.cpp/h`: 리스너 및 워커 스레드 관리
* `IOCPWorker.cpp/h`: IOCP 워커 구현
* `ClientSession.cpp/h`: 클라이언트 세션 관리
* `GameLogic.cpp/h`, `RoomManager.cpp/h`: 채팅 및 방 관리 로직


* **client/**: Unity 클라이언트 프로젝트
* `Assets/Script/`: 네트워크 매니저, 패킷 핸들러, UI 스크립트
* `test_client/`: 스트레스 테스트 및 디버깅용 더미 클라이언트 (Console)



## 설치 및 실행 방법

### 사전 요구 사항

* Visual Studio (C++ 데스크톱 개발 워크로드)
* Unity Hub 및 Unity Editor
* Redis Server, Mysql Server (로컬 또는 원격)

### 서버 빌드 및 실행

1. `server/iocp_chatting_server_practice.sln` 파일을 Visual Studio로 엽니다.
2. Redis 및 Mysql 서버가 실행 중인지 확인합니다.
3. 솔루션을 빌드(Build)하고 실행합니다.

### 클라이언트 실행

1. Unity Hub를 통해 `client/iocp_chatting_server_practice` 폴더를 프로젝트로 엽니다.
2. `Assets/Scenes/SampleScene.unity` (또는 메인 씬)을 엽니다.
3. Unity 에디터에서 Play 버튼을 눌러 실행합니다.
4. 별도로 `client/test_client` 솔루션을 빌드하여 콘솔 기반의 다중 접속 테스트를 진행할 수 있습니다.

## 라이선스

이 프로젝트는 `LICENSE` 파일에 명시된 라이선스를 따릅니다. 자세한 내용은 해당 파일을 참고하십시오.