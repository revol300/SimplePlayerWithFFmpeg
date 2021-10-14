PROJECT_PATH=$(git rev-parse --show-toplevel)
echo ${PROJECT_PATH}

mkdir -p ${PROJECT_PATH}/xcode
pushd xcode
  conan install ${PROJECT_PATH}/conan
  cmake -G Xcode ../
popd
