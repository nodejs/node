export CC=riscv64-unknown-linux-gnu-gcc
export CXX=riscv64-unknown-linux-gnu-g++
export CC_host=clang
export CXX_host=clang++


./configure --cross-compiling --dest-cpu=riscv64  --verbose --without-ssl
