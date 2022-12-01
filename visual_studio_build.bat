for /f %%i in ('git rev-parse --show-toplevel')   do set PROJECT_PATH=%%i

mkdir -p VisualStudio
pushd VisualStudio
  conan install %PROJECT_PATH%/conan/conanfile.txt -pr=playerProfile --build=missing --update
  cmake ..\
popd
