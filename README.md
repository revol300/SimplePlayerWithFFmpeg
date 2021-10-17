# SimplePlayerWithFFmpeg&SDL

이 프로젝트는 ffmpeg과 sdl로 간단한 플레이어를 만들어보고자 만들었습니다.

## Build

### Windows (64bit)
#### 요구 사항
- Git : [https://git-scm.com/download/win](https://git-scm.com/download/win)
- Conan : https://conan.io/downloads.html
- Visual studio build tool [https://aka.ms/vs/16/release/vs_buildtools.exe](https://aka.ms/vs/16/release/vs_buildtools.exe)

#### 설치 방법
- Git, Conan, Visual studio build tool을 설치한다
- visual studio command prompt를 실행
- which cl, which link를 통해서 PATH를 검색
- 해당 PATH를 환경변수에 추가
- Git Bash 실행
- 다음 스크립트 실행
```bash
conan remote add bincrafters https://bincrafters.jfrog.io/artifactory/api/conan/public-conan
conan config set general.revisions_enabled=1
conan profile new playerProfile --detect
conan profile update settings.build_type=Debug playerProfile
```

### Mac
### Linux
