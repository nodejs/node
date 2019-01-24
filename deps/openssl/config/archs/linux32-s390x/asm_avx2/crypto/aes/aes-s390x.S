#include "s390x_arch.h"

.text

.type	AES_Te,@object
.align	256
AES_Te:
.long	0xc66363a5,0xc66363a5
.long	0xf87c7c84,0xf87c7c84
.long	0xee777799,0xee777799
.long	0xf67b7b8d,0xf67b7b8d
.long	0xfff2f20d,0xfff2f20d
.long	0xd66b6bbd,0xd66b6bbd
.long	0xde6f6fb1,0xde6f6fb1
.long	0x91c5c554,0x91c5c554
.long	0x60303050,0x60303050
.long	0x02010103,0x02010103
.long	0xce6767a9,0xce6767a9
.long	0x562b2b7d,0x562b2b7d
.long	0xe7fefe19,0xe7fefe19
.long	0xb5d7d762,0xb5d7d762
.long	0x4dababe6,0x4dababe6
.long	0xec76769a,0xec76769a
.long	0x8fcaca45,0x8fcaca45
.long	0x1f82829d,0x1f82829d
.long	0x89c9c940,0x89c9c940
.long	0xfa7d7d87,0xfa7d7d87
.long	0xeffafa15,0xeffafa15
.long	0xb25959eb,0xb25959eb
.long	0x8e4747c9,0x8e4747c9
.long	0xfbf0f00b,0xfbf0f00b
.long	0x41adadec,0x41adadec
.long	0xb3d4d467,0xb3d4d467
.long	0x5fa2a2fd,0x5fa2a2fd
.long	0x45afafea,0x45afafea
.long	0x239c9cbf,0x239c9cbf
.long	0x53a4a4f7,0x53a4a4f7
.long	0xe4727296,0xe4727296
.long	0x9bc0c05b,0x9bc0c05b
.long	0x75b7b7c2,0x75b7b7c2
.long	0xe1fdfd1c,0xe1fdfd1c
.long	0x3d9393ae,0x3d9393ae
.long	0x4c26266a,0x4c26266a
.long	0x6c36365a,0x6c36365a
.long	0x7e3f3f41,0x7e3f3f41
.long	0xf5f7f702,0xf5f7f702
.long	0x83cccc4f,0x83cccc4f
.long	0x6834345c,0x6834345c
.long	0x51a5a5f4,0x51a5a5f4
.long	0xd1e5e534,0xd1e5e534
.long	0xf9f1f108,0xf9f1f108
.long	0xe2717193,0xe2717193
.long	0xabd8d873,0xabd8d873
.long	0x62313153,0x62313153
.long	0x2a15153f,0x2a15153f
.long	0x0804040c,0x0804040c
.long	0x95c7c752,0x95c7c752
.long	0x46232365,0x46232365
.long	0x9dc3c35e,0x9dc3c35e
.long	0x30181828,0x30181828
.long	0x379696a1,0x379696a1
.long	0x0a05050f,0x0a05050f
.long	0x2f9a9ab5,0x2f9a9ab5
.long	0x0e070709,0x0e070709
.long	0x24121236,0x24121236
.long	0x1b80809b,0x1b80809b
.long	0xdfe2e23d,0xdfe2e23d
.long	0xcdebeb26,0xcdebeb26
.long	0x4e272769,0x4e272769
.long	0x7fb2b2cd,0x7fb2b2cd
.long	0xea75759f,0xea75759f
.long	0x1209091b,0x1209091b
.long	0x1d83839e,0x1d83839e
.long	0x582c2c74,0x582c2c74
.long	0x341a1a2e,0x341a1a2e
.long	0x361b1b2d,0x361b1b2d
.long	0xdc6e6eb2,0xdc6e6eb2
.long	0xb45a5aee,0xb45a5aee
.long	0x5ba0a0fb,0x5ba0a0fb
.long	0xa45252f6,0xa45252f6
.long	0x763b3b4d,0x763b3b4d
.long	0xb7d6d661,0xb7d6d661
.long	0x7db3b3ce,0x7db3b3ce
.long	0x5229297b,0x5229297b
.long	0xdde3e33e,0xdde3e33e
.long	0x5e2f2f71,0x5e2f2f71
.long	0x13848497,0x13848497
.long	0xa65353f5,0xa65353f5
.long	0xb9d1d168,0xb9d1d168
.long	0x00000000,0x00000000
.long	0xc1eded2c,0xc1eded2c
.long	0x40202060,0x40202060
.long	0xe3fcfc1f,0xe3fcfc1f
.long	0x79b1b1c8,0x79b1b1c8
.long	0xb65b5bed,0xb65b5bed
.long	0xd46a6abe,0xd46a6abe
.long	0x8dcbcb46,0x8dcbcb46
.long	0x67bebed9,0x67bebed9
.long	0x7239394b,0x7239394b
.long	0x944a4ade,0x944a4ade
.long	0x984c4cd4,0x984c4cd4
.long	0xb05858e8,0xb05858e8
.long	0x85cfcf4a,0x85cfcf4a
.long	0xbbd0d06b,0xbbd0d06b
.long	0xc5efef2a,0xc5efef2a
.long	0x4faaaae5,0x4faaaae5
.long	0xedfbfb16,0xedfbfb16
.long	0x864343c5,0x864343c5
.long	0x9a4d4dd7,0x9a4d4dd7
.long	0x66333355,0x66333355
.long	0x11858594,0x11858594
.long	0x8a4545cf,0x8a4545cf
.long	0xe9f9f910,0xe9f9f910
.long	0x04020206,0x04020206
.long	0xfe7f7f81,0xfe7f7f81
.long	0xa05050f0,0xa05050f0
.long	0x783c3c44,0x783c3c44
.long	0x259f9fba,0x259f9fba
.long	0x4ba8a8e3,0x4ba8a8e3
.long	0xa25151f3,0xa25151f3
.long	0x5da3a3fe,0x5da3a3fe
.long	0x804040c0,0x804040c0
.long	0x058f8f8a,0x058f8f8a
.long	0x3f9292ad,0x3f9292ad
.long	0x219d9dbc,0x219d9dbc
.long	0x70383848,0x70383848
.long	0xf1f5f504,0xf1f5f504
.long	0x63bcbcdf,0x63bcbcdf
.long	0x77b6b6c1,0x77b6b6c1
.long	0xafdada75,0xafdada75
.long	0x42212163,0x42212163
.long	0x20101030,0x20101030
.long	0xe5ffff1a,0xe5ffff1a
.long	0xfdf3f30e,0xfdf3f30e
.long	0xbfd2d26d,0xbfd2d26d
.long	0x81cdcd4c,0x81cdcd4c
.long	0x180c0c14,0x180c0c14
.long	0x26131335,0x26131335
.long	0xc3ecec2f,0xc3ecec2f
.long	0xbe5f5fe1,0xbe5f5fe1
.long	0x359797a2,0x359797a2
.long	0x884444cc,0x884444cc
.long	0x2e171739,0x2e171739
.long	0x93c4c457,0x93c4c457
.long	0x55a7a7f2,0x55a7a7f2
.long	0xfc7e7e82,0xfc7e7e82
.long	0x7a3d3d47,0x7a3d3d47
.long	0xc86464ac,0xc86464ac
.long	0xba5d5de7,0xba5d5de7
.long	0x3219192b,0x3219192b
.long	0xe6737395,0xe6737395
.long	0xc06060a0,0xc06060a0
.long	0x19818198,0x19818198
.long	0x9e4f4fd1,0x9e4f4fd1
.long	0xa3dcdc7f,0xa3dcdc7f
.long	0x44222266,0x44222266
.long	0x542a2a7e,0x542a2a7e
.long	0x3b9090ab,0x3b9090ab
.long	0x0b888883,0x0b888883
.long	0x8c4646ca,0x8c4646ca
.long	0xc7eeee29,0xc7eeee29
.long	0x6bb8b8d3,0x6bb8b8d3
.long	0x2814143c,0x2814143c
.long	0xa7dede79,0xa7dede79
.long	0xbc5e5ee2,0xbc5e5ee2
.long	0x160b0b1d,0x160b0b1d
.long	0xaddbdb76,0xaddbdb76
.long	0xdbe0e03b,0xdbe0e03b
.long	0x64323256,0x64323256
.long	0x743a3a4e,0x743a3a4e
.long	0x140a0a1e,0x140a0a1e
.long	0x924949db,0x924949db
.long	0x0c06060a,0x0c06060a
.long	0x4824246c,0x4824246c
.long	0xb85c5ce4,0xb85c5ce4
.long	0x9fc2c25d,0x9fc2c25d
.long	0xbdd3d36e,0xbdd3d36e
.long	0x43acacef,0x43acacef
.long	0xc46262a6,0xc46262a6
.long	0x399191a8,0x399191a8
.long	0x319595a4,0x319595a4
.long	0xd3e4e437,0xd3e4e437
.long	0xf279798b,0xf279798b
.long	0xd5e7e732,0xd5e7e732
.long	0x8bc8c843,0x8bc8c843
.long	0x6e373759,0x6e373759
.long	0xda6d6db7,0xda6d6db7
.long	0x018d8d8c,0x018d8d8c
.long	0xb1d5d564,0xb1d5d564
.long	0x9c4e4ed2,0x9c4e4ed2
.long	0x49a9a9e0,0x49a9a9e0
.long	0xd86c6cb4,0xd86c6cb4
.long	0xac5656fa,0xac5656fa
.long	0xf3f4f407,0xf3f4f407
.long	0xcfeaea25,0xcfeaea25
.long	0xca6565af,0xca6565af
.long	0xf47a7a8e,0xf47a7a8e
.long	0x47aeaee9,0x47aeaee9
.long	0x10080818,0x10080818
.long	0x6fbabad5,0x6fbabad5
.long	0xf0787888,0xf0787888
.long	0x4a25256f,0x4a25256f
.long	0x5c2e2e72,0x5c2e2e72
.long	0x381c1c24,0x381c1c24
.long	0x57a6a6f1,0x57a6a6f1
.long	0x73b4b4c7,0x73b4b4c7
.long	0x97c6c651,0x97c6c651
.long	0xcbe8e823,0xcbe8e823
.long	0xa1dddd7c,0xa1dddd7c
.long	0xe874749c,0xe874749c
.long	0x3e1f1f21,0x3e1f1f21
.long	0x964b4bdd,0x964b4bdd
.long	0x61bdbddc,0x61bdbddc
.long	0x0d8b8b86,0x0d8b8b86
.long	0x0f8a8a85,0x0f8a8a85
.long	0xe0707090,0xe0707090
.long	0x7c3e3e42,0x7c3e3e42
.long	0x71b5b5c4,0x71b5b5c4
.long	0xcc6666aa,0xcc6666aa
.long	0x904848d8,0x904848d8
.long	0x06030305,0x06030305
.long	0xf7f6f601,0xf7f6f601
.long	0x1c0e0e12,0x1c0e0e12
.long	0xc26161a3,0xc26161a3
.long	0x6a35355f,0x6a35355f
.long	0xae5757f9,0xae5757f9
.long	0x69b9b9d0,0x69b9b9d0
.long	0x17868691,0x17868691
.long	0x99c1c158,0x99c1c158
.long	0x3a1d1d27,0x3a1d1d27
.long	0x279e9eb9,0x279e9eb9
.long	0xd9e1e138,0xd9e1e138
.long	0xebf8f813,0xebf8f813
.long	0x2b9898b3,0x2b9898b3
.long	0x22111133,0x22111133
.long	0xd26969bb,0xd26969bb
.long	0xa9d9d970,0xa9d9d970
.long	0x078e8e89,0x078e8e89
.long	0x339494a7,0x339494a7
.long	0x2d9b9bb6,0x2d9b9bb6
.long	0x3c1e1e22,0x3c1e1e22
.long	0x15878792,0x15878792
.long	0xc9e9e920,0xc9e9e920
.long	0x87cece49,0x87cece49
.long	0xaa5555ff,0xaa5555ff
.long	0x50282878,0x50282878
.long	0xa5dfdf7a,0xa5dfdf7a
.long	0x038c8c8f,0x038c8c8f
.long	0x59a1a1f8,0x59a1a1f8
.long	0x09898980,0x09898980
.long	0x1a0d0d17,0x1a0d0d17
.long	0x65bfbfda,0x65bfbfda
.long	0xd7e6e631,0xd7e6e631
.long	0x844242c6,0x844242c6
.long	0xd06868b8,0xd06868b8
.long	0x824141c3,0x824141c3
.long	0x299999b0,0x299999b0
.long	0x5a2d2d77,0x5a2d2d77
.long	0x1e0f0f11,0x1e0f0f11
.long	0x7bb0b0cb,0x7bb0b0cb
.long	0xa85454fc,0xa85454fc
.long	0x6dbbbbd6,0x6dbbbbd6
.long	0x2c16163a,0x2c16163a
# Te4[256]
.byte	0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5
.byte	0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76
.byte	0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0
.byte	0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0
.byte	0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc
.byte	0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15
.byte	0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a
.byte	0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75
.byte	0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0
.byte	0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84
.byte	0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b
.byte	0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf
.byte	0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85
.byte	0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8
.byte	0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5
.byte	0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2
.byte	0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17
.byte	0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73
.byte	0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88
.byte	0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb
.byte	0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c
.byte	0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79
.byte	0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9
.byte	0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08
.byte	0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6
.byte	0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a
.byte	0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e
.byte	0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e
.byte	0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94
.byte	0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf
.byte	0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68
.byte	0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16
# rcon[]
.long	0x01000000, 0x02000000, 0x04000000, 0x08000000
.long	0x10000000, 0x20000000, 0x40000000, 0x80000000
.long	0x1B000000, 0x36000000, 0, 0, 0, 0, 0, 0
.align	256
.size	AES_Te,.-AES_Te

# void AES_encrypt(const unsigned char *inp, unsigned char *out,
# 		 const AES_KEY *key) {
.globl	AES_encrypt
.type	AES_encrypt,@function
AES_encrypt:
	l	%r0,240(%r4)
	lhi	%r1,16
	clr	%r0,%r1
	jl	.Lesoft

	la	%r1,0(%r4)
	#la	%r2,0(%r2)
	la	%r4,0(%r3)
	lghi	%r3,16		# single block length
	.long	0xb92e0042	# km %r4,%r2
	brc	1,.-4		# can this happen?
	br	%r14
.align	64
.Lesoft:
	stm	%r3,%r14,3*4(%r15)

	llgf	%r8,0(%r2)
	llgf	%r9,4(%r2)
	llgf	%r10,8(%r2)
	llgf	%r11,12(%r2)

	larl	%r12,AES_Te
	bras	%r14,_s390x_AES_encrypt

	l	%r3,3*4(%r15)
	st	%r8,0(%r3)
	st	%r9,4(%r3)
	st	%r10,8(%r3)
	st	%r11,12(%r3)

	lm	%r6,%r14,6*4(%r15)
	br	%r14
.size	AES_encrypt,.-AES_encrypt

.type   _s390x_AES_encrypt,@function
.align	16
_s390x_AES_encrypt:
	st	%r14,15*4(%r15)
	x	%r8,0(%r4)
	x	%r9,4(%r4)
	x	%r10,8(%r4)
	x	%r11,12(%r4)
	l	%r13,240(%r4)
	llill	%r0,2040
	aghi	%r13,-1
	j	.Lenc_loop
.align	16
.Lenc_loop:
	sllg	%r1,%r8,3
	srlg	%r2,%r8,5
	srlg	%r3,%r8,13
	srl	%r8,21
	nr	%r8,%r0
	ngr	%r1,%r0
	nr	%r2,%r0
	nr	%r3,%r0

	srlg	%r5,%r9,13	# i0
	sllg	%r6,%r9,3
	srlg	%r7,%r9,5
	srl	%r9,21
	nr	%r5,%r0
	nr	%r9,%r0
	ngr	%r6,%r0
	nr	%r7,%r0

	l	%r8,0(%r8,%r12)	# Te0[s0>>24]
	l	%r1,1(%r1,%r12)	# Te3[s0>>0]
	l	%r2,2(%r2,%r12) # Te2[s0>>8]
	l	%r3,3(%r3,%r12)	# Te1[s0>>16]

	x	%r8,3(%r5,%r12)	# Te1[s1>>16]
	l	%r9,0(%r9,%r12)	# Te0[s1>>24]
	x	%r2,1(%r6,%r12)	# Te3[s1>>0]
	x	%r3,2(%r7,%r12)	# Te2[s1>>8]

	srlg	%r5,%r10,5	# i0
	srlg	%r6,%r10,13	# i1
	nr	%r5,%r0
	nr	%r6,%r0
	sllg	%r7,%r10,3
	srl	%r10,21
	nr	%r10,%r0
	ngr	%r7,%r0

	xr	%r9,%r1
	srlg	%r14,%r11,5	# i1
	sllg	%r1,%r11,3	# i0
	nr	%r14,%r0
	la	%r4,16(%r4)
	ngr	%r1,%r0

	x	%r8,2(%r5,%r12)	# Te2[s2>>8]
	x	%r9,3(%r6,%r12)	# Te1[s2>>16]
	l	%r10,0(%r10,%r12)	# Te0[s2>>24]
	x	%r3,1(%r7,%r12)	# Te3[s2>>0]

	srlg	%r7,%r11,13	# i2
	xr	%r10,%r2
	srl	%r11,21
	nr	%r7,%r0
	nr	%r11,%r0

	x	%r8,0(%r4)
	x	%r9,4(%r4)
	x	%r10,8(%r4)
	x	%r3,12(%r4)

	x	%r8,1(%r1,%r12)	# Te3[s3>>0]
	x	%r9,2(%r14,%r12)	# Te2[s3>>8]
	x	%r10,3(%r7,%r12)	# Te1[s3>>16]
	l	%r11,0(%r11,%r12)	# Te0[s3>>24]
	xr	%r11,%r3

	brct	%r13,.Lenc_loop
	.align	16

	sllg	%r1,%r8,3
	srlg	%r2,%r8,5
	ngr	%r1,%r0
	srlg	%r3,%r8,13
	srl	%r8,21
	nr	%r8,%r0
	nr	%r2,%r0
	nr	%r3,%r0

	srlg	%r5,%r9,13	# i0
	sllg	%r6,%r9,3
	ngr	%r6,%r0
	srlg	%r7,%r9,5
	srl	%r9,21
	nr	%r5,%r0
	nr	%r9,%r0
	nr	%r7,%r0

	llgc	%r8,2(%r8,%r12)	# Te4[s0>>24]
	llgc	%r1,2(%r1,%r12)	# Te4[s0>>0]
	sll	%r8,24
	llgc	%r2,2(%r2,%r12)	# Te4[s0>>8]
	llgc	%r3,2(%r3,%r12)	# Te4[s0>>16]
	sll	%r2,8
	sll	%r3,16

	llgc	%r5,2(%r5,%r12)	# Te4[s1>>16]
	llgc	%r9,2(%r9,%r12)	# Te4[s1>>24]
	llgc	%r6,2(%r6,%r12)	# Te4[s1>>0]
	llgc	%r7,2(%r7,%r12)	# Te4[s1>>8]
	sll	%r5,16
	sll	%r9,24
	sll	%r7,8
	or	%r8,%r5
	or	%r9,%r1
	or	%r2,%r6
	or	%r3,%r7

	srlg	%r5,%r10,5	# i0
	srlg	%r6,%r10,13	# i1
	nr	%r5,%r0
	nr	%r6,%r0
	sllg	%r7,%r10,3
	srl	%r10,21
	ngr	%r7,%r0
	nr	%r10,%r0

	sllg	%r1,%r11,3	# i0
	srlg	%r14,%r11,5	# i1
	ngr	%r1,%r0

	llgc	%r5,2(%r5,%r12)	# Te4[s2>>8]
	llgc	%r6,2(%r6,%r12)	# Te4[s2>>16]
	sll	%r5,8
	llgc	%r10,2(%r10,%r12)	# Te4[s2>>24]
	llgc	%r7,2(%r7,%r12)	# Te4[s2>>0]
	sll	%r6,16
	nr	%r14,%r0
	sll	%r10,24
	or	%r8,%r5
	or	%r9,%r6
	or	%r10,%r2
	or	%r3,%r7

	srlg	%r7,%r11,13	# i2
	srl	%r11,21
	nr	%r7,%r0
	nr	%r11,%r0

	l	%r0,16(%r4)
	l	%r2,20(%r4)

	llgc	%r5,2(%r1,%r12)	# Te4[s3>>0]
	llgc	%r6,2(%r14,%r12)	# Te4[s3>>8]
	llgc	%r7,2(%r7,%r12)	# Te4[s3>>16]
	llgc	%r11,2(%r11,%r12)	# Te4[s3>>24]
	sll	%r6,8
	sll	%r7,16
	sll	%r11,24
	or	%r8,%r5
	or	%r9,%r6
	or	%r10,%r7
	or	%r11,%r3

	l	%r14,15*4(%r15)
	xr	%r8,%r0
	xr	%r9,%r2
	x	%r10,24(%r4)
	x	%r11,28(%r4)

	br	%r14
.size	_s390x_AES_encrypt,.-_s390x_AES_encrypt
.type	AES_Td,@object
.align	256
AES_Td:
.long	0x51f4a750,0x51f4a750
.long	0x7e416553,0x7e416553
.long	0x1a17a4c3,0x1a17a4c3
.long	0x3a275e96,0x3a275e96
.long	0x3bab6bcb,0x3bab6bcb
.long	0x1f9d45f1,0x1f9d45f1
.long	0xacfa58ab,0xacfa58ab
.long	0x4be30393,0x4be30393
.long	0x2030fa55,0x2030fa55
.long	0xad766df6,0xad766df6
.long	0x88cc7691,0x88cc7691
.long	0xf5024c25,0xf5024c25
.long	0x4fe5d7fc,0x4fe5d7fc
.long	0xc52acbd7,0xc52acbd7
.long	0x26354480,0x26354480
.long	0xb562a38f,0xb562a38f
.long	0xdeb15a49,0xdeb15a49
.long	0x25ba1b67,0x25ba1b67
.long	0x45ea0e98,0x45ea0e98
.long	0x5dfec0e1,0x5dfec0e1
.long	0xc32f7502,0xc32f7502
.long	0x814cf012,0x814cf012
.long	0x8d4697a3,0x8d4697a3
.long	0x6bd3f9c6,0x6bd3f9c6
.long	0x038f5fe7,0x038f5fe7
.long	0x15929c95,0x15929c95
.long	0xbf6d7aeb,0xbf6d7aeb
.long	0x955259da,0x955259da
.long	0xd4be832d,0xd4be832d
.long	0x587421d3,0x587421d3
.long	0x49e06929,0x49e06929
.long	0x8ec9c844,0x8ec9c844
.long	0x75c2896a,0x75c2896a
.long	0xf48e7978,0xf48e7978
.long	0x99583e6b,0x99583e6b
.long	0x27b971dd,0x27b971dd
.long	0xbee14fb6,0xbee14fb6
.long	0xf088ad17,0xf088ad17
.long	0xc920ac66,0xc920ac66
.long	0x7dce3ab4,0x7dce3ab4
.long	0x63df4a18,0x63df4a18
.long	0xe51a3182,0xe51a3182
.long	0x97513360,0x97513360
.long	0x62537f45,0x62537f45
.long	0xb16477e0,0xb16477e0
.long	0xbb6bae84,0xbb6bae84
.long	0xfe81a01c,0xfe81a01c
.long	0xf9082b94,0xf9082b94
.long	0x70486858,0x70486858
.long	0x8f45fd19,0x8f45fd19
.long	0x94de6c87,0x94de6c87
.long	0x527bf8b7,0x527bf8b7
.long	0xab73d323,0xab73d323
.long	0x724b02e2,0x724b02e2
.long	0xe31f8f57,0xe31f8f57
.long	0x6655ab2a,0x6655ab2a
.long	0xb2eb2807,0xb2eb2807
.long	0x2fb5c203,0x2fb5c203
.long	0x86c57b9a,0x86c57b9a
.long	0xd33708a5,0xd33708a5
.long	0x302887f2,0x302887f2
.long	0x23bfa5b2,0x23bfa5b2
.long	0x02036aba,0x02036aba
.long	0xed16825c,0xed16825c
.long	0x8acf1c2b,0x8acf1c2b
.long	0xa779b492,0xa779b492
.long	0xf307f2f0,0xf307f2f0
.long	0x4e69e2a1,0x4e69e2a1
.long	0x65daf4cd,0x65daf4cd
.long	0x0605bed5,0x0605bed5
.long	0xd134621f,0xd134621f
.long	0xc4a6fe8a,0xc4a6fe8a
.long	0x342e539d,0x342e539d
.long	0xa2f355a0,0xa2f355a0
.long	0x058ae132,0x058ae132
.long	0xa4f6eb75,0xa4f6eb75
.long	0x0b83ec39,0x0b83ec39
.long	0x4060efaa,0x4060efaa
.long	0x5e719f06,0x5e719f06
.long	0xbd6e1051,0xbd6e1051
.long	0x3e218af9,0x3e218af9
.long	0x96dd063d,0x96dd063d
.long	0xdd3e05ae,0xdd3e05ae
.long	0x4de6bd46,0x4de6bd46
.long	0x91548db5,0x91548db5
.long	0x71c45d05,0x71c45d05
.long	0x0406d46f,0x0406d46f
.long	0x605015ff,0x605015ff
.long	0x1998fb24,0x1998fb24
.long	0xd6bde997,0xd6bde997
.long	0x894043cc,0x894043cc
.long	0x67d99e77,0x67d99e77
.long	0xb0e842bd,0xb0e842bd
.long	0x07898b88,0x07898b88
.long	0xe7195b38,0xe7195b38
.long	0x79c8eedb,0x79c8eedb
.long	0xa17c0a47,0xa17c0a47
.long	0x7c420fe9,0x7c420fe9
.long	0xf8841ec9,0xf8841ec9
.long	0x00000000,0x00000000
.long	0x09808683,0x09808683
.long	0x322bed48,0x322bed48
.long	0x1e1170ac,0x1e1170ac
.long	0x6c5a724e,0x6c5a724e
.long	0xfd0efffb,0xfd0efffb
.long	0x0f853856,0x0f853856
.long	0x3daed51e,0x3daed51e
.long	0x362d3927,0x362d3927
.long	0x0a0fd964,0x0a0fd964
.long	0x685ca621,0x685ca621
.long	0x9b5b54d1,0x9b5b54d1
.long	0x24362e3a,0x24362e3a
.long	0x0c0a67b1,0x0c0a67b1
.long	0x9357e70f,0x9357e70f
.long	0xb4ee96d2,0xb4ee96d2
.long	0x1b9b919e,0x1b9b919e
.long	0x80c0c54f,0x80c0c54f
.long	0x61dc20a2,0x61dc20a2
.long	0x5a774b69,0x5a774b69
.long	0x1c121a16,0x1c121a16
.long	0xe293ba0a,0xe293ba0a
.long	0xc0a02ae5,0xc0a02ae5
.long	0x3c22e043,0x3c22e043
.long	0x121b171d,0x121b171d
.long	0x0e090d0b,0x0e090d0b
.long	0xf28bc7ad,0xf28bc7ad
.long	0x2db6a8b9,0x2db6a8b9
.long	0x141ea9c8,0x141ea9c8
.long	0x57f11985,0x57f11985
.long	0xaf75074c,0xaf75074c
.long	0xee99ddbb,0xee99ddbb
.long	0xa37f60fd,0xa37f60fd
.long	0xf701269f,0xf701269f
.long	0x5c72f5bc,0x5c72f5bc
.long	0x44663bc5,0x44663bc5
.long	0x5bfb7e34,0x5bfb7e34
.long	0x8b432976,0x8b432976
.long	0xcb23c6dc,0xcb23c6dc
.long	0xb6edfc68,0xb6edfc68
.long	0xb8e4f163,0xb8e4f163
.long	0xd731dcca,0xd731dcca
.long	0x42638510,0x42638510
.long	0x13972240,0x13972240
.long	0x84c61120,0x84c61120
.long	0x854a247d,0x854a247d
.long	0xd2bb3df8,0xd2bb3df8
.long	0xaef93211,0xaef93211
.long	0xc729a16d,0xc729a16d
.long	0x1d9e2f4b,0x1d9e2f4b
.long	0xdcb230f3,0xdcb230f3
.long	0x0d8652ec,0x0d8652ec
.long	0x77c1e3d0,0x77c1e3d0
.long	0x2bb3166c,0x2bb3166c
.long	0xa970b999,0xa970b999
.long	0x119448fa,0x119448fa
.long	0x47e96422,0x47e96422
.long	0xa8fc8cc4,0xa8fc8cc4
.long	0xa0f03f1a,0xa0f03f1a
.long	0x567d2cd8,0x567d2cd8
.long	0x223390ef,0x223390ef
.long	0x87494ec7,0x87494ec7
.long	0xd938d1c1,0xd938d1c1
.long	0x8ccaa2fe,0x8ccaa2fe
.long	0x98d40b36,0x98d40b36
.long	0xa6f581cf,0xa6f581cf
.long	0xa57ade28,0xa57ade28
.long	0xdab78e26,0xdab78e26
.long	0x3fadbfa4,0x3fadbfa4
.long	0x2c3a9de4,0x2c3a9de4
.long	0x5078920d,0x5078920d
.long	0x6a5fcc9b,0x6a5fcc9b
.long	0x547e4662,0x547e4662
.long	0xf68d13c2,0xf68d13c2
.long	0x90d8b8e8,0x90d8b8e8
.long	0x2e39f75e,0x2e39f75e
.long	0x82c3aff5,0x82c3aff5
.long	0x9f5d80be,0x9f5d80be
.long	0x69d0937c,0x69d0937c
.long	0x6fd52da9,0x6fd52da9
.long	0xcf2512b3,0xcf2512b3
.long	0xc8ac993b,0xc8ac993b
.long	0x10187da7,0x10187da7
.long	0xe89c636e,0xe89c636e
.long	0xdb3bbb7b,0xdb3bbb7b
.long	0xcd267809,0xcd267809
.long	0x6e5918f4,0x6e5918f4
.long	0xec9ab701,0xec9ab701
.long	0x834f9aa8,0x834f9aa8
.long	0xe6956e65,0xe6956e65
.long	0xaaffe67e,0xaaffe67e
.long	0x21bccf08,0x21bccf08
.long	0xef15e8e6,0xef15e8e6
.long	0xbae79bd9,0xbae79bd9
.long	0x4a6f36ce,0x4a6f36ce
.long	0xea9f09d4,0xea9f09d4
.long	0x29b07cd6,0x29b07cd6
.long	0x31a4b2af,0x31a4b2af
.long	0x2a3f2331,0x2a3f2331
.long	0xc6a59430,0xc6a59430
.long	0x35a266c0,0x35a266c0
.long	0x744ebc37,0x744ebc37
.long	0xfc82caa6,0xfc82caa6
.long	0xe090d0b0,0xe090d0b0
.long	0x33a7d815,0x33a7d815
.long	0xf104984a,0xf104984a
.long	0x41ecdaf7,0x41ecdaf7
.long	0x7fcd500e,0x7fcd500e
.long	0x1791f62f,0x1791f62f
.long	0x764dd68d,0x764dd68d
.long	0x43efb04d,0x43efb04d
.long	0xccaa4d54,0xccaa4d54
.long	0xe49604df,0xe49604df
.long	0x9ed1b5e3,0x9ed1b5e3
.long	0x4c6a881b,0x4c6a881b
.long	0xc12c1fb8,0xc12c1fb8
.long	0x4665517f,0x4665517f
.long	0x9d5eea04,0x9d5eea04
.long	0x018c355d,0x018c355d
.long	0xfa877473,0xfa877473
.long	0xfb0b412e,0xfb0b412e
.long	0xb3671d5a,0xb3671d5a
.long	0x92dbd252,0x92dbd252
.long	0xe9105633,0xe9105633
.long	0x6dd64713,0x6dd64713
.long	0x9ad7618c,0x9ad7618c
.long	0x37a10c7a,0x37a10c7a
.long	0x59f8148e,0x59f8148e
.long	0xeb133c89,0xeb133c89
.long	0xcea927ee,0xcea927ee
.long	0xb761c935,0xb761c935
.long	0xe11ce5ed,0xe11ce5ed
.long	0x7a47b13c,0x7a47b13c
.long	0x9cd2df59,0x9cd2df59
.long	0x55f2733f,0x55f2733f
.long	0x1814ce79,0x1814ce79
.long	0x73c737bf,0x73c737bf
.long	0x53f7cdea,0x53f7cdea
.long	0x5ffdaa5b,0x5ffdaa5b
.long	0xdf3d6f14,0xdf3d6f14
.long	0x7844db86,0x7844db86
.long	0xcaaff381,0xcaaff381
.long	0xb968c43e,0xb968c43e
.long	0x3824342c,0x3824342c
.long	0xc2a3405f,0xc2a3405f
.long	0x161dc372,0x161dc372
.long	0xbce2250c,0xbce2250c
.long	0x283c498b,0x283c498b
.long	0xff0d9541,0xff0d9541
.long	0x39a80171,0x39a80171
.long	0x080cb3de,0x080cb3de
.long	0xd8b4e49c,0xd8b4e49c
.long	0x6456c190,0x6456c190
.long	0x7bcb8461,0x7bcb8461
.long	0xd532b670,0xd532b670
.long	0x486c5c74,0x486c5c74
.long	0xd0b85742,0xd0b85742
# Td4[256]
.byte	0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38
.byte	0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb
.byte	0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87
.byte	0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb
.byte	0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d
.byte	0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e
.byte	0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2
.byte	0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25
.byte	0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16
.byte	0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92
.byte	0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda
.byte	0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84
.byte	0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a
.byte	0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06
.byte	0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02
.byte	0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b
.byte	0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea
.byte	0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73
.byte	0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85
.byte	0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e
.byte	0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89
.byte	0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b
.byte	0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20
.byte	0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4
.byte	0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31
.byte	0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f
.byte	0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d
.byte	0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef
.byte	0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0
.byte	0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61
.byte	0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26
.byte	0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d
.size	AES_Td,.-AES_Td

# void AES_decrypt(const unsigned char *inp, unsigned char *out,
# 		 const AES_KEY *key) {
.globl	AES_decrypt
.type	AES_decrypt,@function
AES_decrypt:
	l	%r0,240(%r4)
	lhi	%r1,16
	clr	%r0,%r1
	jl	.Ldsoft

	la	%r1,0(%r4)
	#la	%r2,0(%r2)
	la	%r4,0(%r3)
	lghi	%r3,16		# single block length
	.long	0xb92e0042	# km %r4,%r2
	brc	1,.-4		# can this happen?
	br	%r14
.align	64
.Ldsoft:
	stm	%r3,%r14,3*4(%r15)

	llgf	%r8,0(%r2)
	llgf	%r9,4(%r2)
	llgf	%r10,8(%r2)
	llgf	%r11,12(%r2)

	larl	%r12,AES_Td
	bras	%r14,_s390x_AES_decrypt

	l	%r3,3*4(%r15)
	st	%r8,0(%r3)
	st	%r9,4(%r3)
	st	%r10,8(%r3)
	st	%r11,12(%r3)

	lm	%r6,%r14,6*4(%r15)
	br	%r14
.size	AES_decrypt,.-AES_decrypt

.type   _s390x_AES_decrypt,@function
.align	16
_s390x_AES_decrypt:
	st	%r14,15*4(%r15)
	x	%r8,0(%r4)
	x	%r9,4(%r4)
	x	%r10,8(%r4)
	x	%r11,12(%r4)
	l	%r13,240(%r4)
	llill	%r0,2040
	aghi	%r13,-1
	j	.Ldec_loop
.align	16
.Ldec_loop:
	srlg	%r1,%r8,13
	srlg	%r2,%r8,5
	sllg	%r3,%r8,3
	srl	%r8,21
	nr	%r8,%r0
	nr	%r1,%r0
	nr	%r2,%r0
	ngr	%r3,%r0

	sllg	%r5,%r9,3	# i0
	srlg	%r6,%r9,13
	srlg	%r7,%r9,5
	srl	%r9,21
	ngr	%r5,%r0
	nr	%r9,%r0
	nr	%r6,%r0
	nr	%r7,%r0

	l	%r8,0(%r8,%r12)	# Td0[s0>>24]
	l	%r1,3(%r1,%r12)	# Td1[s0>>16]
	l	%r2,2(%r2,%r12)	# Td2[s0>>8]
	l	%r3,1(%r3,%r12)	# Td3[s0>>0]

	x	%r8,1(%r5,%r12)	# Td3[s1>>0]
	l	%r9,0(%r9,%r12)	# Td0[s1>>24]
	x	%r2,3(%r6,%r12)	# Td1[s1>>16]
	x	%r3,2(%r7,%r12)	# Td2[s1>>8]

	srlg	%r5,%r10,5	# i0
	sllg	%r6,%r10,3	# i1
	srlg	%r7,%r10,13
	srl	%r10,21
	nr	%r5,%r0
	ngr	%r6,%r0
	nr	%r10,%r0
	nr	%r7,%r0

	xr	%r9,%r1
	srlg	%r14,%r11,5	# i1
	srlg	%r1,%r11,13	# i0
	nr	%r14,%r0
	la	%r4,16(%r4)
	nr	%r1,%r0

	x	%r8,2(%r5,%r12)	# Td2[s2>>8]
	x	%r9,1(%r6,%r12)	# Td3[s2>>0]
	l	%r10,0(%r10,%r12)	# Td0[s2>>24]
	x	%r3,3(%r7,%r12)	# Td1[s2>>16]

	sllg	%r7,%r11,3	# i2
	srl	%r11,21
	ngr	%r7,%r0
	nr	%r11,%r0

	xr	%r10,%r2
	x	%r8,0(%r4)
	x	%r9,4(%r4)
	x	%r10,8(%r4)
	x	%r3,12(%r4)

	x	%r8,3(%r1,%r12)	# Td1[s3>>16]
	x	%r9,2(%r14,%r12)	# Td2[s3>>8]
	x	%r10,1(%r7,%r12)	# Td3[s3>>0]
	l	%r11,0(%r11,%r12)	# Td0[s3>>24]
	xr	%r11,%r3

	brct	%r13,.Ldec_loop
	.align	16

	l	%r1,2048(%r12)	# prefetch Td4
	l	%r2,2112(%r12)
	l	%r3,2176(%r12)
	l	%r5,2240(%r12)
	llill	%r0,0xff

	srlg	%r7,%r8,24	# i0
	srlg	%r1,%r8,16
	srlg	%r2,%r8,8
	nr	%r8,%r0	# i3
	nr	%r1,%r0

	srlg	%r5,%r9,24
	nr	%r2,%r0
	srlg	%r6,%r9,16
	srlg	%r14,%r9,8
	nr	%r9,%r0	# i0
	nr	%r6,%r0
	nr	%r14,%r0

	llgc	%r7,2048(%r7,%r12)	# Td4[s0>>24]
	llgc	%r1,2048(%r1,%r12)	# Td4[s0>>16]
	llgc	%r2,2048(%r2,%r12)	# Td4[s0>>8]
	sll	%r1,16
	llgc	%r3,2048(%r8,%r12)	# Td4[s0>>0]
	sllg	%r8,%r7,24
	sll	%r2,8

	llgc	%r9,2048(%r9,%r12)	# Td4[s1>>0]
	llgc	%r5,2048(%r5,%r12)	# Td4[s1>>24]
	llgc	%r6,2048(%r6,%r12)	# Td4[s1>>16]
	sll	%r5,24
	llgc	%r7,2048(%r14,%r12)	# Td4[s1>>8]
	sll	%r6,16
	sll	%r7,8
	or	%r8,%r9
	or	%r1,%r5
	or	%r2,%r6
	or	%r3,%r7

	srlg	%r5,%r10,8	# i0
	srlg	%r6,%r10,24
	srlg	%r7,%r10,16
	nr	%r10,%r0	# i1
	nr	%r5,%r0
	nr	%r7,%r0
	llgc	%r5,2048(%r5,%r12)	# Td4[s2>>8]
	llgc	%r9,2048(%r10,%r12)	# Td4[s2>>0]
	llgc	%r6,2048(%r6,%r12)	# Td4[s2>>24]
	llgc	%r7,2048(%r7,%r12)	# Td4[s2>>16]
	sll	%r5,8
	sll	%r6,24
	or	%r8,%r5
	sll	%r7,16
	or	%r2,%r6
	or	%r3,%r7

	srlg	%r5,%r11,16	# i0
	srlg	%r6,%r11,8	# i1
	srlg	%r7,%r11,24
	nr	%r11,%r0	# i2
	nr	%r5,%r0
	nr	%r6,%r0

	l	%r14,15*4(%r15)
	or	%r9,%r1
	l	%r0,16(%r4)
	l	%r1,20(%r4)

	llgc	%r5,2048(%r5,%r12)	# Td4[s3>>16]
	llgc	%r6,2048(%r6,%r12)	# Td4[s3>>8]
	sll	%r5,16
	llgc	%r10,2048(%r11,%r12)	# Td4[s3>>0]
	llgc	%r11,2048(%r7,%r12)	# Td4[s3>>24]
	sll	%r6,8
	sll	%r11,24
	or	%r8,%r5
	or	%r9,%r6
	or	%r10,%r2
	or	%r11,%r3

	xr	%r8,%r0
	xr	%r9,%r1
	x	%r10,24(%r4)
	x	%r11,28(%r4)

	br	%r14
.size	_s390x_AES_decrypt,.-_s390x_AES_decrypt
# void AES_set_encrypt_key(const unsigned char *in, int bits,
# 		 AES_KEY *key) {
.globl	AES_set_encrypt_key
.type	AES_set_encrypt_key,@function
.align	16
AES_set_encrypt_key:
_s390x_AES_set_encrypt_key:
	lghi	%r0,0
	clr	%r2,%r0
	je	.Lminus1
	clr	%r4,%r0
	je	.Lminus1

	lghi	%r0,128
	clr	%r3,%r0
	je	.Lproceed
	lghi	%r0,192
	clr	%r3,%r0
	je	.Lproceed
	lghi	%r0,256
	clr	%r3,%r0
	je	.Lproceed
	lghi	%r2,-2
	br	%r14

.align	16
.Lproceed:
	# convert bits to km(c) code, [128,192,256]->[18,19,20]
	lhi	%r5,-128
	lhi	%r0,18
	ar	%r5,%r3
	srl	%r5,6
	ar	%r5,%r0

	larl	%r1,OPENSSL_s390xcap_P
	llihh	%r0,0x8000
	srlg	%r0,%r0,0(%r5)
	ng	%r0,S390X_KM(%r1)  # check availability of both km...
	ng	%r0,S390X_KMC(%r1) # ...and kmc support for given key length
	jz	.Lekey_internal

	lmg	%r0,%r1,0(%r2)	# just copy 128 bits...
	stmg	%r0,%r1,0(%r4)
	lhi	%r0,192
	cr	%r3,%r0
	jl	1f
	lg	%r1,16(%r2)
	stg	%r1,16(%r4)
	je	1f
	lg	%r1,24(%r2)
	stg	%r1,24(%r4)
1:	st	%r3,236(%r4)	# save bits [for debugging purposes]
	lgr	%r0,%r5
	st	%r5,240(%r4)	# save km(c) code
	lghi	%r2,0
	br	%r14
.align	16
.Lekey_internal:
	stm	%r4,%r13,4*4(%r15)	# all non-volatile regs and %r4

	larl	%r12,AES_Te+2048

	llgf	%r8,0(%r2)
	llgf	%r9,4(%r2)
	llgf	%r10,8(%r2)
	llgf	%r11,12(%r2)
	st	%r8,0(%r4)
	st	%r9,4(%r4)
	st	%r10,8(%r4)
	st	%r11,12(%r4)
	lghi	%r0,128
	cr	%r3,%r0
	jne	.Lnot128

	llill	%r0,0xff
	lghi	%r3,0			# i=0
	lghi	%r13,10
	st	%r13,240(%r4)

	llgfr	%r2,%r11			# temp=rk[3]
	srlg	%r5,%r11,8
	srlg	%r6,%r11,16
	srlg	%r7,%r11,24
	nr	%r2,%r0
	nr	%r5,%r0
	nr	%r6,%r0

.align	16
.L128_loop:
	la	%r2,0(%r2,%r12)
	la	%r5,0(%r5,%r12)
	la	%r6,0(%r6,%r12)
	la	%r7,0(%r7,%r12)
	icm	%r2,2,0(%r2)		# Te4[rk[3]>>0]<<8
	icm	%r2,4,0(%r5)		# Te4[rk[3]>>8]<<16
	icm	%r2,8,0(%r6)		# Te4[rk[3]>>16]<<24
	icm	%r2,1,0(%r7)		# Te4[rk[3]>>24]
	x	%r2,256(%r3,%r12)	# rcon[i]
	xr	%r8,%r2			# rk[4]=rk[0]^...
	xr	%r9,%r8			# rk[5]=rk[1]^rk[4]
	xr	%r10,%r9			# rk[6]=rk[2]^rk[5]
	xr	%r11,%r10			# rk[7]=rk[3]^rk[6]

	llgfr	%r2,%r11			# temp=rk[3]
	srlg	%r5,%r11,8
	srlg	%r6,%r11,16
	nr	%r2,%r0
	nr	%r5,%r0
	srlg	%r7,%r11,24
	nr	%r6,%r0

	st	%r8,16(%r4)
	st	%r9,20(%r4)
	st	%r10,24(%r4)
	st	%r11,28(%r4)
	la	%r4,16(%r4)		# key+=4
	la	%r3,4(%r3)		# i++
	brct	%r13,.L128_loop
	lghi	%r0,10
	lghi	%r2,0
	lm	%r4,%r13,4*4(%r15)
	br	%r14

.align	16
.Lnot128:
	llgf	%r0,16(%r2)
	llgf	%r1,20(%r2)
	st	%r0,16(%r4)
	st	%r1,20(%r4)
	lghi	%r0,192
	cr	%r3,%r0
	jne	.Lnot192

	llill	%r0,0xff
	lghi	%r3,0			# i=0
	lghi	%r13,12
	st	%r13,240(%r4)
	lghi	%r13,8

	srlg	%r5,%r1,8
	srlg	%r6,%r1,16
	srlg	%r7,%r1,24
	nr	%r1,%r0
	nr	%r5,%r0
	nr	%r6,%r0

.align	16
.L192_loop:
	la	%r1,0(%r1,%r12)
	la	%r5,0(%r5,%r12)
	la	%r6,0(%r6,%r12)
	la	%r7,0(%r7,%r12)
	icm	%r1,2,0(%r1)		# Te4[rk[5]>>0]<<8
	icm	%r1,4,0(%r5)		# Te4[rk[5]>>8]<<16
	icm	%r1,8,0(%r6)		# Te4[rk[5]>>16]<<24
	icm	%r1,1,0(%r7)		# Te4[rk[5]>>24]
	x	%r1,256(%r3,%r12)	# rcon[i]
	xr	%r8,%r1			# rk[6]=rk[0]^...
	xr	%r9,%r8			# rk[7]=rk[1]^rk[6]
	xr	%r10,%r9			# rk[8]=rk[2]^rk[7]
	xr	%r11,%r10			# rk[9]=rk[3]^rk[8]

	st	%r8,24(%r4)
	st	%r9,28(%r4)
	st	%r10,32(%r4)
	st	%r11,36(%r4)
	brct	%r13,.L192_continue
	lghi	%r0,12
	lghi	%r2,0
	lm	%r4,%r13,4*4(%r15)
	br	%r14

.align	16
.L192_continue:
	lgr	%r1,%r11
	x	%r1,16(%r4)		# rk[10]=rk[4]^rk[9]
	st	%r1,40(%r4)
	x	%r1,20(%r4)		# rk[11]=rk[5]^rk[10]
	st	%r1,44(%r4)

	srlg	%r5,%r1,8
	srlg	%r6,%r1,16
	srlg	%r7,%r1,24
	nr	%r1,%r0
	nr	%r5,%r0
	nr	%r6,%r0

	la	%r4,24(%r4)		# key+=6
	la	%r3,4(%r3)		# i++
	j	.L192_loop

.align	16
.Lnot192:
	llgf	%r0,24(%r2)
	llgf	%r1,28(%r2)
	st	%r0,24(%r4)
	st	%r1,28(%r4)
	llill	%r0,0xff
	lghi	%r3,0			# i=0
	lghi	%r13,14
	st	%r13,240(%r4)
	lghi	%r13,7

	srlg	%r5,%r1,8
	srlg	%r6,%r1,16
	srlg	%r7,%r1,24
	nr	%r1,%r0
	nr	%r5,%r0
	nr	%r6,%r0

.align	16
.L256_loop:
	la	%r1,0(%r1,%r12)
	la	%r5,0(%r5,%r12)
	la	%r6,0(%r6,%r12)
	la	%r7,0(%r7,%r12)
	icm	%r1,2,0(%r1)		# Te4[rk[7]>>0]<<8
	icm	%r1,4,0(%r5)		# Te4[rk[7]>>8]<<16
	icm	%r1,8,0(%r6)		# Te4[rk[7]>>16]<<24
	icm	%r1,1,0(%r7)		# Te4[rk[7]>>24]
	x	%r1,256(%r3,%r12)	# rcon[i]
	xr	%r8,%r1			# rk[8]=rk[0]^...
	xr	%r9,%r8			# rk[9]=rk[1]^rk[8]
	xr	%r10,%r9			# rk[10]=rk[2]^rk[9]
	xr	%r11,%r10			# rk[11]=rk[3]^rk[10]
	st	%r8,32(%r4)
	st	%r9,36(%r4)
	st	%r10,40(%r4)
	st	%r11,44(%r4)
	brct	%r13,.L256_continue
	lghi	%r0,14
	lghi	%r2,0
	lm	%r4,%r13,4*4(%r15)
	br	%r14

.align	16
.L256_continue:
	lgr	%r1,%r11			# temp=rk[11]
	srlg	%r5,%r11,8
	srlg	%r6,%r11,16
	srlg	%r7,%r11,24
	nr	%r1,%r0
	nr	%r5,%r0
	nr	%r6,%r0
	la	%r1,0(%r1,%r12)
	la	%r5,0(%r5,%r12)
	la	%r6,0(%r6,%r12)
	la	%r7,0(%r7,%r12)
	llgc	%r1,0(%r1)		# Te4[rk[11]>>0]
	icm	%r1,2,0(%r5)		# Te4[rk[11]>>8]<<8
	icm	%r1,4,0(%r6)		# Te4[rk[11]>>16]<<16
	icm	%r1,8,0(%r7)		# Te4[rk[11]>>24]<<24
	x	%r1,16(%r4)		# rk[12]=rk[4]^...
	st	%r1,48(%r4)
	x	%r1,20(%r4)		# rk[13]=rk[5]^rk[12]
	st	%r1,52(%r4)
	x	%r1,24(%r4)		# rk[14]=rk[6]^rk[13]
	st	%r1,56(%r4)
	x	%r1,28(%r4)		# rk[15]=rk[7]^rk[14]
	st	%r1,60(%r4)

	srlg	%r5,%r1,8
	srlg	%r6,%r1,16
	srlg	%r7,%r1,24
	nr	%r1,%r0
	nr	%r5,%r0
	nr	%r6,%r0

	la	%r4,32(%r4)		# key+=8
	la	%r3,4(%r3)		# i++
	j	.L256_loop

.Lminus1:
	lghi	%r2,-1
	br	%r14
.size	AES_set_encrypt_key,.-AES_set_encrypt_key

# void AES_set_decrypt_key(const unsigned char *in, int bits,
# 		 AES_KEY *key) {
.globl	AES_set_decrypt_key
.type	AES_set_decrypt_key,@function
.align	16
AES_set_decrypt_key:
	#st	%r4,4*4(%r15)	# I rely on AES_set_encrypt_key to
	st	%r14,14*4(%r15)	# save non-volatile registers and %r4!
	bras	%r14,_s390x_AES_set_encrypt_key
	#l	%r4,4*4(%r15)
	l	%r14,14*4(%r15)
	ltgr	%r2,%r2
	bnzr	%r14
	#l	%r0,240(%r4)
	lhi	%r1,16
	cr	%r0,%r1
	jl	.Lgo
	oill	%r0,S390X_DECRYPT	# set "decrypt" bit
	st	%r0,240(%r4)
	br	%r14
.align	16
.Lgo:	lgr	%r13,%r0	#llgf	%r13,240(%r4)
	la	%r5,0(%r4)
	sllg	%r6,%r13,4
	la	%r6,0(%r6,%r4)
	srl	%r13,1
	lghi	%r1,-16

.align	16
.Linv:	lmg	%r8,%r9,0(%r5)
	lmg	%r10,%r11,0(%r6)
	stmg	%r8,%r9,0(%r6)
	stmg	%r10,%r11,0(%r5)
	la	%r5,16(%r5)
	la	%r6,0(%r1,%r6)
	brct	%r13,.Linv
	llgf	%r13,240(%r4)
	aghi	%r13,-1
	sll	%r13,2	# (rounds-1)*4
	llilh	%r5,0x8080
	llilh	%r6,0x1b1b
	llilh	%r7,0xfefe
	oill	%r5,0x8080
	oill	%r6,0x1b1b
	oill	%r7,0xfefe

.align	16
.Lmix:	l	%r8,16(%r4)	# tp1
	lr	%r9,%r8
	ngr	%r9,%r5
	srlg	%r1,%r9,7
	slr	%r9,%r1
	nr	%r9,%r6
	sllg	%r1,%r8,1
	nr	%r1,%r7
	xr	%r9,%r1		# tp2

	lr	%r10,%r9
	ngr	%r10,%r5
	srlg	%r1,%r10,7
	slr	%r10,%r1
	nr	%r10,%r6
	sllg	%r1,%r9,1
	nr	%r1,%r7
	xr	%r10,%r1		# tp4

	lr	%r11,%r10
	ngr	%r11,%r5
	srlg	%r1,%r11,7
	slr	%r11,%r1
	nr	%r11,%r6
	sllg	%r1,%r10,1
	nr	%r1,%r7
	xr	%r11,%r1		# tp8

	xr	%r9,%r8		# tp2^tp1
	xr	%r10,%r8		# tp4^tp1
	rll	%r8,%r8,24	# = ROTATE(tp1,8)
	xr	%r10,%r11		# ^=tp8
	xr	%r8,%r9		# ^=tp2^tp1
	xr	%r9,%r11		# tp2^tp1^tp8
	xr	%r8,%r10		# ^=tp4^tp1^tp8
	rll	%r9,%r9,8
	rll	%r10,%r10,16
	xr	%r8,%r9		# ^= ROTATE(tp8^tp2^tp1,24)
	rll	%r11,%r11,24
	xr	%r8,%r10    	# ^= ROTATE(tp8^tp4^tp1,16)
	xr	%r8,%r11		# ^= ROTATE(tp8,8)

	st	%r8,16(%r4)
	la	%r4,4(%r4)
	brct	%r13,.Lmix

	lm	%r6,%r13,6*4(%r15)# as was saved by AES_set_encrypt_key!
	lghi	%r2,0
	br	%r14
.size	AES_set_decrypt_key,.-AES_set_decrypt_key
.globl	AES_cbc_encrypt
.type	AES_cbc_encrypt,@function
.align	16
AES_cbc_encrypt:
	xgr	%r3,%r4		# flip %r3 and %r4, out and len
	xgr	%r4,%r3
	xgr	%r3,%r4
	lhi	%r0,16
	cl	%r0,240(%r5)
	jh	.Lcbc_software

	lg	%r0,0(%r6)	# copy ivec
	lg	%r1,8(%r6)
	stmg	%r0,%r1,16(%r15)
	lmg	%r0,%r1,0(%r5)	# copy key, cover 256 bit
	stmg	%r0,%r1,32(%r15)
	lmg	%r0,%r1,16(%r5)
	stmg	%r0,%r1,48(%r15)
	l	%r0,240(%r5)	# load kmc code
	lghi	%r5,15		# res=len%16, len-=res;
	ngr	%r5,%r3
	slr	%r3,%r5
	la	%r1,16(%r15)	# parameter block - ivec || key
	jz	.Lkmc_truncated
	.long	0xb92f0042	# kmc %r4,%r2
	brc	1,.-4		# pay attention to "partial completion"
	ltr	%r5,%r5
	jnz	.Lkmc_truncated
.Lkmc_done:
	lmg	%r0,%r1,16(%r15)	# copy ivec to caller
	stg	%r0,0(%r6)
	stg	%r1,8(%r6)
	br	%r14
.align	16
.Lkmc_truncated:
	ahi	%r5,-1		# it's the way it's encoded in mvc
	tmll	%r0,S390X_DECRYPT
	jnz	.Lkmc_truncated_dec
	lghi	%r1,0
	stg	%r1,16*4(%r15)
	stg	%r1,16*4+8(%r15)
	bras	%r1,1f
	mvc	16*4(1,%r15),0(%r2)
1:	ex	%r5,0(%r1)
	la	%r1,16(%r15)	# restore parameter block
	la	%r2,16*4(%r15)
	lghi	%r3,16
	.long	0xb92f0042	# kmc %r4,%r2
	j	.Lkmc_done
.align	16
.Lkmc_truncated_dec:
	st	%r4,4*4(%r15)
	la	%r4,16*4(%r15)
	lghi	%r3,16
	.long	0xb92f0042	# kmc %r4,%r2
	l	%r4,4*4(%r15)
	bras	%r1,2f
	mvc	0(1,%r4),16*4(%r15)
2:	ex	%r5,0(%r1)
	j	.Lkmc_done
.align	16
.Lcbc_software:
	stm	%r5,%r14,5*4(%r15)
	lhi	%r0,0
	cl	%r0,96(%r15)
	je	.Lcbc_decrypt

	larl	%r12,AES_Te

	llgf	%r8,0(%r6)
	llgf	%r9,4(%r6)
	llgf	%r10,8(%r6)
	llgf	%r11,12(%r6)

	lghi	%r0,16
	slr	%r3,%r0
	brc	4,.Lcbc_enc_tail	# if borrow
.Lcbc_enc_loop:
	stm	%r2,%r4,2*4(%r15)
	x	%r8,0(%r2)
	x	%r9,4(%r2)
	x	%r10,8(%r2)
	x	%r11,12(%r2)
	lgr	%r4,%r5

	bras	%r14,_s390x_AES_encrypt

	lm	%r2,%r5,2*4(%r15)
	st	%r8,0(%r4)
	st	%r9,4(%r4)
	st	%r10,8(%r4)
	st	%r11,12(%r4)

	la	%r2,16(%r2)
	la	%r4,16(%r4)
	lghi	%r0,16
	ltr	%r3,%r3
	jz	.Lcbc_enc_done
	slr	%r3,%r0
	brc	4,.Lcbc_enc_tail	# if borrow
	j	.Lcbc_enc_loop
.align	16
.Lcbc_enc_done:
	l	%r6,6*4(%r15)
	st	%r8,0(%r6)
	st	%r9,4(%r6)
	st	%r10,8(%r6)
	st	%r11,12(%r6)

	lm	%r7,%r14,7*4(%r15)
	br	%r14

.align	16
.Lcbc_enc_tail:
	aghi	%r3,15
	lghi	%r0,0
	stg	%r0,16*4(%r15)
	stg	%r0,16*4+8(%r15)
	bras	%r1,3f
	mvc	16*4(1,%r15),0(%r2)
3:	ex	%r3,0(%r1)
	lghi	%r3,0
	la	%r2,16*4(%r15)
	j	.Lcbc_enc_loop

.align	16
.Lcbc_decrypt:
	larl	%r12,AES_Td

	lg	%r0,0(%r6)
	lg	%r1,8(%r6)
	stmg	%r0,%r1,16*4(%r15)

.Lcbc_dec_loop:
	stm	%r2,%r4,2*4(%r15)
	llgf	%r8,0(%r2)
	llgf	%r9,4(%r2)
	llgf	%r10,8(%r2)
	llgf	%r11,12(%r2)
	lgr	%r4,%r5

	bras	%r14,_s390x_AES_decrypt

	lm	%r2,%r5,2*4(%r15)
	sllg	%r8,%r8,32
	sllg	%r10,%r10,32
	lr	%r8,%r9
	lr	%r10,%r11

	lg	%r0,0(%r2)
	lg	%r1,8(%r2)
	xg	%r8,16*4(%r15)
	xg	%r10,16*4+8(%r15)
	lghi	%r9,16
	slr	%r3,%r9
	brc	4,.Lcbc_dec_tail	# if borrow
	brc	2,.Lcbc_dec_done	# if zero
	stg	%r8,0(%r4)
	stg	%r10,8(%r4)
	stmg	%r0,%r1,16*4(%r15)

	la	%r2,16(%r2)
	la	%r4,16(%r4)
	j	.Lcbc_dec_loop

.Lcbc_dec_done:
	stg	%r8,0(%r4)
	stg	%r10,8(%r4)
.Lcbc_dec_exit:
	lm	%r6,%r14,6*4(%r15)
	stmg	%r0,%r1,0(%r6)

	br	%r14

.align	16
.Lcbc_dec_tail:
	aghi	%r3,15
	stg	%r8,16*4(%r15)
	stg	%r10,16*4+8(%r15)
	bras	%r9,4f
	mvc	0(1,%r4),16*4(%r15)
4:	ex	%r3,0(%r9)
	j	.Lcbc_dec_exit
.size	AES_cbc_encrypt,.-AES_cbc_encrypt
.globl	AES_ctr32_encrypt
.type	AES_ctr32_encrypt,@function
.align	16
AES_ctr32_encrypt:
	xgr	%r3,%r4		# flip %r3 and %r4, %r4 and %r3
	xgr	%r4,%r3
	xgr	%r3,%r4
	llgfr	%r3,%r3	# safe in ctr32 subroutine even in 64-bit case
	l	%r0,240(%r5)
	lhi	%r1,16
	clr	%r0,%r1
	jl	.Lctr32_software

	st	%r10,10*4(%r15)
	st	%r11,11*4(%r15)

	clr	%r3,%r1		# does work even in 64-bit mode
	jle	.Lctr32_nokma		# kma is slower for <= 16 blocks

	larl	%r1,OPENSSL_s390xcap_P
	lr	%r10,%r0
	llihh	%r11,0x8000
	srlg	%r11,%r11,0(%r10)
	ng	%r11,S390X_KMA(%r1)		# check kma capability vector
	jz	.Lctr32_nokma

	lhi	%r1,-96-112
	lr	%r11,%r15
	la	%r15,0(%r1,%r15)			# prepare parameter block

	lhi	%r1,0x0600
	sllg	%r3,%r3,4
	or	%r0,%r1				# set HS and LAAD flags

	st	%r11,0(%r15)			# backchain
	la	%r1,96(%r15)

	lmg	%r10,%r11,0(%r5)			# copy key
	stg	%r10,96+80(%r15)
	stg	%r11,96+88(%r15)
	lmg	%r10,%r11,16(%r5)
	stg	%r10,96+96(%r15)
	stg	%r11,96+104(%r15)

	lmg	%r10,%r11,0(%r6)			# copy iv
	stg	%r10,96+64(%r15)
	ahi	%r11,-1				# kma requires counter-1
	stg	%r11,96+72(%r15)
	st	%r11,96+12(%r15)		# copy counter

	lghi	%r10,0				# no AAD
	lghi	%r11,0

	.long	0xb929a042	# kma %r4,%r10,%r2
	brc	1,.-4		# pay attention to "partial completion"

	stg	%r0,96+80(%r15)		# wipe key
	stg	%r0,96+88(%r15)
	stg	%r0,96+96(%r15)
	stg	%r0,96+104(%r15)
	la	%r15,96+112(%r15)

	lm	%r10,%r11,10*4(%r15)
	br	%r14

.align	16
.Lctr32_nokma:
	stm	%r6,%r9,6*4(%r15)

	slgr	%r4,%r2
	la	%r1,0(%r5)	# %r1 is permanent copy of %r5
	lg	%r5,0(%r6)	# load ivec
	lg	%r6,8(%r6)

	# prepare and allocate stack frame at the top of 4K page
	# with 1K reserved for eventual signal handling
	lghi	%r8,-1024-256-16# guarantee at least 256-bytes buffer
	lghi	%r9,-4096
	algr	%r8,%r15
	lgr	%r7,%r15
	ngr	%r8,%r9		# align at page boundary
	slgr	%r7,%r8		# total buffer size
	lgr	%r10,%r15
	lghi	%r9,1024+16	# sl[g]fi is extended-immediate facility
	slgr	%r7,%r9		# deduct reservation to get usable buffer size
	# buffer size is at lest 256 and at most 3072+256-16

	la	%r15,1024(%r8)	# alloca
	srlg	%r7,%r7,4	# convert bytes to blocks, minimum 16
	st	%r10,0(%r15)	# back-chain
	st	%r7,4(%r15)

	slgr	%r3,%r7
	brc	1,.Lctr32_hw_switch	# not zero, no borrow
	algr	%r7,%r3	# input is shorter than allocated buffer
	lghi	%r3,0
	st	%r7,4(%r15)

.Lctr32_hw_switch:
.Lctr32_km_loop:
	la	%r10,16(%r15)
	lgr	%r11,%r7
.Lctr32_km_prepare:
	stg	%r5,0(%r10)
	stg	%r6,8(%r10)
	la	%r10,16(%r10)
	ahi	%r6,1		# 32-bit increment, preserves upper half
	brct	%r11,.Lctr32_km_prepare

	la	%r8,16(%r15)	# inp
	sllg	%r9,%r7,4	# len
	la	%r10,16(%r15)	# out
	.long	0xb92e00a8	# km %r10,%r8
	brc	1,.-4		# pay attention to "partial completion"

	la	%r10,16(%r15)
	lgr	%r11,%r7
	slgr	%r10,%r2
.Lctr32_km_xor:
	lg	%r8,0(%r2)
	lg	%r9,8(%r2)
	xg	%r8,0(%r10,%r2)
	xg	%r9,8(%r10,%r2)
	stg	%r8,0(%r4,%r2)
	stg	%r9,8(%r4,%r2)
	la	%r2,16(%r2)
	brct	%r11,.Lctr32_km_xor

	slgr	%r3,%r7
	brc	1,.Lctr32_km_loop	# not zero, no borrow
	algr	%r7,%r3
	lghi	%r3,0
	brc	4+1,.Lctr32_km_loop	# not zero

	l	%r8,0(%r15)
	l	%r9,4(%r15)
	la	%r10,16(%r15)
.Lctr32_km_zap:
	stg	%r8,0(%r10)
	stg	%r8,8(%r10)
	la	%r10,16(%r10)
	brct	%r9,.Lctr32_km_zap

	la	%r15,0(%r8)
	lm	%r6,%r11,6*4(%r15)
	br	%r14
.align	16
.Lctr32_software:
	stm	%r5,%r14,5*4(%r15)
	slr	%r2,%r4
	larl	%r12,AES_Te
	llgf	%r1,12(%r6)

.Lctr32_loop:
	stm	%r2,%r4,2*4(%r15)
	llgf	%r8,0(%r6)
	llgf	%r9,4(%r6)
	llgf	%r10,8(%r6)
	lgr	%r11,%r1
	st	%r1,16*4(%r15)
	lgr	%r4,%r5

	bras	%r14,_s390x_AES_encrypt

	lm	%r2,%r6,2*4(%r15)
	llgf	%r1,16*4(%r15)
	x	%r8,0(%r2,%r4)
	x	%r9,4(%r2,%r4)
	x	%r10,8(%r2,%r4)
	x	%r11,12(%r2,%r4)
	stm	%r8,%r11,0(%r4)

	la	%r4,16(%r4)
	ahi	%r1,1		# 32-bit increment
	brct	%r3,.Lctr32_loop

	lm	%r6,%r14,6*4(%r15)
	br	%r14
.size	AES_ctr32_encrypt,.-AES_ctr32_encrypt
.type	_s390x_xts_km,@function
.align	16
_s390x_xts_km:
	llgfr	%r8,%r0			# put aside the function code
	lghi	%r9,0x7f
	nr	%r9,%r0
	larl	%r1,OPENSSL_s390xcap_P
	llihh	%r0,0x8000
	srlg	%r0,%r0,32(%r9)		# check for 32+function code
	ng	%r0,S390X_KM(%r1)	# check km capability vector
	lgr	%r0,%r8			# restore the function code
	la	%r1,0(%r5)		# restore %r5
	jz	.Lxts_km_vanilla

	lmg	%r6,%r7,80(%r15)	# put aside the tweak value
	algr	%r4,%r2

	oill	%r0,32			# switch to xts function code
	aghi	%r9,-18			#
	sllg	%r9,%r9,3		# (function code - 18)*8, 0 or 16
	la	%r1,80-16(%r15)
	slgr	%r1,%r9			# parameter block position
	lmg	%r8,%r11,0(%r5)	# load 256 bits of key material,
	stmg	%r8,%r11,0(%r1)		# and copy it to parameter block.
					# yes, it contains junk and overlaps
					# with the tweak in 128-bit case.
					# it's done to avoid conditional
					# branch.
	stmg	%r6,%r7,80(%r15)	# "re-seat" the tweak value

	.long	0xb92e0042		# km %r4,%r2
	brc	1,.-4			# pay attention to "partial completion"

	lrvg	%r8,80+0(%r15)	# load the last tweak
	lrvg	%r9,80+8(%r15)
	stmg	%r0,%r3,80-32(%r15)	# wipe copy of the key

	nill	%r0,0xffdf		# switch back to original function code
	la	%r1,0(%r5)		# restore pointer to %r5
	slgr	%r4,%r2

	llgc	%r3,2*4-1(%r15)
	nill	%r3,0x0f		# %r3%=16
	br	%r14

.align	16
.Lxts_km_vanilla:
	# prepare and allocate stack frame at the top of 4K page
	# with 1K reserved for eventual signal handling
	lghi	%r8,-1024-256-16# guarantee at least 256-bytes buffer
	lghi	%r9,-4096
	algr	%r8,%r15
	lgr	%r7,%r15
	ngr	%r8,%r9		# align at page boundary
	slgr	%r7,%r8		# total buffer size
	lgr	%r10,%r15
	lghi	%r9,1024+16	# sl[g]fi is extended-immediate facility
	slgr	%r7,%r9		# deduct reservation to get usable buffer size
	# buffer size is at lest 256 and at most 3072+256-16

	la	%r15,1024(%r8)	# alloca
	nill	%r7,0xfff0	# round to 16*n
	st	%r10,0(%r15)	# back-chain
	nill	%r3,0xfff0	# redundant
	st	%r7,4(%r15)

	slgr	%r3,%r7
	brc	1,.Lxts_km_go	# not zero, no borrow
	algr	%r7,%r3	# input is shorter than allocated buffer
	lghi	%r3,0
	st	%r7,4(%r15)

.Lxts_km_go:
	lrvg	%r8,80+0(%r10)	# load the tweak value in little-endian
	lrvg	%r9,80+8(%r10)

	la	%r10,16(%r15)		# vector of ascending tweak values
	slgr	%r10,%r2
	srlg	%r11,%r7,4
	j	.Lxts_km_start

.Lxts_km_loop:
	la	%r10,16(%r15)
	slgr	%r10,%r2
	srlg	%r11,%r7,4
.Lxts_km_prepare:
	lghi	%r5,0x87
	srag	%r6,%r9,63		# broadcast upper bit
	ngr	%r5,%r6			# rem
	algr	%r8,%r8
	alcgr	%r9,%r9
	xgr	%r8,%r5
.Lxts_km_start:
	lrvgr	%r5,%r8			# flip byte order
	lrvgr	%r6,%r9
	stg	%r5,0(%r10,%r2)
	stg	%r6,8(%r10,%r2)
	xg	%r5,0(%r2)
	xg	%r6,8(%r2)
	stg	%r5,0(%r4,%r2)
	stg	%r6,8(%r4,%r2)
	la	%r2,16(%r2)
	brct	%r11,.Lxts_km_prepare

	slgr	%r2,%r7		# rewind %r2
	la	%r10,0(%r4,%r2)
	lgr	%r11,%r7
	.long	0xb92e00aa		# km %r10,%r10
	brc	1,.-4			# pay attention to "partial completion"

	la	%r10,16(%r15)
	slgr	%r10,%r2
	srlg	%r11,%r7,4
.Lxts_km_xor:
	lg	%r5,0(%r4,%r2)
	lg	%r6,8(%r4,%r2)
	xg	%r5,0(%r10,%r2)
	xg	%r6,8(%r10,%r2)
	stg	%r5,0(%r4,%r2)
	stg	%r6,8(%r4,%r2)
	la	%r2,16(%r2)
	brct	%r11,.Lxts_km_xor

	slgr	%r3,%r7
	brc	1,.Lxts_km_loop		# not zero, no borrow
	algr	%r7,%r3
	lghi	%r3,0
	brc	4+1,.Lxts_km_loop	# not zero

	l	%r5,0(%r15)		# back-chain
	llgf	%r7,4(%r15)	# bytes used
	la	%r6,16(%r15)
	srlg	%r7,%r7,4
.Lxts_km_zap:
	stg	%r5,0(%r6)
	stg	%r5,8(%r6)
	la	%r6,16(%r6)
	brct	%r7,.Lxts_km_zap

	la	%r15,0(%r5)
	llgc	%r3,2*4-1(%r5)
	nill	%r3,0x0f		# %r3%=16
	bzr	%r14

	# generate one more tweak...
	lghi	%r5,0x87
	srag	%r6,%r9,63		# broadcast upper bit
	ngr	%r5,%r6			# rem
	algr	%r8,%r8
	alcgr	%r9,%r9
	xgr	%r8,%r5

	ltr	%r3,%r3		# clear zero flag
	br	%r14
.size	_s390x_xts_km,.-_s390x_xts_km

.globl	AES_xts_encrypt
.type	AES_xts_encrypt,@function
.align	16
AES_xts_encrypt:
	xgr	%r3,%r4			# flip %r3 and %r4, %r4 and %r3
	xgr	%r4,%r3
	xgr	%r3,%r4
	llgfr	%r3,%r3
	st	%r3,1*4(%r15)	# save copy of %r3
	srag	%r3,%r3,4		# formally wrong, because it expands
					# sign byte, but who can afford asking
					# to process more than 2^63-1 bytes?
					# I use it, because it sets condition
					# code...
	bcr	8,%r14			# abort if zero (i.e. less than 16)
	llgf	%r0,240(%r6)
	lhi	%r1,16
	clr	%r0,%r1
	jl	.Lxts_enc_software

	st	%r14,5*4(%r15)
	stm	%r6,%r11,6*4(%r15)

	sllg	%r3,%r3,4		# %r3&=~15
	slgr	%r4,%r2

	# generate the tweak value
	l	%r11,96(%r15)	# pointer to iv
	la	%r10,80(%r15)
	lmg	%r8,%r9,0(%r11)
	lghi	%r11,16
	stmg	%r8,%r9,0(%r10)
	la	%r1,0(%r6)		# %r6 is not needed anymore
	.long	0xb92e00aa		# km %r10,%r10, generate the tweak
	brc	1,.-4			# can this happen?

	l	%r0,240(%r5)
	la	%r1,0(%r5)		# %r5 is not needed anymore
	bras	%r14,_s390x_xts_km
	jz	.Lxts_enc_km_done

	aghi	%r2,-16		# take one step back
	la	%r7,0(%r4,%r2)	# put aside real %r4
.Lxts_enc_km_steal:
	llgc	%r5,16(%r2)
	llgc	%r6,0(%r4,%r2)
	stc	%r5,0(%r4,%r2)
	stc	%r6,16(%r4,%r2)
	la	%r2,1(%r2)
	brct	%r3,.Lxts_enc_km_steal

	la	%r10,0(%r7)
	lghi	%r11,16
	lrvgr	%r5,%r8			# flip byte order
	lrvgr	%r6,%r9
	xg	%r5,0(%r10)
	xg	%r6,8(%r10)
	stg	%r5,0(%r10)
	stg	%r6,8(%r10)
	.long	0xb92e00aa		# km %r10,%r10
	brc	1,.-4			# can this happen?
	lrvgr	%r5,%r8			# flip byte order
	lrvgr	%r6,%r9
	xg	%r5,0(%r7)
	xg	%r6,8(%r7)
	stg	%r5,0(%r7)
	stg	%r6,8(%r7)

.Lxts_enc_km_done:
	stg	%r15,80+0(%r15)	# wipe tweak
	stg	%r15,80+8(%r15)
	l	%r14,5*4(%r15)
	lm	%r6,%r11,6*4(%r15)
	br	%r14
.align	16
.Lxts_enc_software:
	stm	%r6,%r14,6*4(%r15)

	slgr	%r4,%r2

	l	%r11,96(%r15)	# ivp
	llgf	%r8,0(%r11)		# load iv
	llgf	%r9,4(%r11)
	llgf	%r10,8(%r11)
	llgf	%r11,12(%r11)
	stm	%r2,%r5,2*4(%r15)
	la	%r4,0(%r6)
	larl	%r12,AES_Te
	bras	%r14,_s390x_AES_encrypt	# generate the tweak
	lm	%r2,%r5,2*4(%r15)
	stm	%r8,%r11,80(%r15)	# save the tweak
	j	.Lxts_enc_enter

.align	16
.Lxts_enc_loop:
	lrvg	%r9,80+0(%r15)	# load the tweak in little-endian
	lrvg	%r11,80+8(%r15)
	lghi	%r1,0x87
	srag	%r0,%r11,63		# broadcast upper bit
	ngr	%r1,%r0			# rem
	algr	%r9,%r9
	alcgr	%r11,%r11
	xgr	%r9,%r1
	lrvgr	%r9,%r9			# flip byte order
	lrvgr	%r11,%r11
	srlg	%r8,%r9,32		# smash the tweak to 4x32-bits
	stg	%r9,80+0(%r15)	# save the tweak
	llgfr	%r9,%r9
	srlg	%r10,%r11,32
	stg	%r11,80+8(%r15)
	llgfr	%r11,%r11
	la	%r2,16(%r2)		# %r2+=16
.Lxts_enc_enter:
	x	%r8,0(%r2)		# ^=*(%r2)
	x	%r9,4(%r2)
	x	%r10,8(%r2)
	x	%r11,12(%r2)
	stm	%r2,%r3,2*4(%r15)	# only two registers are changing
	la	%r4,0(%r5)
	bras	%r14,_s390x_AES_encrypt
	lm	%r2,%r5,2*4(%r15)
	x	%r8,80+0(%r15)	# ^=tweak
	x	%r9,80+4(%r15)
	x	%r10,80+8(%r15)
	x	%r11,80+12(%r15)
	st	%r8,0(%r4,%r2)
	st	%r9,4(%r4,%r2)
	st	%r10,8(%r4,%r2)
	st	%r11,12(%r4,%r2)
	brct	%r3,.Lxts_enc_loop

	llgc	%r3,7(%r15)
	nill	%r3,0x0f		# %r3%16
	jz	.Lxts_enc_done

	la	%r7,0(%r2,%r4)	# put aside real %r4
.Lxts_enc_steal:
	llgc	%r0,16(%r2)
	llgc	%r1,0(%r4,%r2)
	stc	%r0,0(%r4,%r2)
	stc	%r1,16(%r4,%r2)
	la	%r2,1(%r2)
	brct	%r3,.Lxts_enc_steal
	la	%r4,0(%r7)		# restore real %r4

	# generate last tweak...
	lrvg	%r9,80+0(%r15)	# load the tweak in little-endian
	lrvg	%r11,80+8(%r15)
	lghi	%r1,0x87
	srag	%r0,%r11,63		# broadcast upper bit
	ngr	%r1,%r0			# rem
	algr	%r9,%r9
	alcgr	%r11,%r11
	xgr	%r9,%r1
	lrvgr	%r9,%r9			# flip byte order
	lrvgr	%r11,%r11
	srlg	%r8,%r9,32		# smash the tweak to 4x32-bits
	stg	%r9,80+0(%r15)	# save the tweak
	llgfr	%r9,%r9
	srlg	%r10,%r11,32
	stg	%r11,80+8(%r15)
	llgfr	%r11,%r11

	x	%r8,0(%r4)		# ^=*(inp)|stolen cipther-text
	x	%r9,4(%r4)
	x	%r10,8(%r4)
	x	%r11,12(%r4)
	st	%r4,4*4(%r15)
	la	%r4,0(%r5)
	bras	%r14,_s390x_AES_encrypt
	l	%r4,4*4(%r15)
	x	%r8,80(%r15)	# ^=tweak
	x	%r9,84(%r15)
	x	%r10,88(%r15)
	x	%r11,92(%r15)
	st	%r8,0(%r4)
	st	%r9,4(%r4)
	st	%r10,8(%r4)
	st	%r11,12(%r4)

.Lxts_enc_done:
	stg	%r15,80+0(%r15)	# wipe tweak
	stg	%r15,+8(%r15)
	lm	%r6,%r14,6*4(%r15)
	br	%r14
.size	AES_xts_encrypt,.-AES_xts_encrypt
.globl	AES_xts_decrypt
.type	AES_xts_decrypt,@function
.align	16
AES_xts_decrypt:
	xgr	%r3,%r4			# flip %r3 and %r4, %r4 and %r3
	xgr	%r4,%r3
	xgr	%r3,%r4
	llgfr	%r3,%r3
	st	%r3,1*4(%r15)	# save copy of %r3
	aghi	%r3,-16
	bcr	4,%r14			# abort if less than zero. formally
					# wrong, because %r3 is unsigned,
					# but who can afford asking to
					# process more than 2^63-1 bytes?
	tmll	%r3,0x0f
	jnz	.Lxts_dec_proceed
	aghi	%r3,16
.Lxts_dec_proceed:
	llgf	%r0,240(%r6)
	lhi	%r1,16
	clr	%r0,%r1
	jl	.Lxts_dec_software

	st	%r14,5*4(%r15)
	stm	%r6,%r11,6*4(%r15)

	nill	%r3,0xfff0		# %r3&=~15
	slgr	%r4,%r2

	# generate the tweak value
	l	%r11,96(%r15)	# pointer to iv
	la	%r10,80(%r15)
	lmg	%r8,%r9,0(%r11)
	lghi	%r11,16
	stmg	%r8,%r9,0(%r10)
	la	%r1,0(%r6)		# %r6 is not needed past this point
	.long	0xb92e00aa		# km %r10,%r10, generate the tweak
	brc	1,.-4			# can this happen?

	l	%r0,240(%r5)
	la	%r1,0(%r5)		# %r5 is not needed anymore

	ltgr	%r3,%r3
	jz	.Lxts_dec_km_short
	bras	%r14,_s390x_xts_km
	jz	.Lxts_dec_km_done

	lrvgr	%r10,%r8			# make copy in reverse byte order
	lrvgr	%r11,%r9
	j	.Lxts_dec_km_2ndtweak

.Lxts_dec_km_short:
	llgc	%r3,7(%r15)
	nill	%r3,0x0f		# %r3%=16
	lrvg	%r8,80+0(%r15)	# load the tweak
	lrvg	%r9,80+8(%r15)
	lrvgr	%r10,%r8			# make copy in reverse byte order
	lrvgr	%r11,%r9

.Lxts_dec_km_2ndtweak:
	lghi	%r5,0x87
	srag	%r6,%r9,63		# broadcast upper bit
	ngr	%r5,%r6			# rem
	algr	%r8,%r8
	alcgr	%r9,%r9
	xgr	%r8,%r5
	lrvgr	%r5,%r8			# flip byte order
	lrvgr	%r6,%r9

	xg	%r5,0(%r2)
	xg	%r6,8(%r2)
	stg	%r5,0(%r4,%r2)
	stg	%r6,8(%r4,%r2)
	la	%r6,0(%r4,%r2)
	lghi	%r7,16
	.long	0xb92e0066		# km %r6,%r6
	brc	1,.-4			# can this happen?
	lrvgr	%r5,%r8
	lrvgr	%r6,%r9
	xg	%r5,0(%r4,%r2)
	xg	%r6,8(%r4,%r2)
	stg	%r5,0(%r4,%r2)
	stg	%r6,8(%r4,%r2)

	la	%r7,0(%r4,%r2)	# put aside real %r4
.Lxts_dec_km_steal:
	llgc	%r5,16(%r2)
	llgc	%r6,0(%r4,%r2)
	stc	%r5,0(%r4,%r2)
	stc	%r6,16(%r4,%r2)
	la	%r2,1(%r2)
	brct	%r3,.Lxts_dec_km_steal

	lgr	%r8,%r10
	lgr	%r9,%r11
	xg	%r8,0(%r7)
	xg	%r9,8(%r7)
	stg	%r8,0(%r7)
	stg	%r9,8(%r7)
	la	%r8,0(%r7)
	lghi	%r9,16
	.long	0xb92e0088		# km %r8,%r8
	brc	1,.-4			# can this happen?
	xg	%r10,0(%r7)
	xg	%r11,8(%r7)
	stg	%r10,0(%r7)
	stg	%r11,8(%r7)
.Lxts_dec_km_done:
	stg	%r15,80+0(%r15)	# wipe tweak
	stg	%r15,80+8(%r15)
	l	%r14,5*4(%r15)
	lm	%r6,%r11,6*4(%r15)
	br	%r14
.align	16
.Lxts_dec_software:
	stm	%r6,%r14,6*4(%r15)

	srlg	%r3,%r3,4
	slgr	%r4,%r2

	l	%r11,96(%r15)	# ivp
	llgf	%r8,0(%r11)		# load iv
	llgf	%r9,4(%r11)
	llgf	%r10,8(%r11)
	llgf	%r11,12(%r11)
	stm	%r2,%r5,2*4(%r15)
	la	%r4,0(%r6)
	larl	%r12,AES_Te
	bras	%r14,_s390x_AES_encrypt	# generate the tweak
	lm	%r2,%r5,2*4(%r15)
	larl	%r12,AES_Td
	ltr	%r3,%r3
	stm	%r8,%r11,80(%r15)	# save the tweak
	jz	.Lxts_dec_short
	j	.Lxts_dec_enter

.align	16
.Lxts_dec_loop:
	lrvg	%r9,80+0(%r15)	# load the tweak in little-endian
	lrvg	%r11,80+8(%r15)
	lghi	%r1,0x87
	srag	%r0,%r11,63		# broadcast upper bit
	ngr	%r1,%r0			# rem
	algr	%r9,%r9
	alcgr	%r11,%r11
	xgr	%r9,%r1
	lrvgr	%r9,%r9			# flip byte order
	lrvgr	%r11,%r11
	srlg	%r8,%r9,32		# smash the tweak to 4x32-bits
	stg	%r9,80+0(%r15)	# save the tweak
	llgfr	%r9,%r9
	srlg	%r10,%r11,32
	stg	%r11,80+8(%r15)
	llgfr	%r11,%r11
.Lxts_dec_enter:
	x	%r8,0(%r2)		# tweak^=*(inp)
	x	%r9,4(%r2)
	x	%r10,8(%r2)
	x	%r11,12(%r2)
	stm	%r2,%r3,2*4(%r15)	# only two registers are changing
	la	%r4,0(%r5)
	bras	%r14,_s390x_AES_decrypt
	lm	%r2,%r5,2*4(%r15)
	x	%r8,80+0(%r15)	# ^=tweak
	x	%r9,80+4(%r15)
	x	%r10,80+8(%r15)
	x	%r11,80+12(%r15)
	st	%r8,0(%r4,%r2)
	st	%r9,4(%r4,%r2)
	st	%r10,8(%r4,%r2)
	st	%r11,12(%r4,%r2)
	la	%r2,16(%r2)
	brct	%r3,.Lxts_dec_loop

	llgc	%r3,7(%r15)
	nill	%r3,0x0f		# %r3%16
	jz	.Lxts_dec_done

	# generate pair of tweaks...
	lrvg	%r9,80+0(%r15)	# load the tweak in little-endian
	lrvg	%r11,80+8(%r15)
	lghi	%r1,0x87
	srag	%r0,%r11,63		# broadcast upper bit
	ngr	%r1,%r0			# rem
	algr	%r9,%r9
	alcgr	%r11,%r11
	xgr	%r9,%r1
	lrvgr	%r6,%r9			# flip byte order
	lrvgr	%r7,%r11
	stmg	%r6,%r7,80(%r15)	# save the 1st tweak
	j	.Lxts_dec_2ndtweak

.align	16
.Lxts_dec_short:
	llgc	%r3,7(%r15)
	nill	%r3,0x0f		# %r3%16
	lrvg	%r9,80+0(%r15)	# load the tweak in little-endian
	lrvg	%r11,80+8(%r15)
.Lxts_dec_2ndtweak:
	lghi	%r1,0x87
	srag	%r0,%r11,63		# broadcast upper bit
	ngr	%r1,%r0			# rem
	algr	%r9,%r9
	alcgr	%r11,%r11
	xgr	%r9,%r1
	lrvgr	%r9,%r9			# flip byte order
	lrvgr	%r11,%r11
	srlg	%r8,%r9,32		# smash the tweak to 4x32-bits
	stg	%r9,80-16+0(%r15)	# save the 2nd tweak
	llgfr	%r9,%r9
	srlg	%r10,%r11,32
	stg	%r11,80-16+8(%r15)
	llgfr	%r11,%r11

	x	%r8,0(%r2)		# tweak_the_2nd^=*(inp)
	x	%r9,4(%r2)
	x	%r10,8(%r2)
	x	%r11,12(%r2)
	stm	%r2,%r3,2*4(%r15)
	la	%r4,0(%r5)
	bras	%r14,_s390x_AES_decrypt
	lm	%r2,%r5,2*4(%r15)
	x	%r8,80-16+0(%r15)	# ^=tweak_the_2nd
	x	%r9,80-16+4(%r15)
	x	%r10,80-16+8(%r15)
	x	%r11,80-16+12(%r15)
	st	%r8,0(%r4,%r2)
	st	%r9,4(%r4,%r2)
	st	%r10,8(%r4,%r2)
	st	%r11,12(%r4,%r2)

	la	%r7,0(%r4,%r2)	# put aside real %r4
.Lxts_dec_steal:
	llgc	%r0,16(%r2)
	llgc	%r1,0(%r4,%r2)
	stc	%r0,0(%r4,%r2)
	stc	%r1,16(%r4,%r2)
	la	%r2,1(%r2)
	brct	%r3,.Lxts_dec_steal
	la	%r4,0(%r7)		# restore real %r4

	lm	%r8,%r11,80(%r15)	# load the 1st tweak
	x	%r8,0(%r4)		# tweak^=*(inp)|stolen cipher-text
	x	%r9,4(%r4)
	x	%r10,8(%r4)
	x	%r11,12(%r4)
	st	%r4,4*4(%r15)
	la	%r4,0(%r5)
	bras	%r14,_s390x_AES_decrypt
	l	%r4,4*4(%r15)
	x	%r8,80+0(%r15)	# ^=tweak
	x	%r9,80+4(%r15)
	x	%r10,80+8(%r15)
	x	%r11,80+12(%r15)
	st	%r8,0(%r4)
	st	%r9,4(%r4)
	st	%r10,8(%r4)
	st	%r11,12(%r4)
	stg	%r15,80-16+0(%r15)	# wipe 2nd tweak
	stg	%r15,80-16+8(%r15)
.Lxts_dec_done:
	stg	%r15,80+0(%r15)	# wipe tweak
	stg	%r15,+8(%r15)
	lm	%r6,%r14,6*4(%r15)
	br	%r14
.size	AES_xts_decrypt,.-AES_xts_decrypt
.string	"AES for s390x, CRYPTOGAMS by <appro@openssl.org>"
