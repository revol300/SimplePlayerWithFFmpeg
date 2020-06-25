if ! test -f "$../build" ; then
  mkdir ../build
fi

if ! test -f "$../output" ; then
  mkdir ../output
fi

if ! test -f "$../build/Makefile"; then
  cd ../build
  cmake ../SimplePlayerWithFFmpeg
fi
cd ../build
make install -j9

