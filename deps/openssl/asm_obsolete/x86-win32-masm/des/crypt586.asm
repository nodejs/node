TITLE	crypt586.asm
IF @Version LT 800
ECHO MASM version 8.00 or later is strongly recommended.
ENDIF
.686
.MODEL	FLAT
OPTION	DOTNAME
IF @Version LT 800
.text$	SEGMENT PAGE 'CODE'
ELSE
.text$	SEGMENT ALIGN(64) 'CODE'
ENDIF
EXTERN	_DES_SPtrans:NEAR
ALIGN	16
_fcrypt_body	PROC PUBLIC
$L_fcrypt_body_begin::
	push	ebp
	push	ebx
	push	esi
	push	edi
	;
	; Load the 2 words
	xor	edi,edi
	xor	esi,esi
	lea	edx,DWORD PTR _DES_SPtrans
	push	edx
	mov	ebp,DWORD PTR 28[esp]
	push	25
$L000start:
	;
	; Round 0
	mov	eax,DWORD PTR 36[esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR [ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 4[ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	edi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 1
	mov	eax,DWORD PTR 36[esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 8[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 12[ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	esi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 2
	mov	eax,DWORD PTR 36[esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 16[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 20[ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	edi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 3
	mov	eax,DWORD PTR 36[esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 24[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 28[ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	esi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 4
	mov	eax,DWORD PTR 36[esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 32[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 36[ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	edi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 5
	mov	eax,DWORD PTR 36[esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 40[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 44[ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	esi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 6
	mov	eax,DWORD PTR 36[esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 48[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 52[ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	edi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 7
	mov	eax,DWORD PTR 36[esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 56[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 60[ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	esi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 8
	mov	eax,DWORD PTR 36[esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 64[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 68[ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	edi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 9
	mov	eax,DWORD PTR 36[esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 72[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 76[ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	esi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 10
	mov	eax,DWORD PTR 36[esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 80[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 84[ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	edi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 11
	mov	eax,DWORD PTR 36[esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 88[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 92[ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	esi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 12
	mov	eax,DWORD PTR 36[esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 96[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 100[ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	edi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 13
	mov	eax,DWORD PTR 36[esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 104[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 108[ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	esi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 14
	mov	eax,DWORD PTR 36[esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 112[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 116[ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	edi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD PTR 32[esp]
	;
	; Round 15
	mov	eax,DWORD PTR 36[esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD PTR 40[esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD PTR 120[ebp]
	xor	eax,ebx
	mov	ecx,DWORD PTR 124[ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0fcfcfcfch
	xor	ebx,ebx
	and	edx,0cfcfcfcfh
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD PTR 4[esp]
	xor	esi,DWORD PTR [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD PTR 0200h[ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD PTR 0100h[ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD PTR 0300h[ecx*1+ebp]
	mov	cl,dh
	and	eax,0ffh
	and	edx,0ffh
	mov	ebx,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0700h[ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD PTR 0500h[edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD PTR 32[esp]
	mov	ebx,DWORD PTR [esp]
	mov	eax,edi
	dec	ebx
	mov	edi,esi
	mov	esi,eax
	mov	DWORD PTR [esp],ebx
	jnz	$L000start
	;
	; FP
	mov	edx,DWORD PTR 28[esp]
	ror	edi,1
	mov	eax,esi
	xor	esi,edi
	and	esi,0aaaaaaaah
	xor	eax,esi
	xor	edi,esi
	;
	rol	eax,23
	mov	esi,eax
	xor	eax,edi
	and	eax,003fc03fch
	xor	esi,eax
	xor	edi,eax
	;
	rol	esi,10
	mov	eax,esi
	xor	esi,edi
	and	esi,033333333h
	xor	eax,esi
	xor	edi,esi
	;
	rol	edi,18
	mov	esi,edi
	xor	edi,eax
	and	edi,0fff0000fh
	xor	esi,edi
	xor	eax,edi
	;
	rol	esi,12
	mov	edi,esi
	xor	esi,eax
	and	esi,0f0f0f0f0h
	xor	edi,esi
	xor	eax,esi
	;
	ror	eax,4
	mov	DWORD PTR [edx],eax
	mov	DWORD PTR 4[edx],edi
	add	esp,8
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
_fcrypt_body ENDP
.text$	ENDS
END
