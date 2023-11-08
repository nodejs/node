#!/bin/bash
set -ve

MACHINE=$(uname -m)
export CC=gcc

uname -a
clang --version  # make analyse
${CC} --version  # make -C test valgrind

for USE_ASSEMBLY in 0 1; do
	if [ "${MACHINE}" == "x86_64" ]; then
		export SSSE3_CFLAGS="-mssse3 -DBASE64_SSSE3_USE_ASM=${USE_ASSEMBLY}"
		export SSE41_CFLAGS="-msse4.1 -DBASE64_SSE41_USE_ASM=${USE_ASSEMBLY}"
		export SSE42_CFLAGS="-msse4.2 -DBASE64_SSE42_USE_ASM=${USE_ASSEMBLY}"
		export AVX_CFLAGS="-mavx -DBASE64_AVX_USE_ASM=${USE_ASSEMBLY}"
		export AVX2_CFLAGS="-mavx2 -DBASE64_AVX2_USE_ASM=${USE_ASSEMBLY}"
		# Temporarily disable AVX512; it is not available in CI yet.
		# export AVX512_CFLAGS="-mavx512vl -mavx512vbmi"
	elif [ "${MACHINE}" == "aarch64" ]; then
		export NEON64_CFLAGS="-march=armv8-a"
	elif [ "${MACHINE}" == "armv7l" ]; then
		export NEON32_CFLAGS="-march=armv7-a -mfloat-abi=hard -mfpu=neon"
	fi

	if [ ${USE_ASSEMBLY} -eq 0 ]; then
		echo "::group::analyze"
		make analyze
		echo "::endgroup::"
	fi

	echo "::group::valgrind (USE_ASSEMBLY=${USE_ASSEMBLY})"
	make clean
	make
	make -C test valgrind
	echo "::endgroup::"
done
