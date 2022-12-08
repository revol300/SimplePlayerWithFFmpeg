# SimplePlayerWithFFmpeg&SDL

이 프로젝트는 ffmpeg과 sdl2로 간단한 플레이어를 만들어보고자 만들었습니다.

## How to build
### Windows (64bit)
#### 요구 사항
- Git : [https://git-scm.com/download/win](https://git-scm.com/download/win)
- Cmake: [https://cmake.org/download/](https://cmake.org/download/)
- Conan : [https://conan.io/downloads.html](https://conan.io/downloads.html)
- Visual studio [https://visualstudio.microsoft.com/ko/](https://visualstudio.microsoft.com/ko/)

#### 설치 방법
- Git, Cmake, Conan, Visual studio를 설치한다
- Developer Command Prompt를 실행
- where cl, where link를 통해서 PATH를 검색
- 해당 PATH를 환경변수에 추가
- Windows PowerShell 실행
- 다음 스크립트 실행
```
conan remote add bincrafters https://bincrafters.jfrog.io/artifactory/api/conan/public-conan
conan config set general.revisions_enabled=1
conan profile new playerProfile --detect
conan profile update settings.build_type=Debug playerProfile
.\visual_studio_build.bat
```
- 프로젝트 내에 생성된 VisualStudio 폴더를 Visual Studio로 연다.

### Mac & Ubuntu
#### 요구 사항
- brew [https://brew.sh/index_ko](https://brew.sh/index_ko) (Mac)
- xcode [https://developer.apple.com/kr/xcode/](https://developer.apple.com/kr/xcode/) (Mac)

다음 스크립트를 통해 git cmake conan gcc(Ubuntu) 설치

Mac
```bash
brew install conan cmake git
```

Ubuntu
```bash
apt install conan cmake git gcc
```

- 다음 스크립트 실행
```bash
conan remote add bincrafters https://bincrafters.jfrog.io/artifactory/api/conan/public-conan
conan config set general.revisions_enabled=1
conan profile new playerProfile --detect
conan profile update settings.build_type=Debug playerProfile
./build.sh
```
- 프로젝트 내의  build/bin에 실행파일이 생성됨
- Xcode 프로젝트 생성을 원할 시 다음 스크립트 실행
```bash
./xcode_build.sh
```
- Xcode 폴더에 Xcode 프로젝트 생성됨
