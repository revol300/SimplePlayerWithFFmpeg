PROJECT_PATH=$(git rev-parse --show-toplevel)
echo ${PROJECT_PATH}

mkdir -p ${PROJECT_PATH}/xcode
pushd xcode
  conan install ${PROJECT_PATH}/conan/conanfile.txt --build=missing --update
  cmake -G Xcode ../
popd
