TITLE	des-586.asm
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
PUBLIC	_DES_SPtrans
ALIGN	16
__x86_DES_encrypt	PROC PRIVATE
	push	ecx
	; Round 0
	mov	eax,DWORD PTR [ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 4[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 1
	mov	eax,DWORD PTR 8[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 12[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 2
	mov	eax,DWORD PTR 16[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 20[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 3
	mov	eax,DWORD PTR 24[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 28[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 4
	mov	eax,DWORD PTR 32[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 36[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 5
	mov	eax,DWORD PTR 40[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 44[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 6
	mov	eax,DWORD PTR 48[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 52[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 7
	mov	eax,DWORD PTR 56[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 60[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 8
	mov	eax,DWORD PTR 64[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 68[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 9
	mov	eax,DWORD PTR 72[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 76[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 10
	mov	eax,DWORD PTR 80[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 84[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 11
	mov	eax,DWORD PTR 88[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 92[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 12
	mov	eax,DWORD PTR 96[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 100[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 13
	mov	eax,DWORD PTR 104[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 108[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 14
	mov	eax,DWORD PTR 112[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 116[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 15
	mov	eax,DWORD PTR 120[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 124[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	add	esp,4
	ret
__x86_DES_encrypt ENDP
ALIGN	16
__x86_DES_decrypt	PROC PRIVATE
	push	ecx
	; Round 15
	mov	eax,DWORD PTR 120[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 124[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 14
	mov	eax,DWORD PTR 112[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 116[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 13
	mov	eax,DWORD PTR 104[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 108[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 12
	mov	eax,DWORD PTR 96[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 100[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 11
	mov	eax,DWORD PTR 88[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 92[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 10
	mov	eax,DWORD PTR 80[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 84[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 9
	mov	eax,DWORD PTR 72[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 76[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 8
	mov	eax,DWORD PTR 64[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 68[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 7
	mov	eax,DWORD PTR 56[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 60[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 6
	mov	eax,DWORD PTR 48[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 52[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 5
	mov	eax,DWORD PTR 40[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 44[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 4
	mov	eax,DWORD PTR 32[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 36[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 3
	mov	eax,DWORD PTR 24[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 28[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 2
	mov	eax,DWORD PTR 16[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 20[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	; Round 1
	mov	eax,DWORD PTR 8[ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 12[ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	edi,DWORD PTR 0600h[ebx*1+ebp]
	xor	edi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	edi,DWORD PTR 0400h[eax*1+ebp]
	xor	edi,DWORD PTR 0500h[edx*1+ebp]
	; Round 0
	mov	eax,DWORD PTR [ecx]
	xor	ebx,ebx
	mov	edx,DWORD PTR 4[ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0fcfcfcfch
	and	edx,0cfcfcfcfh
	mov	bl,al
	mov	cl,ah
	ror	edx,4
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
	xor	esi,DWORD PTR 0600h[ebx*1+ebp]
	xor	esi,DWORD PTR 0700h[ecx*1+ebp]
	mov	ecx,DWORD PTR [esp]
	xor	esi,DWORD PTR 0400h[eax*1+ebp]
	xor	esi,DWORD PTR 0500h[edx*1+ebp]
	add	esp,4
	ret
__x86_DES_decrypt ENDP
ALIGN	16
_DES_encrypt1	PROC PUBLIC
$L_DES_encrypt1_begin::
	push	esi
	push	edi
	;
	; Load the 2 words
	mov	esi,DWORD PTR 12[esp]
	xor	ecx,ecx
	push	ebx
	push	ebp
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 28[esp]
	mov	edi,DWORD PTR 4[esi]
	;
	; IP
	rol	eax,4
	mov	esi,eax
	xor	eax,edi
	and	eax,0f0f0f0f0h
	xor	esi,eax
	xor	edi,eax
	;
	rol	edi,20
	mov	eax,edi
	xor	edi,esi
	and	edi,0fff0000fh
	xor	eax,edi
	xor	esi,edi
	;
	rol	eax,14
	mov	edi,eax
	xor	eax,esi
	and	eax,033333333h
	xor	edi,eax
	xor	esi,eax
	;
	rol	esi,22
	mov	eax,esi
	xor	esi,edi
	and	esi,003fc03fch
	xor	eax,esi
	xor	edi,esi
	;
	rol	eax,9
	mov	esi,eax
	xor	eax,edi
	and	eax,0aaaaaaaah
	xor	esi,eax
	xor	edi,eax
	;
	rol	edi,1
	call	$L000pic_point
$L000pic_point:
	pop	ebp
	lea	ebp,DWORD PTR ($Ldes_sptrans-$L000pic_point)[ebp]
	mov	ecx,DWORD PTR 24[esp]
	cmp	ebx,0
	je	$L001decrypt
	call	__x86_DES_encrypt
	jmp	$L002done
$L001decrypt:
	call	__x86_DES_decrypt
$L002done:
	;
	; FP
	mov	edx,DWORD PTR 20[esp]
	ror	esi,1
	mov	eax,edi
	xor	edi,esi
	and	edi,0aaaaaaaah
	xor	eax,edi
	xor	esi,edi
	;
	rol	eax,23
	mov	edi,eax
	xor	eax,esi
	and	eax,003fc03fch
	xor	edi,eax
	xor	esi,eax
	;
	rol	edi,10
	mov	eax,edi
	xor	edi,esi
	and	edi,033333333h
	xor	eax,edi
	xor	esi,edi
	;
	rol	esi,18
	mov	edi,esi
	xor	esi,eax
	and	esi,0fff0000fh
	xor	edi,esi
	xor	eax,esi
	;
	rol	edi,12
	mov	esi,edi
	xor	edi,eax
	and	edi,0f0f0f0f0h
	xor	esi,edi
	xor	eax,edi
	;
	ror	eax,4
	mov	DWORD PTR [edx],eax
	mov	DWORD PTR 4[edx],esi
	pop	ebp
	pop	ebx
	pop	edi
	pop	esi
	ret
_DES_encrypt1 ENDP
ALIGN	16
_DES_encrypt2	PROC PUBLIC
$L_DES_encrypt2_begin::
	push	esi
	push	edi
	;
	; Load the 2 words
	mov	eax,DWORD PTR 12[esp]
	xor	ecx,ecx
	push	ebx
	push	ebp
	mov	esi,DWORD PTR [eax]
	mov	ebx,DWORD PTR 28[esp]
	rol	esi,3
	mov	edi,DWORD PTR 4[eax]
	rol	edi,3
	call	$L003pic_point
$L003pic_point:
	pop	ebp
	lea	ebp,DWORD PTR ($Ldes_sptrans-$L003pic_point)[ebp]
	mov	ecx,DWORD PTR 24[esp]
	cmp	ebx,0
	je	$L004decrypt
	call	__x86_DES_encrypt
	jmp	$L005done
$L004decrypt:
	call	__x86_DES_decrypt
$L005done:
	;
	; Fixup
	ror	edi,3
	mov	eax,DWORD PTR 20[esp]
	ror	esi,3
	mov	DWORD PTR [eax],edi
	mov	DWORD PTR 4[eax],esi
	pop	ebp
	pop	ebx
	pop	edi
	pop	esi
	ret
_DES_encrypt2 ENDP
ALIGN	16
_DES_encrypt3	PROC PUBLIC
$L_DES_encrypt3_begin::
	push	ebx
	mov	ebx,DWORD PTR 8[esp]
	push	ebp
	push	esi
	push	edi
	;
	; Load the data words
	mov	edi,DWORD PTR [ebx]
	mov	esi,DWORD PTR 4[ebx]
	sub	esp,12
	;
	; IP
	rol	edi,4
	mov	edx,edi
	xor	edi,esi
	and	edi,0f0f0f0f0h
	xor	edx,edi
	xor	esi,edi
	;
	rol	esi,20
	mov	edi,esi
	xor	esi,edx
	and	esi,0fff0000fh
	xor	edi,esi
	xor	edx,esi
	;
	rol	edi,14
	mov	esi,edi
	xor	edi,edx
	and	edi,033333333h
	xor	esi,edi
	xor	edx,edi
	;
	rol	edx,22
	mov	edi,edx
	xor	edx,esi
	and	edx,003fc03fch
	xor	edi,edx
	xor	esi,edx
	;
	rol	edi,9
	mov	edx,edi
	xor	edi,esi
	and	edi,0aaaaaaaah
	xor	edx,edi
	xor	esi,edi
	;
	ror	edx,3
	ror	esi,2
	mov	DWORD PTR 4[ebx],esi
	mov	eax,DWORD PTR 36[esp]
	mov	DWORD PTR [ebx],edx
	mov	edi,DWORD PTR 40[esp]
	mov	esi,DWORD PTR 44[esp]
	mov	DWORD PTR 8[esp],1
	mov	DWORD PTR 4[esp],eax
	mov	DWORD PTR [esp],ebx
	call	$L_DES_encrypt2_begin
	mov	DWORD PTR 8[esp],0
	mov	DWORD PTR 4[esp],edi
	mov	DWORD PTR [esp],ebx
	call	$L_DES_encrypt2_begin
	mov	DWORD PTR 8[esp],1
	mov	DWORD PTR 4[esp],esi
	mov	DWORD PTR [esp],ebx
	call	$L_DES_encrypt2_begin
	add	esp,12
	mov	edi,DWORD PTR [ebx]
	mov	esi,DWORD PTR 4[ebx]
	;
	; FP
	rol	esi,2
	rol	edi,3
	mov	eax,edi
	xor	edi,esi
	and	edi,0aaaaaaaah
	xor	eax,edi
	xor	esi,edi
	;
	rol	eax,23
	mov	edi,eax
	xor	eax,esi
	and	eax,003fc03fch
	xor	edi,eax
	xor	esi,eax
	;
	rol	edi,10
	mov	eax,edi
	xor	edi,esi
	and	edi,033333333h
	xor	eax,edi
	xor	esi,edi
	;
	rol	esi,18
	mov	edi,esi
	xor	esi,eax
	and	esi,0fff0000fh
	xor	edi,esi
	xor	eax,esi
	;
	rol	edi,12
	mov	esi,edi
	xor	edi,eax
	and	edi,0f0f0f0f0h
	xor	esi,edi
	xor	eax,edi
	;
	ror	eax,4
	mov	DWORD PTR [ebx],eax
	mov	DWORD PTR 4[ebx],esi
	pop	edi
	pop	esi
	pop	ebp
	pop	ebx
	ret
_DES_encrypt3 ENDP
ALIGN	16
_DES_decrypt3	PROC PUBLIC
$L_DES_decrypt3_begin::
	push	ebx
	mov	ebx,DWORD PTR 8[esp]
	push	ebp
	push	esi
	push	edi
	;
	; Load the data words
	mov	edi,DWORD PTR [ebx]
	mov	esi,DWORD PTR 4[ebx]
	sub	esp,12
	;
	; IP
	rol	edi,4
	mov	edx,edi
	xor	edi,esi
	and	edi,0f0f0f0f0h
	xor	edx,edi
	xor	esi,edi
	;
	rol	esi,20
	mov	edi,esi
	xor	esi,edx
	and	esi,0fff0000fh
	xor	edi,esi
	xor	edx,esi
	;
	rol	edi,14
	mov	esi,edi
	xor	edi,edx
	and	edi,033333333h
	xor	esi,edi
	xor	edx,edi
	;
	rol	edx,22
	mov	edi,edx
	xor	edx,esi
	and	edx,003fc03fch
	xor	edi,edx
	xor	esi,edx
	;
	rol	edi,9
	mov	edx,edi
	xor	edi,esi
	and	edi,0aaaaaaaah
	xor	edx,edi
	xor	esi,edi
	;
	ror	edx,3
	ror	esi,2
	mov	DWORD PTR 4[ebx],esi
	mov	esi,DWORD PTR 36[esp]
	mov	DWORD PTR [ebx],edx
	mov	edi,DWORD PTR 40[esp]
	mov	eax,DWORD PTR 44[esp]
	mov	DWORD PTR 8[esp],0
	mov	DWORD PTR 4[esp],eax
	mov	DWORD PTR [esp],ebx
	call	$L_DES_encrypt2_begin
	mov	DWORD PTR 8[esp],1
	mov	DWORD PTR 4[esp],edi
	mov	DWORD PTR [esp],ebx
	call	$L_DES_encrypt2_begin
	mov	DWORD PTR 8[esp],0
	mov	DWORD PTR 4[esp],esi
	mov	DWORD PTR [esp],ebx
	call	$L_DES_encrypt2_begin
	add	esp,12
	mov	edi,DWORD PTR [ebx]
	mov	esi,DWORD PTR 4[ebx]
	;
	; FP
	rol	esi,2
	rol	edi,3
	mov	eax,edi
	xor	edi,esi
	and	edi,0aaaaaaaah
	xor	eax,edi
	xor	esi,edi
	;
	rol	eax,23
	mov	edi,eax
	xor	eax,esi
	and	eax,003fc03fch
	xor	edi,eax
	xor	esi,eax
	;
	rol	edi,10
	mov	eax,edi
	xor	edi,esi
	and	edi,033333333h
	xor	eax,edi
	xor	esi,edi
	;
	rol	esi,18
	mov	edi,esi
	xor	esi,eax
	and	esi,0fff0000fh
	xor	edi,esi
	xor	eax,esi
	;
	rol	edi,12
	mov	esi,edi
	xor	edi,eax
	and	edi,0f0f0f0f0h
	xor	esi,edi
	xor	eax,edi
	;
	ror	eax,4
	mov	DWORD PTR [ebx],eax
	mov	DWORD PTR 4[ebx],esi
	pop	edi
	pop	esi
	pop	ebp
	pop	ebx
	ret
_DES_decrypt3 ENDP
ALIGN	16
_DES_ncbc_encrypt	PROC PUBLIC
$L_DES_ncbc_encrypt_begin::
	;
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ebp,DWORD PTR 28[esp]
	; getting iv ptr from parameter 4
	mov	ebx,DWORD PTR 36[esp]
	mov	esi,DWORD PTR [ebx]
	mov	edi,DWORD PTR 4[ebx]
	push	edi
	push	esi
	push	edi
	push	esi
	mov	ebx,esp
	mov	esi,DWORD PTR 36[esp]
	mov	edi,DWORD PTR 40[esp]
	; getting encrypt flag from parameter 5
	mov	ecx,DWORD PTR 56[esp]
	; get and push parameter 5
	push	ecx
	; get and push parameter 3
	mov	eax,DWORD PTR 52[esp]
	push	eax
	push	ebx
	cmp	ecx,0
	jz	$L006decrypt
	and	ebp,4294967288
	mov	eax,DWORD PTR 12[esp]
	mov	ebx,DWORD PTR 16[esp]
	jz	$L007encrypt_finish
$L008encrypt_loop:
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR 4[esi]
	xor	eax,ecx
	xor	ebx,edx
	mov	DWORD PTR 12[esp],eax
	mov	DWORD PTR 16[esp],ebx
	call	$L_DES_encrypt1_begin
	mov	eax,DWORD PTR 12[esp]
	mov	ebx,DWORD PTR 16[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	add	esi,8
	add	edi,8
	sub	ebp,8
	jnz	$L008encrypt_loop
$L007encrypt_finish:
	mov	ebp,DWORD PTR 56[esp]
	and	ebp,7
	jz	$L009finish
	call	$L010PIC_point
$L010PIC_point:
	pop	edx
	lea	ecx,DWORD PTR ($L011cbc_enc_jmp_table-$L010PIC_point)[edx]
	mov	ebp,DWORD PTR [ebp*4+ecx]
	add	ebp,edx
	xor	ecx,ecx
	xor	edx,edx
	jmp	ebp
$L012ej7:
	mov	dh,BYTE PTR 6[esi]
	shl	edx,8
$L013ej6:
	mov	dh,BYTE PTR 5[esi]
$L014ej5:
	mov	dl,BYTE PTR 4[esi]
$L015ej4:
	mov	ecx,DWORD PTR [esi]
	jmp	$L016ejend
$L017ej3:
	mov	ch,BYTE PTR 2[esi]
	shl	ecx,8
$L018ej2:
	mov	ch,BYTE PTR 1[esi]
$L019ej1:
	mov	cl,BYTE PTR [esi]
$L016ejend:
	xor	eax,ecx
	xor	ebx,edx
	mov	DWORD PTR 12[esp],eax
	mov	DWORD PTR 16[esp],ebx
	call	$L_DES_encrypt1_begin
	mov	eax,DWORD PTR 12[esp]
	mov	ebx,DWORD PTR 16[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	jmp	$L009finish
$L006decrypt:
	and	ebp,4294967288
	mov	eax,DWORD PTR 20[esp]
	mov	ebx,DWORD PTR 24[esp]
	jz	$L020decrypt_finish
$L021decrypt_loop:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	DWORD PTR 12[esp],eax
	mov	DWORD PTR 16[esp],ebx
	call	$L_DES_encrypt1_begin
	mov	eax,DWORD PTR 12[esp]
	mov	ebx,DWORD PTR 16[esp]
	mov	ecx,DWORD PTR 20[esp]
	mov	edx,DWORD PTR 24[esp]
	xor	ecx,eax
	xor	edx,ebx
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	DWORD PTR [edi],ecx
	mov	DWORD PTR 4[edi],edx
	mov	DWORD PTR 20[esp],eax
	mov	DWORD PTR 24[esp],ebx
	add	esi,8
	add	edi,8
	sub	ebp,8
	jnz	$L021decrypt_loop
$L020decrypt_finish:
	mov	ebp,DWORD PTR 56[esp]
	and	ebp,7
	jz	$L009finish
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	DWORD PTR 12[esp],eax
	mov	DWORD PTR 16[esp],ebx
	call	$L_DES_encrypt1_begin
	mov	eax,DWORD PTR 12[esp]
	mov	ebx,DWORD PTR 16[esp]
	mov	ecx,DWORD PTR 20[esp]
	mov	edx,DWORD PTR 24[esp]
	xor	ecx,eax
	xor	edx,ebx
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
$L022dj7:
	ror	edx,16
	mov	BYTE PTR 6[edi],dl
	shr	edx,16
$L023dj6:
	mov	BYTE PTR 5[edi],dh
$L024dj5:
	mov	BYTE PTR 4[edi],dl
$L025dj4:
	mov	DWORD PTR [edi],ecx
	jmp	$L026djend
$L027dj3:
	ror	ecx,16
	mov	BYTE PTR 2[edi],cl
	shl	ecx,16
$L028dj2:
	mov	BYTE PTR 1[esi],ch
$L029dj1:
	mov	BYTE PTR [esi],cl
$L026djend:
	jmp	$L009finish
$L009finish:
	mov	ecx,DWORD PTR 64[esp]
	add	esp,28
	mov	DWORD PTR [ecx],eax
	mov	DWORD PTR 4[ecx],ebx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
ALIGN	64
$L011cbc_enc_jmp_table:
DD	0
DD	$L019ej1-$L010PIC_point
DD	$L018ej2-$L010PIC_point
DD	$L017ej3-$L010PIC_point
DD	$L015ej4-$L010PIC_point
DD	$L014ej5-$L010PIC_point
DD	$L013ej6-$L010PIC_point
DD	$L012ej7-$L010PIC_point
ALIGN	64
_DES_ncbc_encrypt ENDP
ALIGN	16
_DES_ede3_cbc_encrypt	PROC PUBLIC
$L_DES_ede3_cbc_encrypt_begin::
	;
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ebp,DWORD PTR 28[esp]
	; getting iv ptr from parameter 6
	mov	ebx,DWORD PTR 44[esp]
	mov	esi,DWORD PTR [ebx]
	mov	edi,DWORD PTR 4[ebx]
	push	edi
	push	esi
	push	edi
	push	esi
	mov	ebx,esp
	mov	esi,DWORD PTR 36[esp]
	mov	edi,DWORD PTR 40[esp]
	; getting encrypt flag from parameter 7
	mov	ecx,DWORD PTR 64[esp]
	; get and push parameter 5
	mov	eax,DWORD PTR 56[esp]
	push	eax
	; get and push parameter 4
	mov	eax,DWORD PTR 56[esp]
	push	eax
	; get and push parameter 3
	mov	eax,DWORD PTR 56[esp]
	push	eax
	push	ebx
	cmp	ecx,0
	jz	$L030decrypt
	and	ebp,4294967288
	mov	eax,DWORD PTR 16[esp]
	mov	ebx,DWORD PTR 20[esp]
	jz	$L031encrypt_finish
$L032encrypt_loop:
	mov	ecx,DWORD PTR [esi]
	mov	edx,DWORD PTR 4[esi]
	xor	eax,ecx
	xor	ebx,edx
	mov	DWORD PTR 16[esp],eax
	mov	DWORD PTR 20[esp],ebx
	call	$L_DES_encrypt3_begin
	mov	eax,DWORD PTR 16[esp]
	mov	ebx,DWORD PTR 20[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	add	esi,8
	add	edi,8
	sub	ebp,8
	jnz	$L032encrypt_loop
$L031encrypt_finish:
	mov	ebp,DWORD PTR 60[esp]
	and	ebp,7
	jz	$L033finish
	call	$L034PIC_point
$L034PIC_point:
	pop	edx
	lea	ecx,DWORD PTR ($L035cbc_enc_jmp_table-$L034PIC_point)[edx]
	mov	ebp,DWORD PTR [ebp*4+ecx]
	add	ebp,edx
	xor	ecx,ecx
	xor	edx,edx
	jmp	ebp
$L036ej7:
	mov	dh,BYTE PTR 6[esi]
	shl	edx,8
$L037ej6:
	mov	dh,BYTE PTR 5[esi]
$L038ej5:
	mov	dl,BYTE PTR 4[esi]
$L039ej4:
	mov	ecx,DWORD PTR [esi]
	jmp	$L040ejend
$L041ej3:
	mov	ch,BYTE PTR 2[esi]
	shl	ecx,8
$L042ej2:
	mov	ch,BYTE PTR 1[esi]
$L043ej1:
	mov	cl,BYTE PTR [esi]
$L040ejend:
	xor	eax,ecx
	xor	ebx,edx
	mov	DWORD PTR 16[esp],eax
	mov	DWORD PTR 20[esp],ebx
	call	$L_DES_encrypt3_begin
	mov	eax,DWORD PTR 16[esp]
	mov	ebx,DWORD PTR 20[esp]
	mov	DWORD PTR [edi],eax
	mov	DWORD PTR 4[edi],ebx
	jmp	$L033finish
$L030decrypt:
	and	ebp,4294967288
	mov	eax,DWORD PTR 24[esp]
	mov	ebx,DWORD PTR 28[esp]
	jz	$L044decrypt_finish
$L045decrypt_loop:
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	DWORD PTR 16[esp],eax
	mov	DWORD PTR 20[esp],ebx
	call	$L_DES_decrypt3_begin
	mov	eax,DWORD PTR 16[esp]
	mov	ebx,DWORD PTR 20[esp]
	mov	ecx,DWORD PTR 24[esp]
	mov	edx,DWORD PTR 28[esp]
	xor	ecx,eax
	xor	edx,ebx
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	DWORD PTR [edi],ecx
	mov	DWORD PTR 4[edi],edx
	mov	DWORD PTR 24[esp],eax
	mov	DWORD PTR 28[esp],ebx
	add	esi,8
	add	edi,8
	sub	ebp,8
	jnz	$L045decrypt_loop
$L044decrypt_finish:
	mov	ebp,DWORD PTR 60[esp]
	and	ebp,7
	jz	$L033finish
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
	mov	DWORD PTR 16[esp],eax
	mov	DWORD PTR 20[esp],ebx
	call	$L_DES_decrypt3_begin
	mov	eax,DWORD PTR 16[esp]
	mov	ebx,DWORD PTR 20[esp]
	mov	ecx,DWORD PTR 24[esp]
	mov	edx,DWORD PTR 28[esp]
	xor	ecx,eax
	xor	edx,ebx
	mov	eax,DWORD PTR [esi]
	mov	ebx,DWORD PTR 4[esi]
$L046dj7:
	ror	edx,16
	mov	BYTE PTR 6[edi],dl
	shr	edx,16
$L047dj6:
	mov	BYTE PTR 5[edi],dh
$L048dj5:
	mov	BYTE PTR 4[edi],dl
$L049dj4:
	mov	DWORD PTR [edi],ecx
	jmp	$L050djend
$L051dj3:
	ror	ecx,16
	mov	BYTE PTR 2[edi],cl
	shl	ecx,16
$L052dj2:
	mov	BYTE PTR 1[esi],ch
$L053dj1:
	mov	BYTE PTR [esi],cl
$L050djend:
	jmp	$L033finish
$L033finish:
	mov	ecx,DWORD PTR 76[esp]
	add	esp,32
	mov	DWORD PTR [ecx],eax
	mov	DWORD PTR 4[ecx],ebx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
ALIGN	64
$L035cbc_enc_jmp_table:
DD	0
DD	$L043ej1-$L034PIC_point
DD	$L042ej2-$L034PIC_point
DD	$L041ej3-$L034PIC_point
DD	$L039ej4-$L034PIC_point
DD	$L038ej5-$L034PIC_point
DD	$L037ej6-$L034PIC_point
DD	$L036ej7-$L034PIC_point
ALIGN	64
_DES_ede3_cbc_encrypt ENDP
ALIGN	64
_DES_SPtrans::
$Ldes_sptrans::
DD	34080768,524288,33554434,34080770
DD	33554432,526338,524290,33554434
DD	526338,34080768,34078720,2050
DD	33556482,33554432,0,524290
DD	524288,2,33556480,526336
DD	34080770,34078720,2050,33556480
DD	2,2048,526336,34078722
DD	2048,33556482,34078722,0
DD	0,34080770,33556480,524290
DD	34080768,524288,2050,33556480
DD	34078722,2048,526336,33554434
DD	526338,2,33554434,34078720
DD	34080770,526336,34078720,33556482
DD	33554432,2050,524290,0
DD	524288,33554432,33556482,34080768
DD	2,34078722,2048,526338
DD	1074823184,0,1081344,1074790400
DD	1073741840,32784,1073774592,1081344
DD	32768,1074790416,16,1073774592
DD	1048592,1074823168,1074790400,16
DD	1048576,1073774608,1074790416,32768
DD	1081360,1073741824,0,1048592
DD	1073774608,1081360,1074823168,1073741840
DD	1073741824,1048576,32784,1074823184
DD	1048592,1074823168,1073774592,1081360
DD	1074823184,1048592,1073741840,0
DD	1073741824,32784,1048576,1074790416
DD	32768,1073741824,1081360,1073774608
DD	1074823168,32768,0,1073741840
DD	16,1074823184,1081344,1074790400
DD	1074790416,1048576,32784,1073774592
DD	1073774608,16,1074790400,1081344
DD	67108865,67371264,256,67109121
DD	262145,67108864,67109121,262400
DD	67109120,262144,67371008,1
DD	67371265,257,1,67371009
DD	0,262145,67371264,256
DD	257,67371265,262144,67108865
DD	67371009,67109120,262401,67371008
DD	262400,0,67108864,262401
DD	67371264,256,1,262144
DD	257,262145,67371008,67109121
DD	0,67371264,262400,67371009
DD	262145,67108864,67371265,1
DD	262401,67108865,67108864,67371265
DD	262144,67109120,67109121,262400
DD	67109120,0,67371009,257
DD	67108865,262401,256,67371008
DD	4198408,268439552,8,272633864
DD	0,272629760,268439560,4194312
DD	272633856,268435464,268435456,4104
DD	268435464,4198408,4194304,268435456
DD	272629768,4198400,4096,8
DD	4198400,268439560,272629760,4096
DD	4104,0,4194312,272633856
DD	268439552,272629768,272633864,4194304
DD	272629768,4104,4194304,268435464
DD	4198400,268439552,8,272629760
DD	268439560,0,4096,4194312
DD	0,272629768,272633856,4096
DD	268435456,272633864,4198408,4194304
DD	272633864,8,268439552,4198408
DD	4194312,4198400,272629760,268439560
DD	4104,268435456,268435464,272633856
DD	134217728,65536,1024,134284320
DD	134283296,134218752,66592,134283264
DD	65536,32,134217760,66560
DD	134218784,134283296,134284288,0
DD	66560,134217728,65568,1056
DD	134218752,66592,0,134217760
DD	32,134218784,134284320,65568
DD	134283264,1024,1056,134284288
DD	134284288,134218784,65568,134283264
DD	65536,32,134217760,134218752
DD	134217728,66560,134284320,0
DD	66592,134217728,1024,65568
DD	134218784,1024,0,134284320
DD	134283296,134284288,1056,65536
DD	66560,134283296,134218752,1056
DD	32,66592,134283264,134217760
DD	2147483712,2097216,0,2149588992
DD	2097216,8192,2147491904,2097152
DD	8256,2149589056,2105344,2147483648
DD	2147491840,2147483712,2149580800,2105408
DD	2097152,2147491904,2149580864,0
DD	8192,64,2149588992,2149580864
DD	2149589056,2149580800,2147483648,8256
DD	64,2105344,2105408,2147491840
DD	8256,2147483648,2147491840,2105408
DD	2149588992,2097216,0,2147491840
DD	2147483648,8192,2149580864,2097152
DD	2097216,2149589056,2105344,64
DD	2149589056,2105344,2097152,2147491904
DD	2147483712,2149580800,2105408,0
DD	8192,2147483712,2147491904,2149588992
DD	2149580800,8256,64,2149580864
DD	16384,512,16777728,16777220
DD	16794116,16388,16896,0
DD	16777216,16777732,516,16793600
DD	4,16794112,16793600,516
DD	16777732,16384,16388,16794116
DD	0,16777728,16777220,16896
DD	16793604,16900,16794112,4
DD	16900,16793604,512,16777216
DD	16900,16793600,16793604,516
DD	16384,512,16777216,16793604
DD	16777732,16900,16896,0
DD	512,16777220,4,16777728
DD	0,16777732,16777728,16896
DD	516,16384,16794116,16777216
DD	16794112,4,16388,16794116
DD	16777220,16794112,16793600,16388
DD	545259648,545390592,131200,0
DD	537001984,8388736,545259520,545390720
DD	128,536870912,8519680,131200
DD	8519808,537002112,536871040,545259520
DD	131072,8519808,8388736,537001984
DD	545390720,536871040,0,8519680
DD	536870912,8388608,537002112,545259648
DD	8388608,131072,545390592,128
DD	8388608,131072,536871040,545390720
DD	131200,536870912,0,8519680
DD	545259648,537002112,537001984,8388736
DD	545390592,128,8388736,537001984
DD	545390720,8388608,545259520,536871040
DD	8519680,131200,537002112,545259520
DD	128,545390592,8519808,0
DD	536870912,545259648,131072,8519808
.text$	ENDS
END
