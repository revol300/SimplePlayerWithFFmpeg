PROJECT_PATH=$(git rev-parse --show-toplevel)
echo ${PROJECT_PATH}

mkdir -p ${PROJECT_PATH}/VisualStudio
pushd VisualStudio
  conan install ${PROJECT_PATH}/conan/conanfile.txt -pr=playerProfile --build=missing --update
  cmake ../
popd
