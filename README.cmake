Instructions for building with cmake

Make sure you have cmake:
  Ubuntu/Debian: sudo apt-get install cmake
  Mac: http://www.cmake.org/files/v2.8/cmake-2.8.3-Darwin-universal.dmg
  Other platforms: http://www.cmake.org/cmake/resources/software.html

To build:

  make -f Makefile.cmake
  make -f Makefile.cmake install

To run the tests:

  make -f Makefile.cmake test

To build the documentation:

  make -f Makefile.cmake doc
 
To read the documentation:

  man doc/node.1

To build distro packages (tgz, deb, rpm, PackageMaker):

  make -f Makefile.cmake package

To submit test results (see http://my.cdash.org/index.php?project=node):

  make -f Makefile.cmake cdash

To submit coverage test results:

  make -f Makefile.cmake cdash-cov

To submit valgrind test results:

  make -f Makefile.cmake cdash-mem

Cross-compiling:
  An example toolchain file for the CodeSourcery ARM toolchain is included in
  the cmake directory: codesourcery-arm-toolchain.cmake.

  Install the CodeSourcery toolchain, set the path to the toolchain in
  cmake/codesourcery-arm-toolchain.cmake, and uncomment the TOOLCHAIN_FILE
  variable in Makefile.cmake to use it.

  If you are using cmake directly, just add the flag
  "-DCMAKE_TOOLCHAIN_FILE=/path/to/toolchain-file" when
  running cmake.

Using cmake directly:
  cd ~/your-node-source-dir
  mkdir name-of-build-dir     (can be anything)
  cd name-of-build-dir
  cmake ..

  At this point you have generated a set of Makefiles and can use the standard
  make commands (make, make install, etc.). The Makefile.cmake file is just a
  wrapper around these commands; take a look at it for more details.

Other build targets:
  make Experimental
  make Nightly
  make NightlyMemoryCheck
  make Continuous

Additional options:
  In the CMakeLists.txt, you'll see things like
  option(SHARED_V8, ...). If you want to enable any of those options you can
  pass "-DOPTION=True" when running cmake (e.g., cmake -DSHARED_V8=True).

See http://nodejs.org/ for more information. For help and discussion
subscribe to the mailing list by visiting
http://groups.google.com/group/nodejs or by sending an email to
nodejs+subscribe@googlegroups.com.
