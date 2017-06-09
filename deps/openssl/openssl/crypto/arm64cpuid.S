#include "arm_arch.h"

.text
.arch	armv8-a+crypto

.align	5
.global	_armv7_neon_probe
.type	_armv7_neon_probe,%function
_armv7_neon_probe:
	orr	v15.16b, v15.16b, v15.16b
	ret
.size	_armv7_neon_probe,.-_armv7_neon_probe

.global	_armv7_tick
.type	_armv7_tick,%function
_armv7_tick:
	mrs	x0, CNTVCT_EL0
	ret
.size	_armv7_tick,.-_armv7_tick

.global	_armv8_aes_probe
.type	_armv8_aes_probe,%function
_armv8_aes_probe:
	aese	v0.16b, v0.16b
	ret
.size	_armv8_aes_probe,.-_armv8_aes_probe

.global	_armv8_sha1_probe
.type	_armv8_sha1_probe,%function
_armv8_sha1_probe:
	sha1h	s0, s0
	ret
.size	_armv8_sha1_probe,.-_armv8_sha1_probe

.global	_armv8_sha256_probe
.type	_armv8_sha256_probe,%function
_armv8_sha256_probe:
	sha256su0	v0.4s, v0.4s
	ret
.size	_armv8_sha256_probe,.-_armv8_sha256_probe
.global	_armv8_pmull_probe
.type	_armv8_pmull_probe,%function
_armv8_pmull_probe:
	pmull	v0.1q, v0.1d, v0.1d
	ret
.size	_armv8_pmull_probe,.-_armv8_pmull_probe
