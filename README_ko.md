# SimpleCQ

[English](README.md) | [中文](README_zh.md) | [한국어](README_ko.md) | [日本語](README_ja.md)

## 개요

SimpleCQ는 컴퓨터 생성 전력(CGF) 행동 학습을 위한 오픈소스 심층 강화학습(DRL) 시뮬레이션 프레임워크입니다.

SimpleCQ는 설정 기반 CGF 시나리오 실행 방식을 유지하면서, 학습에 필요한 실행 인터페이스를 제공합니다. 이 프레임워크는 이기종 모델 DLL을 로드하고, YAML로 정의된 토픽 규칙에 따라 엔티티 출력을 라우팅하며, 재사용 가능한 작업 그래프로 시뮬레이션 프레임을 진행하고, 에이전트 모델을 통해 Python 강화학습 러너와 연결됩니다.

이 README는 특히 엔진과 가속 기능을 강조합니다.

- Taskflow 기반 프레임 스케줄링으로 고정 직렬 모델 루프를 대체합니다.
- 토픽 라우팅으로 모델 출력, 메시지 변환, 대상 입력을 분리합니다.
- 이중 버퍼 토픽 교환으로 결정적인 프레임 의미를 유지합니다.
- 동적 모델 생성과 정리를 프레임 작업 그래프에 통합합니다.
- 비동기 rollout worker와 Gym 스타일 에이전트 인터페이스는 높은 처리량의 DRL 샘플링을 지원할 수 있습니다.
- 교대 self-play runner는 적대적 정책 학습을 지원할 수 있습니다.
- `output_movable`은 모델 소유권 이전이 안전할 때 이동 기반 출력 전달을 허용합니다.

## 런타임 아키텍처

엔진은 각 프레임을 다음 인과 순서로 진행합니다.

1. 각 모델이 `GetOutput`으로 출력을 게시합니다.
2. 토픽 규칙이 필요한 멤버가 있는지 확인합니다.
3. 토픽 데이터가 변환되어 구독자에게 수집됩니다.
4. 토픽 버퍼를 교체하여 모든 모델이 같은 프레임에 정렬된 입력을 읽습니다.
5. 대상 모델은 `SetInput`으로 데이터를 받습니다.
6. 각 모델은 `Tick(dt)`로 상태를 진행합니다.
7. 콜백이 요청한 동적 모델을 생성하고, 파괴된 모델은 만료 창 이후 정리합니다.

이 흐름은 `src/engine/executionengine.hpp`에 구현되어 있습니다. `ExecutionEngine::buildGraph()`는 프레임을 Taskflow DAG로 컴파일하고, `ExecutionEngine::run(times)`는 이 DAG를 재사용해 여러 프레임을 진행하며 FPS를 기록합니다.

## 저장소 구조

| 경로 | 목적 |
| --- | --- |
| `src/engine/` | 런타임 엔진, 콘솔 인터페이스, 작업 그래프 스케줄링, 모델 관리 |
| `src/model.cpp` | `assemble.yml`로 여러 원자 모델을 결합하는 조립 모델 DLL |
| `src/agent.cpp` | Python 학습 상호작용을 위한 socket 기반 에이전트 모델 DLL |
| `src/gym_interface.py` | Python 측 Gym 스타일 통신 도우미 |
| `config/` | 예제 시나리오, 조립 모델 설정, 모델 설명 파일 |
| `scripts/` | 멀티프로세싱, 벡터 환경, self-play 학습 notebook |
| `3rd-party/` | 포함된 Taskflow, yaml-cpp, RapidXML 소스 |
| `agent/` | 선택적 문서화 및 설정 점검 도우미 |
| `doc/README_Detailed.md` | 자세한 엔진 설명과 확장 사용 안내 |

## 빌드

권장 환경:

- Windows 및 MSVC 2022
- `CMakePresets.json` 사용 시 CMake 3.21 이상
- Ninja
- `./vcpkg`에 설치된 vcpkg, 또는 `CMakePresets.json`의 `toolchainFile` 수정

vcpkg로 선언된 의존성은 `mimalloc` 하나입니다. Taskflow와 yaml-cpp는 `3rd-party/`에 포함되어 있습니다.

```powershell
cmake --preset ninja-multi-vcpkg
cmake --build --preset ninja-vcpkg-release
```

주요 산출물은 `bin/Release/`에 생성됩니다.

- `tinycq.exe`: 대화형 시뮬레이션 컨트롤러
- `mymodel.dll`: 조립 모델 래퍼
- `agent.dll`: 학습용 socket 기반 에이전트 모델

GitHub Actions 워크플로 `.github/workflows/compile.yml`은 `windows-latest`에서 동일한 CMake preset을 사용합니다.

## 빠른 시작

예제를 실행하기 전에 선택한 YAML 파일의 `dll_path` 값을 수정해야 합니다. `config/`의 일부 파일은 원래 실험 환경의 절대 경로를 보존하고 있습니다.

```powershell
.\bin\Release\tinycq.exe
```

콘솔 내부:

```text
>>> load config\navigation.yml
>>> set dt 100
>>> set drawrate 100
>>> r 1000
>>> model
```

주요 명령:

| 명령 | 의미 |
| --- | --- |
| `load <file>` 또는 `l <file>` | 시나리오 YAML 로드 |
| `run <frames>` 또는 `r <frames>` | 시뮬레이션 프레임 진행 |
| `cfg` | 런타임 설정 목록 표시 |
| `get <key>` | 런타임 설정 조회 |
| `set <key> <value>` | 런타임 설정 변경 |
| `print` 또는 `p` | 현재 토픽 버퍼 출력 |
| `model` | 정적 및 동적 모델 목록 표시 |

## 자세한 엔진 설명

엔진 메커니즘, 토픽 스케줄링, 조립 모델 재시작, 시나리오 라우팅 규칙, 프로파일링, 시각화, 소스 구조에 대한 자세한 설명은 [doc/README_Detailed.md](doc/README_Detailed.md)를 참고하십시오.

## 시나리오 설정

시나리오 YAML 파일은 실행 가능한 시뮬레이션 그래프를 설명합니다.

- `model_types`: 모델 타입 이름, DLL 경로, `output_movable` 설정.
- `models`: `model_type`, `side_id`, `id`, XML 형식 `init_value`를 가진 모델 인스턴스.
- `topics`: 게시-구독 규칙. 게시자, 필수 출력 멤버, 구독자, 이름 변환 규칙을 포함합니다.

협동 UAV 정찰 예제는 `config/navigation.yml`에, 2-vs-2 적대적 self-play 예제는 `config/scene_planebattle2v2_self_learning.yml`에 대응합니다.

## 조립 모델 설정

`mymodel.dll`은 DLL 디렉터리에서 `assemble.yml`을 로드합니다. 조립 모델은 여러 원자 모델을 하나의 CGF 모델로 노출합니다. 설정 파일은 다음 항목을 포함합니다.

- `config`: 선택적 프로파일링 경로, 재시작 키, 진영 필터, 로그 레벨.
- `models`: 원자 모델 DLL과 논리 이름.
- `init_convert`: 루트 초기화 데이터를 하위 모델로 라우팅합니다.
- `input_convert`: 루트 입력 데이터를 하위 모델로 라우팅합니다.
- `output_convert`: 하위 모델 출력을 루트 또는 다른 하위 모델로 라우팅합니다.

`restart_key`가 설정되어 있고 `SetInput` 데이터에 해당 키가 있으면, 조립 모델은 다음 `GetOutput` 전에 하위 모델을 다시 생성하고 초기화합니다.

## DRL 인터페이스

`src/agent.cpp`의 에이전트 모델은 `127.0.0.1:<port>`에 연결하고 직렬화된 `CSValueMap` 데이터를 Python과 교환합니다. 이 모델은 시뮬레이터 측 `Init`, `SetInput`, `GetOutput`, `Tick`을 학습 측 reset-step 루프로 감쌉니다.

- `reset`은 시나리오를 초기화하고 정렬된 관측을 반환합니다.
- `step(action)`은 명령을 토픽 데이터에 쓰고 프레임을 진행한 뒤 관측, 보상, 종료 플래그를 반환합니다.
- 학습 스크립트는 다중 환경과 비동기 rollout worker를 지원하여 샘플 처리량을 높일 수 있습니다.

## 예제

다음 예제는 현재 지원할 수 있는 워크플로를 보여줍니다.

- 협동 UAV 정찰: `config/navigation.yml`, `scripts/Train_vecenv.ipynb`.
- 비동기 및 멀티프로세스 rollout 가속을 지원할 수 있습니다: `scripts/Train_Multiprocess.ipynb`.
- 2-vs-2 교대 self-play를 지원할 수 있습니다: `config/scene_planebattle2v2_self_learning.yml`, `scripts/Train2v2_self_learning.ipynb`.

## 라이선스

이 프로젝트는 MIT License로 배포됩니다. 자세한 내용은 `LICENSE`를 참조하십시오.
