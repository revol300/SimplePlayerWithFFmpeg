PROJECT_PATH=$(git rev-parse --show-toplevel)
echo ${PROJECT_PATH}

mkdir -p ${PROJECT_PATH}/build
pushd ${PROJECT_PATH}/build
  conan install ${PROJECT_PATH}/conan --build=missing
  cmake ${PROJECT_PATH}
  make -j9
popd
