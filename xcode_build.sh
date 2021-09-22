if ! test -f "$./xcode" ; then
  mkdir ./xcode
fi

pushd xcode
  cmake -G Xcode ../
popd
