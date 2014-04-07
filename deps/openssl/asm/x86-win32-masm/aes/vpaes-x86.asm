TITLE	vpaes-x86.asm
IF @Version LT 800
ECHO MASM version 8.00 or later is strongly recommended.
ENDIF
.686
.XMM
IF @Version LT 800
XMMWORD STRUCT 16
DQ	2 dup (?)
XMMWORD	ENDS
ENDIF

.MODEL	FLAT
OPTION	DOTNAME
IF @Version LT 800
.text$	SEGMENT PAGE 'CODE'
ELSE
.text$	SEGMENT ALIGN(64) 'CODE'
ENDIF
ALIGN	64
$L_vpaes_consts::
DD	218628480,235210255,168496130,67568393
DD	252381056,17041926,33884169,51187212
DD	252645135,252645135,252645135,252645135
DD	1512730624,3266504856,1377990664,3401244816
DD	830229760,1275146365,2969422977,3447763452
DD	3411033600,2979783055,338359620,2782886510
DD	4209124096,907596821,221174255,1006095553
DD	191964160,3799684038,3164090317,1589111125
DD	182528256,1777043520,2877432650,3265356744
DD	1874708224,3503451415,3305285752,363511674
DD	1606117888,3487855781,1093350906,2384367825
DD	197121,67569157,134941193,202313229
DD	67569157,134941193,202313229,197121
DD	134941193,202313229,197121,67569157
DD	202313229,197121,67569157,134941193
DD	33619971,100992007,168364043,235736079
DD	235736079,33619971,100992007,168364043
DD	168364043,235736079,33619971,100992007
DD	100992007,168364043,235736079,33619971
DD	50462976,117835012,185207048,252579084
DD	252314880,51251460,117574920,184942860
DD	184682752,252054788,50987272,118359308
DD	118099200,185467140,251790600,50727180
DD	2946363062,528716217,1300004225,1881839624
DD	1532713819,1532713819,1532713819,1532713819
DD	3602276352,4288629033,3737020424,4153884961
DD	1354558464,32357713,2958822624,3775749553
DD	1201988352,132424512,1572796698,503232858
DD	2213177600,1597421020,4103937655,675398315
DD	2749646592,4273543773,1511898873,121693092
DD	3040248576,1103263732,2871565598,1608280554
DD	2236667136,2588920351,482954393,64377734
DD	3069987328,291237287,2117370568,3650299247
DD	533321216,3573750986,2572112006,1401264716
DD	1339849704,2721158661,548607111,3445553514
DD	2128193280,3054596040,2183486460,1257083700
DD	655635200,1165381986,3923443150,2344132524
DD	190078720,256924420,290342170,357187870
DD	1610966272,2263057382,4103205268,309794674
DD	2592527872,2233205587,1335446729,3402964816
DD	3973531904,3225098121,3002836325,1918774430
DD	3870401024,2102906079,2284471353,4117666579
DD	617007872,1021508343,366931923,691083277
DD	2528395776,3491914898,2968704004,1613121270
DD	3445188352,3247741094,844474987,4093578302
DD	651481088,1190302358,1689581232,574775300
DD	4289380608,206939853,2555985458,2489840491
DD	2130264064,327674451,3566485037,3349835193
DD	2470714624,316102159,3636825756,3393945945
DB	86,101,99,116,111,114,32,80,101,114,109,117,116,97,116,105
DB	111,110,32,65,69,83,32,102,111,114,32,120,56,54,47,83
DB	83,83,69,51,44,32,77,105,107,101,32,72,97,109,98,117
DB	114,103,32,40,83,116,97,110,102,111,114,100,32,85,110,105
DB	118,101,114,115,105,116,121,41,0
ALIGN	64
ALIGN	16
__vpaes_preheat	PROC PRIVATE
	add	ebp,DWORD PTR [esp]
	movdqa	xmm7,XMMWORD PTR [ebp-48]
	movdqa	xmm6,XMMWORD PTR [ebp-16]
	ret
__vpaes_preheat ENDP
ALIGN	16
__vpaes_encrypt_core	PROC PRIVATE
	mov	ecx,16
	mov	eax,DWORD PTR 240[edx]
	movdqa	xmm1,xmm6
	movdqa	xmm2,XMMWORD PTR [ebp]
	pandn	xmm1,xmm0
	movdqu	xmm5,XMMWORD PTR [edx]
	psrld	xmm1,4
	pand	xmm0,xmm6
DB	102,15,56,0,208
	movdqa	xmm0,XMMWORD PTR 16[ebp]
DB	102,15,56,0,193
	pxor	xmm2,xmm5
	pxor	xmm0,xmm2
	add	edx,16
	lea	ebx,DWORD PTR 192[ebp]
	jmp	$L000enc_entry
ALIGN	16
$L001enc_loop:
	movdqa	xmm4,XMMWORD PTR 32[ebp]
DB	102,15,56,0,226
	pxor	xmm4,xmm5
	movdqa	xmm0,XMMWORD PTR 48[ebp]
DB	102,15,56,0,195
	pxor	xmm0,xmm4
	movdqa	xmm5,XMMWORD PTR 64[ebp]
DB	102,15,56,0,234
	movdqa	xmm1,XMMWORD PTR [ecx*1+ebx-64]
	movdqa	xmm2,XMMWORD PTR 80[ebp]
DB	102,15,56,0,211
	pxor	xmm2,xmm5
	movdqa	xmm4,XMMWORD PTR [ecx*1+ebx]
	movdqa	xmm3,xmm0
DB	102,15,56,0,193
	add	edx,16
	pxor	xmm0,xmm2
DB	102,15,56,0,220
	add	ecx,16
	pxor	xmm3,xmm0
DB	102,15,56,0,193
	and	ecx,48
	pxor	xmm0,xmm3
	sub	eax,1
$L000enc_entry:
	movdqa	xmm1,xmm6
	pandn	xmm1,xmm0
	psrld	xmm1,4
	pand	xmm0,xmm6
	movdqa	xmm5,XMMWORD PTR [ebp-32]
DB	102,15,56,0,232
	pxor	xmm0,xmm1
	movdqa	xmm3,xmm7
DB	102,15,56,0,217
	pxor	xmm3,xmm5
	movdqa	xmm4,xmm7
DB	102,15,56,0,224
	pxor	xmm4,xmm5
	movdqa	xmm2,xmm7
DB	102,15,56,0,211
	pxor	xmm2,xmm0
	movdqa	xmm3,xmm7
	movdqu	xmm5,XMMWORD PTR [edx]
DB	102,15,56,0,220
	pxor	xmm3,xmm1
	jnz	$L001enc_loop
	movdqa	xmm4,XMMWORD PTR 96[ebp]
	movdqa	xmm0,XMMWORD PTR 112[ebp]
DB	102,15,56,0,226
	pxor	xmm4,xmm5
DB	102,15,56,0,195
	movdqa	xmm1,XMMWORD PTR 64[ecx*1+ebx]
	pxor	xmm0,xmm4
DB	102,15,56,0,193
	ret
__vpaes_encrypt_core ENDP
ALIGN	16
__vpaes_decrypt_core	PROC PRIVATE
	mov	eax,DWORD PTR 240[edx]
	lea	ebx,DWORD PTR 608[ebp]
	movdqa	xmm1,xmm6
	movdqa	xmm2,XMMWORD PTR [ebx-64]
	pandn	xmm1,xmm0
	mov	ecx,eax
	psrld	xmm1,4
	movdqu	xmm5,XMMWORD PTR [edx]
	shl	ecx,4
	pand	xmm0,xmm6
DB	102,15,56,0,208
	movdqa	xmm0,XMMWORD PTR [ebx-48]
	xor	ecx,48
DB	102,15,56,0,193
	and	ecx,48
	pxor	xmm2,xmm5
	movdqa	xmm5,XMMWORD PTR 176[ebp]
	pxor	xmm0,xmm2
	add	edx,16
	lea	ecx,DWORD PTR [ecx*1+ebx-352]
	jmp	$L002dec_entry
ALIGN	16
$L003dec_loop:
	movdqa	xmm4,XMMWORD PTR [ebx-32]
DB	102,15,56,0,226
	pxor	xmm4,xmm0
	movdqa	xmm0,XMMWORD PTR [ebx-16]
DB	102,15,56,0,195
	pxor	xmm0,xmm4
	add	edx,16
DB	102,15,56,0,197
	movdqa	xmm4,XMMWORD PTR [ebx]
DB	102,15,56,0,226
	pxor	xmm4,xmm0
	movdqa	xmm0,XMMWORD PTR 16[ebx]
DB	102,15,56,0,195
	pxor	xmm0,xmm4
	sub	eax,1
DB	102,15,56,0,197
	movdqa	xmm4,XMMWORD PTR 32[ebx]
DB	102,15,56,0,226
	pxor	xmm4,xmm0
	movdqa	xmm0,XMMWORD PTR 48[ebx]
DB	102,15,56,0,195
	pxor	xmm0,xmm4
DB	102,15,56,0,197
	movdqa	xmm4,XMMWORD PTR 64[ebx]
DB	102,15,56,0,226
	pxor	xmm4,xmm0
	movdqa	xmm0,XMMWORD PTR 80[ebx]
DB	102,15,56,0,195
	pxor	xmm0,xmm4
DB	102,15,58,15,237,12
$L002dec_entry:
	movdqa	xmm1,xmm6
	pandn	xmm1,xmm0
	psrld	xmm1,4
	pand	xmm0,xmm6
	movdqa	xmm2,XMMWORD PTR [ebp-32]
DB	102,15,56,0,208
	pxor	xmm0,xmm1
	movdqa	xmm3,xmm7
DB	102,15,56,0,217
	pxor	xmm3,xmm2
	movdqa	xmm4,xmm7
DB	102,15,56,0,224
	pxor	xmm4,xmm2
	movdqa	xmm2,xmm7
DB	102,15,56,0,211
	pxor	xmm2,xmm0
	movdqa	xmm3,xmm7
DB	102,15,56,0,220
	pxor	xmm3,xmm1
	movdqu	xmm0,XMMWORD PTR [edx]
	jnz	$L003dec_loop
	movdqa	xmm4,XMMWORD PTR 96[ebx]
DB	102,15,56,0,226
	pxor	xmm4,xmm0
	movdqa	xmm0,XMMWORD PTR 112[ebx]
	movdqa	xmm2,XMMWORD PTR [ecx]
DB	102,15,56,0,195
	pxor	xmm0,xmm4
DB	102,15,56,0,194
	ret
__vpaes_decrypt_core ENDP
ALIGN	16
__vpaes_schedule_core	PROC PRIVATE
	add	ebp,DWORD PTR [esp]
	movdqu	xmm0,XMMWORD PTR [esi]
	movdqa	xmm2,XMMWORD PTR 320[ebp]
	movdqa	xmm3,xmm0
	lea	ebx,DWORD PTR [ebp]
	movdqa	XMMWORD PTR 4[esp],xmm2
	call	__vpaes_schedule_transform
	movdqa	xmm7,xmm0
	test	edi,edi
	jnz	$L004schedule_am_decrypting
	movdqu	XMMWORD PTR [edx],xmm0
	jmp	$L005schedule_go
$L004schedule_am_decrypting:
	movdqa	xmm1,XMMWORD PTR 256[ecx*1+ebp]
DB	102,15,56,0,217
	movdqu	XMMWORD PTR [edx],xmm3
	xor	ecx,48
$L005schedule_go:
	cmp	eax,192
	ja	$L006schedule_256
	je	$L007schedule_192
$L008schedule_128:
	mov	eax,10
$L009loop_schedule_128:
	call	__vpaes_schedule_round
	dec	eax
	jz	$L010schedule_mangle_last
	call	__vpaes_schedule_mangle
	jmp	$L009loop_schedule_128
ALIGN	16
$L007schedule_192:
	movdqu	xmm0,XMMWORD PTR 8[esi]
	call	__vpaes_schedule_transform
	movdqa	xmm6,xmm0
	pxor	xmm4,xmm4
	movhlps	xmm6,xmm4
	mov	eax,4
$L011loop_schedule_192:
	call	__vpaes_schedule_round
DB	102,15,58,15,198,8
	call	__vpaes_schedule_mangle
	call	__vpaes_schedule_192_smear
	call	__vpaes_schedule_mangle
	call	__vpaes_schedule_round
	dec	eax
	jz	$L010schedule_mangle_last
	call	__vpaes_schedule_mangle
	call	__vpaes_schedule_192_smear
	jmp	$L011loop_schedule_192
ALIGN	16
$L006schedule_256:
	movdqu	xmm0,XMMWORD PTR 16[esi]
	call	__vpaes_schedule_transform
	mov	eax,7
$L012loop_schedule_256:
	call	__vpaes_schedule_mangle
	movdqa	xmm6,xmm0
	call	__vpaes_schedule_round
	dec	eax
	jz	$L010schedule_mangle_last
	call	__vpaes_schedule_mangle
	pshufd	xmm0,xmm0,255
	movdqa	XMMWORD PTR 20[esp],xmm7
	movdqa	xmm7,xmm6
	call	$L_vpaes_schedule_low_round
	movdqa	xmm7,XMMWORD PTR 20[esp]
	jmp	$L012loop_schedule_256
ALIGN	16
$L010schedule_mangle_last:
	lea	ebx,DWORD PTR 384[ebp]
	test	edi,edi
	jnz	$L013schedule_mangle_last_dec
	movdqa	xmm1,XMMWORD PTR 256[ecx*1+ebp]
DB	102,15,56,0,193
	lea	ebx,DWORD PTR 352[ebp]
	add	edx,32
$L013schedule_mangle_last_dec:
	add	edx,-16
	pxor	xmm0,XMMWORD PTR 336[ebp]
	call	__vpaes_schedule_transform
	movdqu	XMMWORD PTR [edx],xmm0
	pxor	xmm0,xmm0
	pxor	xmm1,xmm1
	pxor	xmm2,xmm2
	pxor	xmm3,xmm3
	pxor	xmm4,xmm4
	pxor	xmm5,xmm5
	pxor	xmm6,xmm6
	pxor	xmm7,xmm7
	ret
__vpaes_schedule_core ENDP
ALIGN	16
__vpaes_schedule_192_smear	PROC PRIVATE
	pshufd	xmm0,xmm6,128
	pxor	xmm6,xmm0
	pshufd	xmm0,xmm7,254
	pxor	xmm6,xmm0
	movdqa	xmm0,xmm6
	pxor	xmm1,xmm1
	movhlps	xmm6,xmm1
	ret
__vpaes_schedule_192_smear ENDP
ALIGN	16
__vpaes_schedule_round	PROC PRIVATE
	movdqa	xmm2,XMMWORD PTR 8[esp]
	pxor	xmm1,xmm1
DB	102,15,58,15,202,15
DB	102,15,58,15,210,15
	pxor	xmm7,xmm1
	pshufd	xmm0,xmm0,255
DB	102,15,58,15,192,1
	movdqa	XMMWORD PTR 8[esp],xmm2
$L_vpaes_schedule_low_round::
	movdqa	xmm1,xmm7
	pslldq	xmm7,4
	pxor	xmm7,xmm1
	movdqa	xmm1,xmm7
	pslldq	xmm7,8
	pxor	xmm7,xmm1
	pxor	xmm7,XMMWORD PTR 336[ebp]
	movdqa	xmm4,XMMWORD PTR [ebp-16]
	movdqa	xmm5,XMMWORD PTR [ebp-48]
	movdqa	xmm1,xmm4
	pandn	xmm1,xmm0
	psrld	xmm1,4
	pand	xmm0,xmm4
	movdqa	xmm2,XMMWORD PTR [ebp-32]
DB	102,15,56,0,208
	pxor	xmm0,xmm1
	movdqa	xmm3,xmm5
DB	102,15,56,0,217
	pxor	xmm3,xmm2
	movdqa	xmm4,xmm5
DB	102,15,56,0,224
	pxor	xmm4,xmm2
	movdqa	xmm2,xmm5
DB	102,15,56,0,211
	pxor	xmm2,xmm0
	movdqa	xmm3,xmm5
DB	102,15,56,0,220
	pxor	xmm3,xmm1
	movdqa	xmm4,XMMWORD PTR 32[ebp]
DB	102,15,56,0,226
	movdqa	xmm0,XMMWORD PTR 48[ebp]
DB	102,15,56,0,195
	pxor	xmm0,xmm4
	pxor	xmm0,xmm7
	movdqa	xmm7,xmm0
	ret
__vpaes_schedule_round ENDP
ALIGN	16
__vpaes_schedule_transform	PROC PRIVATE
	movdqa	xmm2,XMMWORD PTR [ebp-16]
	movdqa	xmm1,xmm2
	pandn	xmm1,xmm0
	psrld	xmm1,4
	pand	xmm0,xmm2
	movdqa	xmm2,XMMWORD PTR [ebx]
DB	102,15,56,0,208
	movdqa	xmm0,XMMWORD PTR 16[ebx]
DB	102,15,56,0,193
	pxor	xmm0,xmm2
	ret
__vpaes_schedule_transform ENDP
ALIGN	16
__vpaes_schedule_mangle	PROC PRIVATE
	movdqa	xmm4,xmm0
	movdqa	xmm5,XMMWORD PTR 128[ebp]
	test	edi,edi
	jnz	$L014schedule_mangle_dec
	add	edx,16
	pxor	xmm4,XMMWORD PTR 336[ebp]
DB	102,15,56,0,229
	movdqa	xmm3,xmm4
DB	102,15,56,0,229
	pxor	xmm3,xmm4
DB	102,15,56,0,229
	pxor	xmm3,xmm4
	jmp	$L015schedule_mangle_both
ALIGN	16
$L014schedule_mangle_dec:
	movdqa	xmm2,XMMWORD PTR [ebp-16]
	lea	esi,DWORD PTR 416[ebp]
	movdqa	xmm1,xmm2
	pandn	xmm1,xmm4
	psrld	xmm1,4
	pand	xmm4,xmm2
	movdqa	xmm2,XMMWORD PTR [esi]
DB	102,15,56,0,212
	movdqa	xmm3,XMMWORD PTR 16[esi]
DB	102,15,56,0,217
	pxor	xmm3,xmm2
DB	102,15,56,0,221
	movdqa	xmm2,XMMWORD PTR 32[esi]
DB	102,15,56,0,212
	pxor	xmm2,xmm3
	movdqa	xmm3,XMMWORD PTR 48[esi]
DB	102,15,56,0,217
	pxor	xmm3,xmm2
DB	102,15,56,0,221
	movdqa	xmm2,XMMWORD PTR 64[esi]
DB	102,15,56,0,212
	pxor	xmm2,xmm3
	movdqa	xmm3,XMMWORD PTR 80[esi]
DB	102,15,56,0,217
	pxor	xmm3,xmm2
DB	102,15,56,0,221
	movdqa	xmm2,XMMWORD PTR 96[esi]
DB	102,15,56,0,212
	pxor	xmm2,xmm3
	movdqa	xmm3,XMMWORD PTR 112[esi]
DB	102,15,56,0,217
	pxor	xmm3,xmm2
	add	edx,-16
$L015schedule_mangle_both:
	movdqa	xmm1,XMMWORD PTR 256[ecx*1+ebp]
DB	102,15,56,0,217
	add	ecx,-16
	and	ecx,48
	movdqu	XMMWORD PTR [edx],xmm3
	ret
__vpaes_schedule_mangle ENDP
ALIGN	16
_vpaes_set_encrypt_key	PROC PUBLIC
$L_vpaes_set_encrypt_key_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	lea	ebx,DWORD PTR [esp-56]
	mov	eax,DWORD PTR 24[esp]
	and	ebx,-16
	mov	edx,DWORD PTR 28[esp]
	xchg	ebx,esp
	mov	DWORD PTR 48[esp],ebx
	mov	ebx,eax
	shr	ebx,5
	add	ebx,5
	mov	DWORD PTR 240[edx],ebx
	mov	ecx,48
	mov	edi,0
	mov	ebp,OFFSET ($L_vpaes_consts+030h-$L016pic_point)
	call	__vpaes_schedule_core
$L016pic_point:
	mov	esp,DWORD PTR 48[esp]
	xor	eax,eax
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_vpaes_set_encrypt_key ENDP
ALIGN	16
_vpaes_set_decrypt_key	PROC PUBLIC
$L_vpaes_set_decrypt_key_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	lea	ebx,DWORD PTR [esp-56]
	mov	eax,DWORD PTR 24[esp]
	and	ebx,-16
	mov	edx,DWORD PTR 28[esp]
	xchg	ebx,esp
	mov	DWORD PTR 48[esp],ebx
	mov	ebx,eax
	shr	ebx,5
	add	ebx,5
	mov	DWORD PTR 240[edx],ebx
	shl	ebx,4
	lea	edx,DWORD PTR 16[ebx*1+edx]
	mov	edi,1
	mov	ecx,eax
	shr	ecx,1
	and	ecx,32
	xor	ecx,32
	mov	ebp,OFFSET ($L_vpaes_consts+030h-$L017pic_point)
	call	__vpaes_schedule_core
$L017pic_point:
	mov	esp,DWORD PTR 48[esp]
	xor	eax,eax
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_vpaes_set_decrypt_key ENDP
ALIGN	16
_vpaes_encrypt	PROC PUBLIC
$L_vpaes_encrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ebp,OFFSET ($L_vpaes_consts+030h-$L018pic_point)
	call	__vpaes_preheat
$L018pic_point:
	mov	esi,DWORD PTR 20[esp]
	lea	ebx,DWORD PTR [esp-56]
	mov	edi,DWORD PTR 24[esp]
	and	ebx,-16
	mov	edx,DWORD PTR 28[esp]
	xchg	ebx,esp
	mov	DWORD PTR 48[esp],ebx
	movdqu	xmm0,XMMWORD PTR [esi]
	call	__vpaes_encrypt_core
	movdqu	XMMWORD PTR [edi],xmm0
	mov	esp,DWORD PTR 48[esp]
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_vpaes_encrypt ENDP
ALIGN	16
_vpaes_decrypt	PROC PUBLIC
$L_vpaes_decrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ebp,OFFSET ($L_vpaes_consts+030h-$L019pic_point)
	call	__vpaes_preheat
$L019pic_point:
	mov	esi,DWORD PTR 20[esp]
	lea	ebx,DWORD PTR [esp-56]
	mov	edi,DWORD PTR 24[esp]
	and	ebx,-16
	mov	edx,DWORD PTR 28[esp]
	xchg	ebx,esp
	mov	DWORD PTR 48[esp],ebx
	movdqu	xmm0,XMMWORD PTR [esi]
	call	__vpaes_decrypt_core
	movdqu	XMMWORD PTR [edi],xmm0
	mov	esp,DWORD PTR 48[esp]
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_vpaes_decrypt ENDP
ALIGN	16
_vpaes_cbc_encrypt	PROC PUBLIC
$L_vpaes_cbc_encrypt_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	esi,DWORD PTR 20[esp]
	mov	edi,DWORD PTR 24[esp]
	mov	eax,DWORD PTR 28[esp]
	mov	edx,DWORD PTR 32[esp]
	sub	eax,16
	jc	$L020cbc_abort
	lea	ebx,DWORD PTR [esp-56]
	mov	ebp,DWORD PTR 36[esp]
	and	ebx,-16
	mov	ecx,DWORD PTR 40[esp]
	xchg	ebx,esp
	movdqu	xmm1,XMMWORD PTR [ebp]
	sub	edi,esi
	mov	DWORD PTR 48[esp],ebx
	mov	DWORD PTR [esp],edi
	mov	DWORD PTR 4[esp],edx
	mov	DWORD PTR 8[esp],ebp
	mov	edi,eax
	mov	ebp,OFFSET ($L_vpaes_consts+030h-$L021pic_point)
	call	__vpaes_preheat
$L021pic_point:
	cmp	ecx,0
	je	$L022cbc_dec_loop
	jmp	$L023cbc_enc_loop
ALIGN	16
$L023cbc_enc_loop:
	movdqu	xmm0,XMMWORD PTR [esi]
	pxor	xmm0,xmm1
	call	__vpaes_encrypt_core
	mov	ebx,DWORD PTR [esp]
	mov	edx,DWORD PTR 4[esp]
	movdqa	xmm1,xmm0
	movdqu	XMMWORD PTR [esi*1+ebx],xmm0
	lea	esi,DWORD PTR 16[esi]
	sub	edi,16
	jnc	$L023cbc_enc_loop
	jmp	$L024cbc_done
ALIGN	16
$L022cbc_dec_loop:
	movdqu	xmm0,XMMWORD PTR [esi]
	movdqa	XMMWORD PTR 16[esp],xmm1
	movdqa	XMMWORD PTR 32[esp],xmm0
	call	__vpaes_decrypt_core
	mov	ebx,DWORD PTR [esp]
	mov	edx,DWORD PTR 4[esp]
	pxor	xmm0,XMMWORD PTR 16[esp]
	movdqa	xmm1,XMMWORD PTR 32[esp]
	movdqu	XMMWORD PTR [esi*1+ebx],xmm0
	lea	esi,DWORD PTR 16[esi]
	sub	edi,16
	jnc	$L022cbc_dec_loop
$L024cbc_done:
	mov	ebx,DWORD PTR 8[esp]
	mov	esp,DWORD PTR 48[esp]
	movdqu	XMMWORD PTR [ebx],xmm1
$L020cbc_abort:
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_vpaes_cbc_encrypt ENDP
.text$	ENDS
END
