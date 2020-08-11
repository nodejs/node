export CC=riscv64-unknown-linux-gnu-gcc
export CXX=riscv64-unknown-linux-gnu-g++
export CC_host=gcc
export CXX_host=g++


./configure --cross-compiling --dest-cpu=riscv64  --verbose --openssl-no-asm
