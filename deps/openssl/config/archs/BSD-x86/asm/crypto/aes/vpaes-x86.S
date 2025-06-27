.text
.align	6,0x90
L_vpaes_consts:
.long	218628480,235210255,168496130,67568393
.long	252381056,17041926,33884169,51187212
.long	252645135,252645135,252645135,252645135
.long	1512730624,3266504856,1377990664,3401244816
.long	830229760,1275146365,2969422977,3447763452
.long	3411033600,2979783055,338359620,2782886510
.long	4209124096,907596821,221174255,1006095553
.long	191964160,3799684038,3164090317,1589111125
.long	182528256,1777043520,2877432650,3265356744
.long	1874708224,3503451415,3305285752,363511674
.long	1606117888,3487855781,1093350906,2384367825
.long	197121,67569157,134941193,202313229
.long	67569157,134941193,202313229,197121
.long	134941193,202313229,197121,67569157
.long	202313229,197121,67569157,134941193
.long	33619971,100992007,168364043,235736079
.long	235736079,33619971,100992007,168364043
.long	168364043,235736079,33619971,100992007
.long	100992007,168364043,235736079,33619971
.long	50462976,117835012,185207048,252579084
.long	252314880,51251460,117574920,184942860
.long	184682752,252054788,50987272,118359308
.long	118099200,185467140,251790600,50727180
.long	2946363062,528716217,1300004225,1881839624
.long	1532713819,1532713819,1532713819,1532713819
.long	3602276352,4288629033,3737020424,4153884961
.long	1354558464,32357713,2958822624,3775749553
.long	1201988352,132424512,1572796698,503232858
.long	2213177600,1597421020,4103937655,675398315
.long	2749646592,4273543773,1511898873,121693092
.long	3040248576,1103263732,2871565598,1608280554
.long	2236667136,2588920351,482954393,64377734
.long	3069987328,291237287,2117370568,3650299247
.long	533321216,3573750986,2572112006,1401264716
.long	1339849704,2721158661,548607111,3445553514
.long	2128193280,3054596040,2183486460,1257083700
.long	655635200,1165381986,3923443150,2344132524
.long	190078720,256924420,290342170,357187870
.long	1610966272,2263057382,4103205268,309794674
.long	2592527872,2233205587,1335446729,3402964816
.long	3973531904,3225098121,3002836325,1918774430
.long	3870401024,2102906079,2284471353,4117666579
.long	617007872,1021508343,366931923,691083277
.long	2528395776,3491914898,2968704004,1613121270
.long	3445188352,3247741094,844474987,4093578302
.long	651481088,1190302358,1689581232,574775300
.long	4289380608,206939853,2555985458,2489840491
.long	2130264064,327674451,3566485037,3349835193
.long	2470714624,316102159,3636825756,3393945945
.byte	86,101,99,116,111,114,32,80,101,114,109,117,116,97,116,105
.byte	111,110,32,65,69,83,32,102,111,114,32,120,56,54,47,83
.byte	83,83,69,51,44,32,77,105,107,101,32,72,97,109,98,117
.byte	114,103,32,40,83,116,97,110,102,111,114,100,32,85,110,105
.byte	118,101,114,115,105,116,121,41,0
.align	6,0x90
.type	__vpaes_preheat,@function
.align	4
__vpaes_preheat:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	addl	(%esp),%ebp
	movdqa	-48(%ebp),%xmm7
	movdqa	-16(%ebp),%xmm6
	ret
.type	__vpaes_encrypt_core,@function
.align	4
__vpaes_encrypt_core:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movl	$16,%ecx
	movl	240(%edx),%eax
	movdqa	%xmm6,%xmm1
	movdqa	(%ebp),%xmm2
	pandn	%xmm0,%xmm1
	pand	%xmm6,%xmm0
	movdqu	(%edx),%xmm5
.byte	102,15,56,0,208
	movdqa	16(%ebp),%xmm0
	pxor	%xmm5,%xmm2
	psrld	$4,%xmm1
	addl	$16,%edx
.byte	102,15,56,0,193
	leal	192(%ebp),%ebx
	pxor	%xmm2,%xmm0
	jmp	L000enc_entry
.align	4,0x90
L001enc_loop:
	movdqa	32(%ebp),%xmm4
	movdqa	48(%ebp),%xmm0
.byte	102,15,56,0,226
.byte	102,15,56,0,195
	pxor	%xmm5,%xmm4
	movdqa	64(%ebp),%xmm5
	pxor	%xmm4,%xmm0
	movdqa	-64(%ebx,%ecx,1),%xmm1
.byte	102,15,56,0,234
	movdqa	80(%ebp),%xmm2
	movdqa	(%ebx,%ecx,1),%xmm4
.byte	102,15,56,0,211
	movdqa	%xmm0,%xmm3
	pxor	%xmm5,%xmm2
.byte	102,15,56,0,193
	addl	$16,%edx
	pxor	%xmm2,%xmm0
.byte	102,15,56,0,220
	addl	$16,%ecx
	pxor	%xmm0,%xmm3
.byte	102,15,56,0,193
	andl	$48,%ecx
	subl	$1,%eax
	pxor	%xmm3,%xmm0
L000enc_entry:
	movdqa	%xmm6,%xmm1
	movdqa	-32(%ebp),%xmm5
	pandn	%xmm0,%xmm1
	psrld	$4,%xmm1
	pand	%xmm6,%xmm0
.byte	102,15,56,0,232
	movdqa	%xmm7,%xmm3
	pxor	%xmm1,%xmm0
.byte	102,15,56,0,217
	movdqa	%xmm7,%xmm4
	pxor	%xmm5,%xmm3
.byte	102,15,56,0,224
	movdqa	%xmm7,%xmm2
	pxor	%xmm5,%xmm4
.byte	102,15,56,0,211
	movdqa	%xmm7,%xmm3
	pxor	%xmm0,%xmm2
.byte	102,15,56,0,220
	movdqu	(%edx),%xmm5
	pxor	%xmm1,%xmm3
	jnz	L001enc_loop
	movdqa	96(%ebp),%xmm4
	movdqa	112(%ebp),%xmm0
.byte	102,15,56,0,226
	pxor	%xmm5,%xmm4
.byte	102,15,56,0,195
	movdqa	64(%ebx,%ecx,1),%xmm1
	pxor	%xmm4,%xmm0
.byte	102,15,56,0,193
	ret
.type	__vpaes_decrypt_core,@function
.align	4
__vpaes_decrypt_core:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	leal	608(%ebp),%ebx
	movl	240(%edx),%eax
	movdqa	%xmm6,%xmm1
	movdqa	-64(%ebx),%xmm2
	pandn	%xmm0,%xmm1
	movl	%eax,%ecx
	psrld	$4,%xmm1
	movdqu	(%edx),%xmm5
	shll	$4,%ecx
	pand	%xmm6,%xmm0
.byte	102,15,56,0,208
	movdqa	-48(%ebx),%xmm0
	xorl	$48,%ecx
.byte	102,15,56,0,193
	andl	$48,%ecx
	pxor	%xmm5,%xmm2
	movdqa	176(%ebp),%xmm5
	pxor	%xmm2,%xmm0
	addl	$16,%edx
	leal	-352(%ebx,%ecx,1),%ecx
	jmp	L002dec_entry
.align	4,0x90
L003dec_loop:
	movdqa	-32(%ebx),%xmm4
	movdqa	-16(%ebx),%xmm1
.byte	102,15,56,0,226
.byte	102,15,56,0,203
	pxor	%xmm4,%xmm0
	movdqa	(%ebx),%xmm4
	pxor	%xmm1,%xmm0
	movdqa	16(%ebx),%xmm1
.byte	102,15,56,0,226
.byte	102,15,56,0,197
.byte	102,15,56,0,203
	pxor	%xmm4,%xmm0
	movdqa	32(%ebx),%xmm4
	pxor	%xmm1,%xmm0
	movdqa	48(%ebx),%xmm1
.byte	102,15,56,0,226
.byte	102,15,56,0,197
.byte	102,15,56,0,203
	pxor	%xmm4,%xmm0
	movdqa	64(%ebx),%xmm4
	pxor	%xmm1,%xmm0
	movdqa	80(%ebx),%xmm1
.byte	102,15,56,0,226
.byte	102,15,56,0,197
.byte	102,15,56,0,203
	pxor	%xmm4,%xmm0
	addl	$16,%edx
.byte	102,15,58,15,237,12
	pxor	%xmm1,%xmm0
	subl	$1,%eax
L002dec_entry:
	movdqa	%xmm6,%xmm1
	movdqa	-32(%ebp),%xmm2
	pandn	%xmm0,%xmm1
	pand	%xmm6,%xmm0
	psrld	$4,%xmm1
.byte	102,15,56,0,208
	movdqa	%xmm7,%xmm3
	pxor	%xmm1,%xmm0
.byte	102,15,56,0,217
	movdqa	%xmm7,%xmm4
	pxor	%xmm2,%xmm3
.byte	102,15,56,0,224
	pxor	%xmm2,%xmm4
	movdqa	%xmm7,%xmm2
.byte	102,15,56,0,211
	movdqa	%xmm7,%xmm3
	pxor	%xmm0,%xmm2
.byte	102,15,56,0,220
	movdqu	(%edx),%xmm0
	pxor	%xmm1,%xmm3
	jnz	L003dec_loop
	movdqa	96(%ebx),%xmm4
.byte	102,15,56,0,226
	pxor	%xmm0,%xmm4
	movdqa	112(%ebx),%xmm0
	movdqa	(%ecx),%xmm2
.byte	102,15,56,0,195
	pxor	%xmm4,%xmm0
.byte	102,15,56,0,194
	ret
.type	__vpaes_schedule_core,@function
.align	4
__vpaes_schedule_core:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	addl	(%esp),%ebp
	movdqu	(%esi),%xmm0
	movdqa	320(%ebp),%xmm2
	movdqa	%xmm0,%xmm3
	leal	(%ebp),%ebx
	movdqa	%xmm2,4(%esp)
	call	__vpaes_schedule_transform
	movdqa	%xmm0,%xmm7
	testl	%edi,%edi
	jnz	L004schedule_am_decrypting
	movdqu	%xmm0,(%edx)
	jmp	L005schedule_go
L004schedule_am_decrypting:
	movdqa	256(%ebp,%ecx,1),%xmm1
.byte	102,15,56,0,217
	movdqu	%xmm3,(%edx)
	xorl	$48,%ecx
L005schedule_go:
	cmpl	$192,%eax
	ja	L006schedule_256
	je	L007schedule_192
L008schedule_128:
	movl	$10,%eax
L009loop_schedule_128:
	call	__vpaes_schedule_round
	decl	%eax
	jz	L010schedule_mangle_last
	call	__vpaes_schedule_mangle
	jmp	L009loop_schedule_128
.align	4,0x90
L007schedule_192:
	movdqu	8(%esi),%xmm0
	call	__vpaes_schedule_transform
	movdqa	%xmm0,%xmm6
	pxor	%xmm4,%xmm4
	movhlps	%xmm4,%xmm6
	movl	$4,%eax
L011loop_schedule_192:
	call	__vpaes_schedule_round
.byte	102,15,58,15,198,8
	call	__vpaes_schedule_mangle
	call	__vpaes_schedule_192_smear
	call	__vpaes_schedule_mangle
	call	__vpaes_schedule_round
	decl	%eax
	jz	L010schedule_mangle_last
	call	__vpaes_schedule_mangle
	call	__vpaes_schedule_192_smear
	jmp	L011loop_schedule_192
.align	4,0x90
L006schedule_256:
	movdqu	16(%esi),%xmm0
	call	__vpaes_schedule_transform
	movl	$7,%eax
L012loop_schedule_256:
	call	__vpaes_schedule_mangle
	movdqa	%xmm0,%xmm6
	call	__vpaes_schedule_round
	decl	%eax
	jz	L010schedule_mangle_last
	call	__vpaes_schedule_mangle
	pshufd	$255,%xmm0,%xmm0
	movdqa	%xmm7,20(%esp)
	movdqa	%xmm6,%xmm7
	call	L_vpaes_schedule_low_round
	movdqa	20(%esp),%xmm7
	jmp	L012loop_schedule_256
.align	4,0x90
L010schedule_mangle_last:
	leal	384(%ebp),%ebx
	testl	%edi,%edi
	jnz	L013schedule_mangle_last_dec
	movdqa	256(%ebp,%ecx,1),%xmm1
.byte	102,15,56,0,193
	leal	352(%ebp),%ebx
	addl	$32,%edx
L013schedule_mangle_last_dec:
	addl	$-16,%edx
	pxor	336(%ebp),%xmm0
	call	__vpaes_schedule_transform
	movdqu	%xmm0,(%edx)
	pxor	%xmm0,%xmm0
	pxor	%xmm1,%xmm1
	pxor	%xmm2,%xmm2
	pxor	%xmm3,%xmm3
	pxor	%xmm4,%xmm4
	pxor	%xmm5,%xmm5
	pxor	%xmm6,%xmm6
	pxor	%xmm7,%xmm7
	ret
.type	__vpaes_schedule_192_smear,@function
.align	4
__vpaes_schedule_192_smear:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	pshufd	$128,%xmm6,%xmm1
	pshufd	$254,%xmm7,%xmm0
	pxor	%xmm1,%xmm6
	pxor	%xmm1,%xmm1
	pxor	%xmm0,%xmm6
	movdqa	%xmm6,%xmm0
	movhlps	%xmm1,%xmm6
	ret
.type	__vpaes_schedule_round,@function
.align	4
__vpaes_schedule_round:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movdqa	8(%esp),%xmm2
	pxor	%xmm1,%xmm1
.byte	102,15,58,15,202,15
.byte	102,15,58,15,210,15
	pxor	%xmm1,%xmm7
	pshufd	$255,%xmm0,%xmm0
.byte	102,15,58,15,192,1
	movdqa	%xmm2,8(%esp)
L_vpaes_schedule_low_round:
	movdqa	%xmm7,%xmm1
	pslldq	$4,%xmm7
	pxor	%xmm1,%xmm7
	movdqa	%xmm7,%xmm1
	pslldq	$8,%xmm7
	pxor	%xmm1,%xmm7
	pxor	336(%ebp),%xmm7
	movdqa	-16(%ebp),%xmm4
	movdqa	-48(%ebp),%xmm5
	movdqa	%xmm4,%xmm1
	pandn	%xmm0,%xmm1
	psrld	$4,%xmm1
	pand	%xmm4,%xmm0
	movdqa	-32(%ebp),%xmm2
.byte	102,15,56,0,208
	pxor	%xmm1,%xmm0
	movdqa	%xmm5,%xmm3
.byte	102,15,56,0,217
	pxor	%xmm2,%xmm3
	movdqa	%xmm5,%xmm4
.byte	102,15,56,0,224
	pxor	%xmm2,%xmm4
	movdqa	%xmm5,%xmm2
.byte	102,15,56,0,211
	pxor	%xmm0,%xmm2
	movdqa	%xmm5,%xmm3
.byte	102,15,56,0,220
	pxor	%xmm1,%xmm3
	movdqa	32(%ebp),%xmm4
.byte	102,15,56,0,226
	movdqa	48(%ebp),%xmm0
.byte	102,15,56,0,195
	pxor	%xmm4,%xmm0
	pxor	%xmm7,%xmm0
	movdqa	%xmm0,%xmm7
	ret
.type	__vpaes_schedule_transform,@function
.align	4
__vpaes_schedule_transform:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movdqa	-16(%ebp),%xmm2
	movdqa	%xmm2,%xmm1
	pandn	%xmm0,%xmm1
	psrld	$4,%xmm1
	pand	%xmm2,%xmm0
	movdqa	(%ebx),%xmm2
.byte	102,15,56,0,208
	movdqa	16(%ebx),%xmm0
.byte	102,15,56,0,193
	pxor	%xmm2,%xmm0
	ret
.type	__vpaes_schedule_mangle,@function
.align	4
__vpaes_schedule_mangle:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	movdqa	%xmm0,%xmm4
	movdqa	128(%ebp),%xmm5
	testl	%edi,%edi
	jnz	L014schedule_mangle_dec
	addl	$16,%edx
	pxor	336(%ebp),%xmm4
.byte	102,15,56,0,229
	movdqa	%xmm4,%xmm3
.byte	102,15,56,0,229
	pxor	%xmm4,%xmm3
.byte	102,15,56,0,229
	pxor	%xmm4,%xmm3
	jmp	L015schedule_mangle_both
.align	4,0x90
L014schedule_mangle_dec:
	movdqa	-16(%ebp),%xmm2
	leal	416(%ebp),%esi
	movdqa	%xmm2,%xmm1
	pandn	%xmm4,%xmm1
	psrld	$4,%xmm1
	pand	%xmm2,%xmm4
	movdqa	(%esi),%xmm2
.byte	102,15,56,0,212
	movdqa	16(%esi),%xmm3
.byte	102,15,56,0,217
	pxor	%xmm2,%xmm3
.byte	102,15,56,0,221
	movdqa	32(%esi),%xmm2
.byte	102,15,56,0,212
	pxor	%xmm3,%xmm2
	movdqa	48(%esi),%xmm3
.byte	102,15,56,0,217
	pxor	%xmm2,%xmm3
.byte	102,15,56,0,221
	movdqa	64(%esi),%xmm2
.byte	102,15,56,0,212
	pxor	%xmm3,%xmm2
	movdqa	80(%esi),%xmm3
.byte	102,15,56,0,217
	pxor	%xmm2,%xmm3
.byte	102,15,56,0,221
	movdqa	96(%esi),%xmm2
.byte	102,15,56,0,212
	pxor	%xmm3,%xmm2
	movdqa	112(%esi),%xmm3
.byte	102,15,56,0,217
	pxor	%xmm2,%xmm3
	addl	$-16,%edx
L015schedule_mangle_both:
	movdqa	256(%ebp,%ecx,1),%xmm1
.byte	102,15,56,0,217
	addl	$-16,%ecx
	andl	$48,%ecx
	movdqu	%xmm3,(%edx)
	ret
.globl	_vpaes_set_encrypt_key
.type	_vpaes_set_encrypt_key,@function
.align	4
_vpaes_set_encrypt_key:
L_vpaes_set_encrypt_key_begin:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	20(%esp),%esi
	leal	-56(%esp),%ebx
	movl	24(%esp),%eax
	andl	$-16,%ebx
	movl	28(%esp),%edx
	xchgl	%esp,%ebx
	movl	%ebx,48(%esp)
	movl	%eax,%ebx
	shrl	$5,%ebx
	addl	$5,%ebx
	movl	%ebx,240(%edx)
	movl	$48,%ecx
	movl	$0,%edi
	leal	L_vpaes_consts+0x30-L016pic_point,%ebp
	call	__vpaes_schedule_core
L016pic_point:
	movl	48(%esp),%esp
	xorl	%eax,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_vpaes_set_decrypt_key
.type	_vpaes_set_decrypt_key,@function
.align	4
_vpaes_set_decrypt_key:
L_vpaes_set_decrypt_key_begin:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	20(%esp),%esi
	leal	-56(%esp),%ebx
	movl	24(%esp),%eax
	andl	$-16,%ebx
	movl	28(%esp),%edx
	xchgl	%esp,%ebx
	movl	%ebx,48(%esp)
	movl	%eax,%ebx
	shrl	$5,%ebx
	addl	$5,%ebx
	movl	%ebx,240(%edx)
	shll	$4,%ebx
	leal	16(%edx,%ebx,1),%edx
	movl	$1,%edi
	movl	%eax,%ecx
	shrl	$1,%ecx
	andl	$32,%ecx
	xorl	$32,%ecx
	leal	L_vpaes_consts+0x30-L017pic_point,%ebp
	call	__vpaes_schedule_core
L017pic_point:
	movl	48(%esp),%esp
	xorl	%eax,%eax
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_vpaes_encrypt
.type	_vpaes_encrypt,@function
.align	4
_vpaes_encrypt:
L_vpaes_encrypt_begin:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	leal	L_vpaes_consts+0x30-L018pic_point,%ebp
	call	__vpaes_preheat
L018pic_point:
	movl	20(%esp),%esi
	leal	-56(%esp),%ebx
	movl	24(%esp),%edi
	andl	$-16,%ebx
	movl	28(%esp),%edx
	xchgl	%esp,%ebx
	movl	%ebx,48(%esp)
	movdqu	(%esi),%xmm0
	call	__vpaes_encrypt_core
	movdqu	%xmm0,(%edi)
	movl	48(%esp),%esp
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_vpaes_decrypt
.type	_vpaes_decrypt,@function
.align	4
_vpaes_decrypt:
L_vpaes_decrypt_begin:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	leal	L_vpaes_consts+0x30-L019pic_point,%ebp
	call	__vpaes_preheat
L019pic_point:
	movl	20(%esp),%esi
	leal	-56(%esp),%ebx
	movl	24(%esp),%edi
	andl	$-16,%ebx
	movl	28(%esp),%edx
	xchgl	%esp,%ebx
	movl	%ebx,48(%esp)
	movdqu	(%esi),%xmm0
	call	__vpaes_decrypt_core
	movdqu	%xmm0,(%edi)
	movl	48(%esp),%esp
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
.globl	_vpaes_cbc_encrypt
.type	_vpaes_cbc_encrypt,@function
.align	4
_vpaes_cbc_encrypt:
L_vpaes_cbc_encrypt_begin:
	%ifdef __CET__

.byte	243,15,30,251
	%endif

	pushl	%ebp
	pushl	%ebx
	pushl	%esi
	pushl	%edi
	movl	20(%esp),%esi
	movl	24(%esp),%edi
	movl	28(%esp),%eax
	movl	32(%esp),%edx
	subl	$16,%eax
	jc	L020cbc_abort
	leal	-56(%esp),%ebx
	movl	36(%esp),%ebp
	andl	$-16,%ebx
	movl	40(%esp),%ecx
	xchgl	%esp,%ebx
	movdqu	(%ebp),%xmm1
	subl	%esi,%edi
	movl	%ebx,48(%esp)
	movl	%edi,(%esp)
	movl	%edx,4(%esp)
	movl	%ebp,8(%esp)
	movl	%eax,%edi
	leal	L_vpaes_consts+0x30-L021pic_point,%ebp
	call	__vpaes_preheat
L021pic_point:
	cmpl	$0,%ecx
	je	L022cbc_dec_loop
	jmp	L023cbc_enc_loop
.align	4,0x90
L023cbc_enc_loop:
	movdqu	(%esi),%xmm0
	pxor	%xmm1,%xmm0
	call	__vpaes_encrypt_core
	movl	(%esp),%ebx
	movl	4(%esp),%edx
	movdqa	%xmm0,%xmm1
	movdqu	%xmm0,(%ebx,%esi,1)
	leal	16(%esi),%esi
	subl	$16,%edi
	jnc	L023cbc_enc_loop
	jmp	L024cbc_done
.align	4,0x90
L022cbc_dec_loop:
	movdqu	(%esi),%xmm0
	movdqa	%xmm1,16(%esp)
	movdqa	%xmm0,32(%esp)
	call	__vpaes_decrypt_core
	movl	(%esp),%ebx
	movl	4(%esp),%edx
	pxor	16(%esp),%xmm0
	movdqa	32(%esp),%xmm1
	movdqu	%xmm0,(%ebx,%esi,1)
	leal	16(%esi),%esi
	subl	$16,%edi
	jnc	L022cbc_dec_loop
L024cbc_done:
	movl	8(%esp),%ebx
	movl	48(%esp),%esp
	movdqu	%xmm1,(%ebx)
L020cbc_abort:
	popl	%edi
	popl	%esi
	popl	%ebx
	popl	%ebp
	ret
