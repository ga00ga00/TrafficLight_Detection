# TrafficLight_v2 실행 가이드

`TrafficLight_v2.cpp`는 Ubuntu/Linux 환경에서 웹캠 영상을 받아 빨간불(`R`), 주황불(`O`), 초록불(`G`)을 인식하고 최종 주행 판단을 `GO(1)` 또는 `STOP(0)`으로 출력하는 OpenCV C++ 프로그램입니다.

> 이 코드는 Linux의 V4L2 카메라 백엔드(`cv::CAP_V4L2`)와 `localtime_r()`를 사용하므로, 아래 명령어는 **Ubuntu/Linux 기준**입니다.

---

## 1. 필요한 파일

프로젝트 폴더에 다음 파일이 있어야 합니다.

```text
traffic_light_project/
├── TrafficLight_v2.cpp
└── README.md
```

터미널에서 소스 파일이 있는 폴더로 이동합니다.

```bash
cd ~/원하는/프로젝트/경로
```

예시:

```bash
cd ~/Yonsei/Autovehicle/traffic_light_project
```

파일이 존재하는지 확인합니다.

```bash
ls
```

출력에 `TrafficLight_v2.cpp`가 보이면 됩니다.

---

## 2. 필수 패키지 설치

패키지 목록을 업데이트합니다.

```bash
sudo apt update
```

C++ 컴파일러, OpenCV 개발 라이브러리, `pkg-config`, 카메라 확인 도구를 설치합니다.

```bash
sudo apt install -y build-essential libopencv-dev pkg-config v4l-utils
```

설치 상태를 확인합니다.

```bash
g++ --version
pkg-config --modversion opencv4
```

두 명령어 모두 버전이 출력되면 준비가 완료된 것입니다.

---

## 3. 연결된 카메라 확인

사용 가능한 비디오 장치를 확인합니다.

```bash
ls -l /dev/video*
```

카메라 이름과 장치 번호를 자세히 확인하려면 다음 명령어를 사용합니다.

```bash
v4l2-ctl --list-devices
```

예시 출력:

```text
HD Pro Webcam C920:
    /dev/video2
    /dev/video3
```

이 경우 일반적으로 영상 촬영 장치는 `/dev/video2`입니다.

카메라가 지원하는 해상도와 영상 형식을 확인하려면 다음 명령어를 사용합니다.

```bash
v4l2-ctl -d /dev/video2 --list-formats-ext
```

> 카메라 번호는 연결 순서나 재부팅에 따라 바뀔 수 있으므로 실행 전에 확인하는 것이 안전합니다.

---

## 4. 코드 컴파일

이 코드는 `std::filesystem`, `std::optional`, 구조적 바인딩 등을 사용하므로 **C++17 이상**으로 컴파일해야 합니다.

```bash
g++ -std=c++17 -O2 -Wall -Wextra \
    TrafficLight_v2.cpp \
    -o traffic_light_v2 \
    $(pkg-config --cflags --libs opencv4)
```

컴파일에 성공하면 현재 폴더에 `traffic_light_v2` 실행 파일이 생성됩니다.

```bash
ls -l traffic_light_v2
```

실행 권한이 없다면 다음 명령어를 사용합니다.

```bash
chmod +x traffic_light_v2
```

---

## 5. 기본 실행

코드의 기본 카메라 번호는 `2`이므로 다음과 같이 실행할 수 있습니다.

```bash
./traffic_light_v2
```

카메라를 명시하는 방식을 권장합니다.

```bash
./traffic_light_v2 --camera 2
```

또는 장치 경로를 직접 지정합니다.

```bash
./traffic_light_v2 --camera /dev/video2
```

프로그램을 종료하려면 영상 창을 선택한 상태에서 키보드의 `q`를 누릅니다.

---

## 6. 권장 실행 명령어

### 6.1 Logitech C920을 `/dev/video2`로 실행

```bash
./traffic_light_v2 \
    --camera /dev/video2 \
    --width 640 \
    --height 480 \
    --save-every-n 30 \
    --print-every-frame
```

### 6.2 세로로 설치된 카메라를 시계 방향으로 회전

```bash
./traffic_light_v2 \
    --camera /dev/video2 \
    --rotate cw \
    --width 640 \
    --height 480 \
    --save-every-n 30
```

회전 옵션은 다음과 같습니다.

```text
none : 회전하지 않음
cw   : 시계 방향 90도
ccw  : 반시계 방향 90도
180  : 180도 회전
```

### 6.3 디버그 화면을 모두 표시

```bash
./traffic_light_v2 \
    --camera /dev/video2 \
    --show-mask \
    --show-candidates \
    --show-body-candidates \
    --show-pairs \
    --show-traffic-bbox \
    --print-every-frame \
    --save-debug \
    --save-every-n 30
```

이 명령어를 실행하면 다음 화면 또는 표시가 추가됩니다.

- 흰색 발광 영역 마스크
- 활성 색상 영역 마스크
- 어두운 신호등 몸체 마스크
- 불빛 후보 영역
- 신호등 몸체 후보 영역
- 불빛과 몸체의 조합 후보
- 최종 신호등 바운딩 박스

### 6.4 신호등이 검출된 프레임만 저장

```bash
./traffic_light_v2 \
    --camera /dev/video2 \
    --save-only-detected \
    --save-debug
```

### 6.5 저장 폴더 변경

```bash
./traffic_light_v2 \
    --camera /dev/video2 \
    --save-dir ~/traffic_light_result \
    --save-every-n 30
```

---

## 7. 프레임 저장 주의사항

이 프로그램은 별도의 저장 비활성화 옵션이 없으며, 기본 설정에서는 **모든 프레임을 이미지로 저장**합니다.

기본 저장 위치는 다음과 같습니다.

```text
~/Downloads/traffic_light_frames/session_날짜_시간/raw/
```

`--save-debug`를 사용하면 디버그 영상도 다음 폴더에 저장됩니다.

```text
~/Downloads/traffic_light_frames/session_날짜_시간/debug/
```

저장 공간이 빠르게 증가할 수 있으므로 일반 테스트에서는 다음 옵션을 권장합니다.

```bash
--save-every-n 30
```

이는 30프레임마다 한 장씩 저장합니다.

검출된 경우에만 저장하려면 다음 옵션을 추가합니다.

```bash
--save-only-detected
```

현재 저장 용량은 다음과 같이 확인할 수 있습니다.

```bash
du -sh ~/Downloads/traffic_light_frames
```

저장된 테스트 결과를 모두 삭제하려면 다음 명령어를 사용합니다.

```bash
rm -rf ~/Downloads/traffic_light_frames
```

> `rm -rf`는 휴지통을 거치지 않고 삭제하므로 경로를 반드시 확인한 후 실행하세요.

---

## 8. 주요 실행 옵션

| 옵션 | 설명 | 예시 |
|---|---|---|
| `--camera N` | 카메라 번호 지정 | `--camera 2` |
| `--camera PATH` | 카메라 장치 경로 지정 | `--camera /dev/video2` |
| `--rotate` | 영상 회전 | `--rotate cw` |
| `--width` | 요청 영상 너비 | `--width 640` |
| `--height` | 요청 영상 높이 | `--height 480` |
| `--confirm` | 초록불 안정화에 필요한 연속 프레임 수 | `--confirm 3` |
| `--roi` | 탐색 영역 비율 `x1 y1 x2 y2` | `--roi 0 0.1 1 0.65` |
| `--print-every-frame` | 매 프레임 판단 결과 출력 | `--print-every-frame` |
| `--save-dir` | 이미지 저장 폴더 | `--save-dir ~/result` |
| `--save-format` | 저장 형식 | `--save-format png` |
| `--jpeg-quality` | JPG 품질 1~100 | `--jpeg-quality 95` |
| `--save-every-n` | N프레임마다 저장 | `--save-every-n 30` |
| `--save-debug` | 디버그 영상도 저장 | `--save-debug` |
| `--save-only-detected` | 색상이 검출된 프레임만 저장 | `--save-only-detected` |
| `--allow-portrait` | 세로 영상을 자동 가로 변환하지 않음 | `--allow-portrait` |
| `--show-mask` | 검출용 마스크 창 표시 | `--show-mask` |
| `--show-candidates` | 불빛 후보 표시 | `--show-candidates` |
| `--show-body-candidates` | 몸체 후보 표시 | `--show-body-candidates` |
| `--show-pairs` | 불빛·몸체 조합 표시 | `--show-pairs` |
| `--show-traffic-bbox` | 최종 신호등 영역 표시 | `--show-traffic-bbox` |
| `--manual-exposure` | 수동 노출값 요청 | `--manual-exposure 100` |
| `--disable-color-override` | 색상 기반 위치 보정 비활성화 | `--disable-color-override` |
| `--disable-color-fallback` | 색상 단독 검출 방식 비활성화 | `--disable-color-fallback` |
| `--help` | 도움말 출력 | `./traffic_light_v2 --help` |

---

## 9. 터미널 출력 해석

프로그램은 다음 형식으로 판단 결과를 출력합니다.

```text
EXIST=1 STATE=[1,0,0] ACTION=0 (STOP) LABEL=R
```

각 항목의 의미는 다음과 같습니다.

```text
EXIST=0 : 신호등을 찾지 못함
EXIST=1 : 신호등을 찾음

STATE=[R,O,G]
R : 빨간불 상태
O : 주황불 상태
G : 초록불 상태

ACTION=1 : GO
ACTION=0 : STOP

LABEL=R : 안정화된 빨간불
LABEL=O : 안정화된 주황불
LABEL=G : 안정화된 초록불
LABEL=N : 확정된 색상 없음
```

판단 규칙은 다음과 같습니다.

| 신호등 존재 | 상태 `[R,O,G]` | 최종 판단 |
|---|---:|---|
| 없음 | `[0,0,0]` | `GO(1)` |
| 있음 | `[0,0,1]` | `GO(1)` |
| 있음 | `[1,0,0]` | `STOP(0)` |
| 있음 | `[0,1,0]` | `STOP(0)` |
| 있음 | 둘 이상의 색상 검출 | `STOP(0)` |
| 있음 | 색상 미확정 | `STOP(0)` |

즉, 신호등이 검출되었을 때는 **초록불만 단독으로 켜진 경우에만 주행**합니다.

---

## 10. 수동 노출 설정

카메라가 지원하는 노출 관련 설정을 확인합니다.

```bash
v4l2-ctl -d /dev/video2 --list-ctrls
```

프로그램 실행 시 수동 노출값을 전달할 수 있습니다.

```bash
./traffic_light_v2 \
    --camera /dev/video2 \
    --manual-exposure 100 \
    --save-every-n 30
```

노출값의 유효 범위는 카메라마다 다릅니다. 설정이 적용되지 않는 경우 `v4l2-ctl --list-ctrls` 출력에서 범위를 확인하세요.

---

## 11. 자주 발생하는 오류

### 11.1 `opencv2/opencv.hpp: No such file or directory`

OpenCV 개발 패키지가 설치되지 않았거나 컴파일 명령어에 OpenCV 옵션이 빠진 경우입니다.

```bash
sudo apt update
sudo apt install -y libopencv-dev pkg-config
pkg-config --modversion opencv4
```

반드시 다음 부분을 포함하여 컴파일합니다.

```bash
$(pkg-config --cflags --libs opencv4)
```

---

### 11.2 `Package opencv4 was not found`

`pkg-config` 또는 OpenCV 개발 패키지를 다시 설치합니다.

```bash
sudo apt install --reinstall -y pkg-config libopencv-dev
```

설치 파일을 확인합니다.

```bash
pkg-config --list-all | grep -i opencv
```

---

### 11.3 `[ERROR] camera open failed`

먼저 실제 카메라 번호를 확인합니다.

```bash
v4l2-ctl --list-devices
ls -l /dev/video*
```

확인된 장치로 다시 실행합니다.

```bash
./traffic_light_v2 --camera /dev/video2
```

다른 프로그램이 카메라를 사용하고 있는지 확인합니다.

```bash
fuser /dev/video2
```

PID가 출력되면 해당 프로그램을 먼저 종료합니다. 브라우저, Cheese, OBS, 다른 ROS 카메라 노드가 카메라를 점유하고 있을 수 있습니다.

---

### 11.4 `Permission denied` 또는 카메라 권한 오류

현재 사용자가 `video` 그룹에 포함되어 있는지 확인합니다.

```bash
groups
```

`video`가 없다면 다음 명령어를 실행합니다.

```bash
sudo usermod -aG video "$USER"
```

이후 로그아웃 후 다시 로그인하거나 컴퓨터를 재부팅합니다.

---

### 11.5 창이 열리지 않거나 Qt/X11 오류 발생

이 프로그램은 `cv::imshow()`를 사용하므로 GUI 데스크톱 환경에서 실행해야 합니다.

현재 디스플레이 설정을 확인합니다.

```bash
echo $DISPLAY
```

SSH 환경이라면 X11 포워딩 없이 영상 창을 표시할 수 없습니다. Ubuntu 데스크톱의 터미널에서 직접 실행하는 것을 권장합니다.

---

### 11.6 영상이 너무 어둡거나 신호등 색상이 흰색으로 보임

자동 노출 때문에 신호등이 포화될 수 있습니다. 다음 순서로 테스트합니다.

```bash
v4l2-ctl -d /dev/video2 --list-ctrls
```

그다음 여러 노출값을 시험합니다.

```bash
./traffic_light_v2 --camera /dev/video2 --manual-exposure 50 --save-every-n 30
```

```bash
./traffic_light_v2 --camera /dev/video2 --manual-exposure 100 --save-every-n 30
```

```bash
./traffic_light_v2 --camera /dev/video2 --manual-exposure 200 --save-every-n 30
```

카메라마다 값의 범위와 방향이 다르므로 실제 영상과 마스크를 보면서 조정해야 합니다.

---

### 11.7 저장 폴더에 이미지가 너무 많이 생성됨

기본적으로 매 프레임이 저장되므로 실행 시 다음 옵션을 사용합니다.

```bash
./traffic_light_v2 \
    --camera /dev/video2 \
    --save-every-n 30 \
    --save-only-detected
```

---

## 12. 한 번에 설치·컴파일·실행하기

카메라가 `/dev/video2`라고 가정한 전체 과정입니다.

```bash
sudo apt update
sudo apt install -y build-essential libopencv-dev pkg-config v4l-utils

cd ~/원하는/프로젝트/경로

v4l2-ctl --list-devices

g++ -std=c++17 -O2 -Wall -Wextra \
    TrafficLight_v2.cpp \
    -o traffic_light_v2 \
    $(pkg-config --cflags --libs opencv4)

./traffic_light_v2 \
    --camera /dev/video2 \
    --width 640 \
    --height 480 \
    --save-every-n 30 \
    --show-traffic-bbox \
    --print-every-frame
```

영상 창에서 `q`를 누르면 프로그램이 종료됩니다.

---

## 13. 코드 수정 후 다시 실행하는 과정

소스 코드를 수정한 뒤에는 실행 파일을 다시 컴파일해야 합니다.

```bash
g++ -std=c++17 -O2 -Wall -Wextra \
    TrafficLight_v2.cpp \
    -o traffic_light_v2 \
    $(pkg-config --cflags --libs opencv4)
```

컴파일이 성공하면 다시 실행합니다.

```bash
./traffic_light_v2 --camera /dev/video2 --save-every-n 30
```

기존 실행 파일을 먼저 삭제하고 새로 빌드하려면 다음과 같이 실행합니다.

```bash
rm -f traffic_light_v2

g++ -std=c++17 -O2 -Wall -Wextra \
    TrafficLight_v2.cpp \
    -o traffic_light_v2 \
    $(pkg-config --cflags --libs opencv4)
```

