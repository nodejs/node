%ifidn __OUTPUT_FORMAT__,obj
section	code	use32 class=code align=64
%elifidn __OUTPUT_FORMAT__,win32
$@feat.00 equ 1
section	.text	code align=64
%else
section	.text	code
%endif
global	_DES_SPtrans
align	16
__x86_DES_encrypt:
	push	ecx
	; Round 0
	mov	eax,DWORD [ecx]
	xor	ebx,ebx
	mov	edx,DWORD [4+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 1
	mov	eax,DWORD [8+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [12+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 2
	mov	eax,DWORD [16+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [20+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 3
	mov	eax,DWORD [24+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [28+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 4
	mov	eax,DWORD [32+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [36+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 5
	mov	eax,DWORD [40+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [44+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 6
	mov	eax,DWORD [48+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [52+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 7
	mov	eax,DWORD [56+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [60+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 8
	mov	eax,DWORD [64+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [68+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 9
	mov	eax,DWORD [72+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [76+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 10
	mov	eax,DWORD [80+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [84+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 11
	mov	eax,DWORD [88+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [92+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 12
	mov	eax,DWORD [96+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [100+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 13
	mov	eax,DWORD [104+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [108+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 14
	mov	eax,DWORD [112+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [116+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 15
	mov	eax,DWORD [120+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [124+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	add	esp,4
	ret
align	16
__x86_DES_decrypt:
	push	ecx
	; Round 15
	mov	eax,DWORD [120+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [124+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 14
	mov	eax,DWORD [112+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [116+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 13
	mov	eax,DWORD [104+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [108+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 12
	mov	eax,DWORD [96+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [100+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 11
	mov	eax,DWORD [88+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [92+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 10
	mov	eax,DWORD [80+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [84+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 9
	mov	eax,DWORD [72+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [76+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 8
	mov	eax,DWORD [64+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [68+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 7
	mov	eax,DWORD [56+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [60+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 6
	mov	eax,DWORD [48+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [52+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 5
	mov	eax,DWORD [40+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [44+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 4
	mov	eax,DWORD [32+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [36+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 3
	mov	eax,DWORD [24+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [28+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 2
	mov	eax,DWORD [16+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [20+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	; Round 1
	mov	eax,DWORD [8+ecx]
	xor	ebx,ebx
	mov	edx,DWORD [12+ecx]
	xor	eax,esi
	xor	ecx,ecx
	xor	edx,esi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	edi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	edi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	edi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	edi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	edi,DWORD [0x600+ebx*1+ebp]
	xor	edi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	edi,DWORD [0x400+eax*1+ebp]
	xor	edi,DWORD [0x500+edx*1+ebp]
	; Round 0
	mov	eax,DWORD [ecx]
	xor	ebx,ebx
	mov	edx,DWORD [4+ecx]
	xor	eax,edi
	xor	ecx,ecx
	xor	edx,edi
	and	eax,0xfcfcfcfc
	and	edx,0xcfcfcfcf
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	xor	esi,DWORD [ebx*1+ebp]
	mov	bl,dl
	xor	esi,DWORD [0x200+ecx*1+ebp]
	mov	cl,dh
	shr	eax,16
	xor	esi,DWORD [0x100+ebx*1+ebp]
	mov	bl,ah
	shr	edx,16
	xor	esi,DWORD [0x300+ecx*1+ebp]
	mov	cl,dh
	and	eax,0xff
	and	edx,0xff
	xor	esi,DWORD [0x600+ebx*1+ebp]
	xor	esi,DWORD [0x700+ecx*1+ebp]
	mov	ecx,DWORD [esp]
	xor	esi,DWORD [0x400+eax*1+ebp]
	xor	esi,DWORD [0x500+edx*1+ebp]
	add	esp,4
	ret
global	_DES_encrypt1
align	16
_DES_encrypt1:
L$_DES_encrypt1_begin:
	push	esi
	push	edi
	; 
	; Load the 2 words
	mov	esi,DWORD [12+esp]
	xor	ecx,ecx
	push	ebx
	push	ebp
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [28+esp]
	mov	edi,DWORD [4+esi]
	; 
	; IP
	rol	eax,4
	mov	esi,eax
	xor	eax,edi
	and	eax,0xf0f0f0f0
	xor	esi,eax
	xor	edi,eax
	; 
	rol	edi,20
	mov	eax,edi
	xor	edi,esi
	and	edi,0xfff0000f
	xor	eax,edi
	xor	esi,edi
	; 
	rol	eax,14
	mov	edi,eax
	xor	eax,esi
	and	eax,0x33333333
	xor	edi,eax
	xor	esi,eax
	; 
	rol	esi,22
	mov	eax,esi
	xor	esi,edi
	and	esi,0x03fc03fc
	xor	eax,esi
	xor	edi,esi
	; 
	rol	eax,9
	mov	esi,eax
	xor	eax,edi
	and	eax,0xaaaaaaaa
	xor	esi,eax
	xor	edi,eax
	; 
	rol	edi,1
	call	L$000pic_point
L$000pic_point:
	pop	ebp
	lea	ebp,[(L$des_sptrans-L$000pic_point)+ebp]
	mov	ecx,DWORD [24+esp]
	cmp	ebx,0
	je	NEAR L$001decrypt
	call	__x86_DES_encrypt
	jmp	NEAR L$002done
L$001decrypt:
	call	__x86_DES_decrypt
L$002done:
	; 
	; FP
	mov	edx,DWORD [20+esp]
	ror	esi,1
	mov	eax,edi
	xor	edi,esi
	and	edi,0xaaaaaaaa
	xor	eax,edi
	xor	esi,edi
	; 
	rol	eax,23
	mov	edi,eax
	xor	eax,esi
	and	eax,0x03fc03fc
	xor	edi,eax
	xor	esi,eax
	; 
	rol	edi,10
	mov	eax,edi
	xor	edi,esi
	and	edi,0x33333333
	xor	eax,edi
	xor	esi,edi
	; 
	rol	esi,18
	mov	edi,esi
	xor	esi,eax
	and	esi,0xfff0000f
	xor	edi,esi
	xor	eax,esi
	; 
	rol	edi,12
	mov	esi,edi
	xor	edi,eax
	and	edi,0xf0f0f0f0
	xor	esi,edi
	xor	eax,edi
	; 
	ror	eax,4
	mov	DWORD [edx],eax
	mov	DWORD [4+edx],esi
	pop	ebp
	pop	ebx
	pop	edi
	pop	esi
	ret
global	_DES_encrypt2
align	16
_DES_encrypt2:
L$_DES_encrypt2_begin:
	push	esi
	push	edi
	; 
	; Load the 2 words
	mov	eax,DWORD [12+esp]
	xor	ecx,ecx
	push	ebx
	push	ebp
	mov	esi,DWORD [eax]
	mov	ebx,DWORD [28+esp]
	rol	esi,3
	mov	edi,DWORD [4+eax]
	rol	edi,3
	call	L$003pic_point
L$003pic_point:
	pop	ebp
	lea	ebp,[(L$des_sptrans-L$003pic_point)+ebp]
	mov	ecx,DWORD [24+esp]
	cmp	ebx,0
	je	NEAR L$004decrypt
	call	__x86_DES_encrypt
	jmp	NEAR L$005done
L$004decrypt:
	call	__x86_DES_decrypt
L$005done:
	; 
	; Fixup
	ror	edi,3
	mov	eax,DWORD [20+esp]
	ror	esi,3
	mov	DWORD [eax],edi
	mov	DWORD [4+eax],esi
	pop	ebp
	pop	ebx
	pop	edi
	pop	esi
	ret
global	_DES_encrypt3
align	16
_DES_encrypt3:
L$_DES_encrypt3_begin:
	push	ebx
	mov	ebx,DWORD [8+esp]
	push	ebp
	push	esi
	push	edi
	; 
	; Load the data words
	mov	edi,DWORD [ebx]
	mov	esi,DWORD [4+ebx]
	sub	esp,12
	; 
	; IP
	rol	edi,4
	mov	edx,edi
	xor	edi,esi
	and	edi,0xf0f0f0f0
	xor	edx,edi
	xor	esi,edi
	; 
	rol	esi,20
	mov	edi,esi
	xor	esi,edx
	and	esi,0xfff0000f
	xor	edi,esi
	xor	edx,esi
	; 
	rol	edi,14
	mov	esi,edi
	xor	edi,edx
	and	edi,0x33333333
	xor	esi,edi
	xor	edx,edi
	; 
	rol	edx,22
	mov	edi,edx
	xor	edx,esi
	and	edx,0x03fc03fc
	xor	edi,edx
	xor	esi,edx
	; 
	rol	edi,9
	mov	edx,edi
	xor	edi,esi
	and	edi,0xaaaaaaaa
	xor	edx,edi
	xor	esi,edi
	; 
	ror	edx,3
	ror	esi,2
	mov	DWORD [4+ebx],esi
	mov	eax,DWORD [36+esp]
	mov	DWORD [ebx],edx
	mov	edi,DWORD [40+esp]
	mov	esi,DWORD [44+esp]
	mov	DWORD [8+esp],DWORD 1
	mov	DWORD [4+esp],eax
	mov	DWORD [esp],ebx
	call	L$_DES_encrypt2_begin
	mov	DWORD [8+esp],DWORD 0
	mov	DWORD [4+esp],edi
	mov	DWORD [esp],ebx
	call	L$_DES_encrypt2_begin
	mov	DWORD [8+esp],DWORD 1
	mov	DWORD [4+esp],esi
	mov	DWORD [esp],ebx
	call	L$_DES_encrypt2_begin
	add	esp,12
	mov	edi,DWORD [ebx]
	mov	esi,DWORD [4+ebx]
	; 
	; FP
	rol	esi,2
	rol	edi,3
	mov	eax,edi
	xor	edi,esi
	and	edi,0xaaaaaaaa
	xor	eax,edi
	xor	esi,edi
	; 
	rol	eax,23
	mov	edi,eax
	xor	eax,esi
	and	eax,0x03fc03fc
	xor	edi,eax
	xor	esi,eax
	; 
	rol	edi,10
	mov	eax,edi
	xor	edi,esi
	and	edi,0x33333333
	xor	eax,edi
	xor	esi,edi
	; 
	rol	esi,18
	mov	edi,esi
	xor	esi,eax
	and	esi,0xfff0000f
	xor	edi,esi
	xor	eax,esi
	; 
	rol	edi,12
	mov	esi,edi
	xor	edi,eax
	and	edi,0xf0f0f0f0
	xor	esi,edi
	xor	eax,edi
	; 
	ror	eax,4
	mov	DWORD [ebx],eax
	mov	DWORD [4+ebx],esi
	pop	edi
	pop	esi
	pop	ebp
	pop	ebx
	ret
global	_DES_decrypt3
align	16
_DES_decrypt3:
L$_DES_decrypt3_begin:
	push	ebx
	mov	ebx,DWORD [8+esp]
	push	ebp
	push	esi
	push	edi
	; 
	; Load the data words
	mov	edi,DWORD [ebx]
	mov	esi,DWORD [4+ebx]
	sub	esp,12
	; 
	; IP
	rol	edi,4
	mov	edx,edi
	xor	edi,esi
	and	edi,0xf0f0f0f0
	xor	edx,edi
	xor	esi,edi
	; 
	rol	esi,20
	mov	edi,esi
	xor	esi,edx
	and	esi,0xfff0000f
	xor	edi,esi
	xor	edx,esi
	; 
	rol	edi,14
	mov	esi,edi
	xor	edi,edx
	and	edi,0x33333333
	xor	esi,edi
	xor	edx,edi
	; 
	rol	edx,22
	mov	edi,edx
	xor	edx,esi
	and	edx,0x03fc03fc
	xor	edi,edx
	xor	esi,edx
	; 
	rol	edi,9
	mov	edx,edi
	xor	edi,esi
	and	edi,0xaaaaaaaa
	xor	edx,edi
	xor	esi,edi
	; 
	ror	edx,3
	ror	esi,2
	mov	DWORD [4+ebx],esi
	mov	esi,DWORD [36+esp]
	mov	DWORD [ebx],edx
	mov	edi,DWORD [40+esp]
	mov	eax,DWORD [44+esp]
	mov	DWORD [8+esp],DWORD 0
	mov	DWORD [4+esp],eax
	mov	DWORD [esp],ebx
	call	L$_DES_encrypt2_begin
	mov	DWORD [8+esp],DWORD 1
	mov	DWORD [4+esp],edi
	mov	DWORD [esp],ebx
	call	L$_DES_encrypt2_begin
	mov	DWORD [8+esp],DWORD 0
	mov	DWORD [4+esp],esi
	mov	DWORD [esp],ebx
	call	L$_DES_encrypt2_begin
	add	esp,12
	mov	edi,DWORD [ebx]
	mov	esi,DWORD [4+ebx]
	; 
	; FP
	rol	esi,2
	rol	edi,3
	mov	eax,edi
	xor	edi,esi
	and	edi,0xaaaaaaaa
	xor	eax,edi
	xor	esi,edi
	; 
	rol	eax,23
	mov	edi,eax
	xor	eax,esi
	and	eax,0x03fc03fc
	xor	edi,eax
	xor	esi,eax
	; 
	rol	edi,10
	mov	eax,edi
	xor	edi,esi
	and	edi,0x33333333
	xor	eax,edi
	xor	esi,edi
	; 
	rol	esi,18
	mov	edi,esi
	xor	esi,eax
	and	esi,0xfff0000f
	xor	edi,esi
	xor	eax,esi
	; 
	rol	edi,12
	mov	esi,edi
	xor	edi,eax
	and	edi,0xf0f0f0f0
	xor	esi,edi
	xor	eax,edi
	; 
	ror	eax,4
	mov	DWORD [ebx],eax
	mov	DWORD [4+ebx],esi
	pop	edi
	pop	esi
	pop	ebp
	pop	ebx
	ret
global	_DES_ncbc_encrypt
align	16
_DES_ncbc_encrypt:
L$_DES_ncbc_encrypt_begin:
	; 
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ebp,DWORD [28+esp]
	; getting iv ptr from parameter 4
	mov	ebx,DWORD [36+esp]
	mov	esi,DWORD [ebx]
	mov	edi,DWORD [4+ebx]
	push	edi
	push	esi
	push	edi
	push	esi
	mov	ebx,esp
	mov	esi,DWORD [36+esp]
	mov	edi,DWORD [40+esp]
	; getting encrypt flag from parameter 5
	mov	ecx,DWORD [56+esp]
	; get and push parameter 5
	push	ecx
	; get and push parameter 3
	mov	eax,DWORD [52+esp]
	push	eax
	push	ebx
	cmp	ecx,0
	jz	NEAR L$006decrypt
	and	ebp,4294967288
	mov	eax,DWORD [12+esp]
	mov	ebx,DWORD [16+esp]
	jz	NEAR L$007encrypt_finish
L$008encrypt_loop:
	mov	ecx,DWORD [esi]
	mov	edx,DWORD [4+esi]
	xor	eax,ecx
	xor	ebx,edx
	mov	DWORD [12+esp],eax
	mov	DWORD [16+esp],ebx
	call	L$_DES_encrypt1_begin
	mov	eax,DWORD [12+esp]
	mov	ebx,DWORD [16+esp]
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],ebx
	add	esi,8
	add	edi,8
	sub	ebp,8
	jnz	NEAR L$008encrypt_loop
L$007encrypt_finish:
	mov	ebp,DWORD [56+esp]
	and	ebp,7
	jz	NEAR L$009finish
	call	L$010PIC_point
L$010PIC_point:
	pop	edx
	lea	ecx,[(L$011cbc_enc_jmp_table-L$010PIC_point)+edx]
	mov	ebp,DWORD [ebp*4+ecx]
	add	ebp,edx
	xor	ecx,ecx
	xor	edx,edx
	jmp	ebp
L$012ej7:
	mov	dh,BYTE [6+esi]
	shl	edx,8
L$013ej6:
	mov	dh,BYTE [5+esi]
L$014ej5:
	mov	dl,BYTE [4+esi]
L$015ej4:
	mov	ecx,DWORD [esi]
	jmp	NEAR L$016ejend
L$017ej3:
	mov	ch,BYTE [2+esi]
	shl	ecx,8
L$018ej2:
	mov	ch,BYTE [1+esi]
L$019ej1:
	mov	cl,BYTE [esi]
L$016ejend:
	xor	eax,ecx
	xor	ebx,edx
	mov	DWORD [12+esp],eax
	mov	DWORD [16+esp],ebx
	call	L$_DES_encrypt1_begin
	mov	eax,DWORD [12+esp]
	mov	ebx,DWORD [16+esp]
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],ebx
	jmp	NEAR L$009finish
L$006decrypt:
	and	ebp,4294967288
	mov	eax,DWORD [20+esp]
	mov	ebx,DWORD [24+esp]
	jz	NEAR L$020decrypt_finish
L$021decrypt_loop:
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	DWORD [12+esp],eax
	mov	DWORD [16+esp],ebx
	call	L$_DES_encrypt1_begin
	mov	eax,DWORD [12+esp]
	mov	ebx,DWORD [16+esp]
	mov	ecx,DWORD [20+esp]
	mov	edx,DWORD [24+esp]
	xor	ecx,eax
	xor	edx,ebx
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	DWORD [edi],ecx
	mov	DWORD [4+edi],edx
	mov	DWORD [20+esp],eax
	mov	DWORD [24+esp],ebx
	add	esi,8
	add	edi,8
	sub	ebp,8
	jnz	NEAR L$021decrypt_loop
L$020decrypt_finish:
	mov	ebp,DWORD [56+esp]
	and	ebp,7
	jz	NEAR L$009finish
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	DWORD [12+esp],eax
	mov	DWORD [16+esp],ebx
	call	L$_DES_encrypt1_begin
	mov	eax,DWORD [12+esp]
	mov	ebx,DWORD [16+esp]
	mov	ecx,DWORD [20+esp]
	mov	edx,DWORD [24+esp]
	xor	ecx,eax
	xor	edx,ebx
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
L$022dj7:
	ror	edx,16
	mov	BYTE [6+edi],dl
	shr	edx,16
L$023dj6:
	mov	BYTE [5+edi],dh
L$024dj5:
	mov	BYTE [4+edi],dl
L$025dj4:
	mov	DWORD [edi],ecx
	jmp	NEAR L$026djend
L$027dj3:
	ror	ecx,16
	mov	BYTE [2+edi],cl
	shl	ecx,16
L$028dj2:
	mov	BYTE [1+esi],ch
L$029dj1:
	mov	BYTE [esi],cl
L$026djend:
	jmp	NEAR L$009finish
L$009finish:
	mov	ecx,DWORD [64+esp]
	add	esp,28
	mov	DWORD [ecx],eax
	mov	DWORD [4+ecx],ebx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
align	64
L$011cbc_enc_jmp_table:
dd	0
dd	L$019ej1-L$010PIC_point
dd	L$018ej2-L$010PIC_point
dd	L$017ej3-L$010PIC_point
dd	L$015ej4-L$010PIC_point
dd	L$014ej5-L$010PIC_point
dd	L$013ej6-L$010PIC_point
dd	L$012ej7-L$010PIC_point
align	64
global	_DES_ede3_cbc_encrypt
align	16
_DES_ede3_cbc_encrypt:
L$_DES_ede3_cbc_encrypt_begin:
	; 
	push	ebp
	push	ebx
	push	esi
	push	edi
	mov	ebp,DWORD [28+esp]
	; getting iv ptr from parameter 6
	mov	ebx,DWORD [44+esp]
	mov	esi,DWORD [ebx]
	mov	edi,DWORD [4+ebx]
	push	edi
	push	esi
	push	edi
	push	esi
	mov	ebx,esp
	mov	esi,DWORD [36+esp]
	mov	edi,DWORD [40+esp]
	; getting encrypt flag from parameter 7
	mov	ecx,DWORD [64+esp]
	; get and push parameter 5
	mov	eax,DWORD [56+esp]
	push	eax
	; get and push parameter 4
	mov	eax,DWORD [56+esp]
	push	eax
	; get and push parameter 3
	mov	eax,DWORD [56+esp]
	push	eax
	push	ebx
	cmp	ecx,0
	jz	NEAR L$030decrypt
	and	ebp,4294967288
	mov	eax,DWORD [16+esp]
	mov	ebx,DWORD [20+esp]
	jz	NEAR L$031encrypt_finish
L$032encrypt_loop:
	mov	ecx,DWORD [esi]
	mov	edx,DWORD [4+esi]
	xor	eax,ecx
	xor	ebx,edx
	mov	DWORD [16+esp],eax
	mov	DWORD [20+esp],ebx
	call	L$_DES_encrypt3_begin
	mov	eax,DWORD [16+esp]
	mov	ebx,DWORD [20+esp]
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],ebx
	add	esi,8
	add	edi,8
	sub	ebp,8
	jnz	NEAR L$032encrypt_loop
L$031encrypt_finish:
	mov	ebp,DWORD [60+esp]
	and	ebp,7
	jz	NEAR L$033finish
	call	L$034PIC_point
L$034PIC_point:
	pop	edx
	lea	ecx,[(L$035cbc_enc_jmp_table-L$034PIC_point)+edx]
	mov	ebp,DWORD [ebp*4+ecx]
	add	ebp,edx
	xor	ecx,ecx
	xor	edx,edx
	jmp	ebp
L$036ej7:
	mov	dh,BYTE [6+esi]
	shl	edx,8
L$037ej6:
	mov	dh,BYTE [5+esi]
L$038ej5:
	mov	dl,BYTE [4+esi]
L$039ej4:
	mov	ecx,DWORD [esi]
	jmp	NEAR L$040ejend
L$041ej3:
	mov	ch,BYTE [2+esi]
	shl	ecx,8
L$042ej2:
	mov	ch,BYTE [1+esi]
L$043ej1:
	mov	cl,BYTE [esi]
L$040ejend:
	xor	eax,ecx
	xor	ebx,edx
	mov	DWORD [16+esp],eax
	mov	DWORD [20+esp],ebx
	call	L$_DES_encrypt3_begin
	mov	eax,DWORD [16+esp]
	mov	ebx,DWORD [20+esp]
	mov	DWORD [edi],eax
	mov	DWORD [4+edi],ebx
	jmp	NEAR L$033finish
L$030decrypt:
	and	ebp,4294967288
	mov	eax,DWORD [24+esp]
	mov	ebx,DWORD [28+esp]
	jz	NEAR L$044decrypt_finish
L$045decrypt_loop:
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	DWORD [16+esp],eax
	mov	DWORD [20+esp],ebx
	call	L$_DES_decrypt3_begin
	mov	eax,DWORD [16+esp]
	mov	ebx,DWORD [20+esp]
	mov	ecx,DWORD [24+esp]
	mov	edx,DWORD [28+esp]
	xor	ecx,eax
	xor	edx,ebx
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	DWORD [edi],ecx
	mov	DWORD [4+edi],edx
	mov	DWORD [24+esp],eax
	mov	DWORD [28+esp],ebx
	add	esi,8
	add	edi,8
	sub	ebp,8
	jnz	NEAR L$045decrypt_loop
L$044decrypt_finish:
	mov	ebp,DWORD [60+esp]
	and	ebp,7
	jz	NEAR L$033finish
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
	mov	DWORD [16+esp],eax
	mov	DWORD [20+esp],ebx
	call	L$_DES_decrypt3_begin
	mov	eax,DWORD [16+esp]
	mov	ebx,DWORD [20+esp]
	mov	ecx,DWORD [24+esp]
	mov	edx,DWORD [28+esp]
	xor	ecx,eax
	xor	edx,ebx
	mov	eax,DWORD [esi]
	mov	ebx,DWORD [4+esi]
L$046dj7:
	ror	edx,16
	mov	BYTE [6+edi],dl
	shr	edx,16
L$047dj6:
	mov	BYTE [5+edi],dh
L$048dj5:
	mov	BYTE [4+edi],dl
L$049dj4:
	mov	DWORD [edi],ecx
	jmp	NEAR L$050djend
L$051dj3:
	ror	ecx,16
	mov	BYTE [2+edi],cl
	shl	ecx,16
L$052dj2:
	mov	BYTE [1+esi],ch
L$053dj1:
	mov	BYTE [esi],cl
L$050djend:
	jmp	NEAR L$033finish
L$033finish:
	mov	ecx,DWORD [76+esp]
	add	esp,32
	mov	DWORD [ecx],eax
	mov	DWORD [4+ecx],ebx
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
align	64
L$035cbc_enc_jmp_table:
dd	0
dd	L$043ej1-L$034PIC_point
dd	L$042ej2-L$034PIC_point
dd	L$041ej3-L$034PIC_point
dd	L$039ej4-L$034PIC_point
dd	L$038ej5-L$034PIC_point
dd	L$037ej6-L$034PIC_point
dd	L$036ej7-L$034PIC_point
align	64
align	64
_DES_SPtrans:
L$des_sptrans:
dd	34080768,524288,33554434,34080770
dd	33554432,526338,524290,33554434
dd	526338,34080768,34078720,2050
dd	33556482,33554432,0,524290
dd	524288,2,33556480,526336
dd	34080770,34078720,2050,33556480
dd	2,2048,526336,34078722
dd	2048,33556482,34078722,0
dd	0,34080770,33556480,524290
dd	34080768,524288,2050,33556480
dd	34078722,2048,526336,33554434
dd	526338,2,33554434,34078720
dd	34080770,526336,34078720,33556482
dd	33554432,2050,524290,0
dd	524288,33554432,33556482,34080768
dd	2,34078722,2048,526338
dd	1074823184,0,1081344,1074790400
dd	1073741840,32784,1073774592,1081344
dd	32768,1074790416,16,1073774592
dd	1048592,1074823168,1074790400,16
dd	1048576,1073774608,1074790416,32768
dd	1081360,1073741824,0,1048592
dd	1073774608,1081360,1074823168,1073741840
dd	1073741824,1048576,32784,1074823184
dd	1048592,1074823168,1073774592,1081360
dd	1074823184,1048592,1073741840,0
dd	1073741824,32784,1048576,1074790416
dd	32768,1073741824,1081360,1073774608
dd	1074823168,32768,0,1073741840
dd	16,1074823184,1081344,1074790400
dd	1074790416,1048576,32784,1073774592
dd	1073774608,16,1074790400,1081344
dd	67108865,67371264,256,67109121
dd	262145,67108864,67109121,262400
dd	67109120,262144,67371008,1
dd	67371265,257,1,67371009
dd	0,262145,67371264,256
dd	257,67371265,262144,67108865
dd	67371009,67109120,262401,67371008
dd	262400,0,67108864,262401
dd	67371264,256,1,262144
dd	257,262145,67371008,67109121
dd	0,67371264,262400,67371009
dd	262145,67108864,67371265,1
dd	262401,67108865,67108864,67371265
dd	262144,67109120,67109121,262400
dd	67109120,0,67371009,257
dd	67108865,262401,256,67371008
dd	4198408,268439552,8,272633864
dd	0,272629760,268439560,4194312
dd	272633856,268435464,268435456,4104
dd	268435464,4198408,4194304,268435456
dd	272629768,4198400,4096,8
dd	4198400,268439560,272629760,4096
dd	4104,0,4194312,272633856
dd	268439552,272629768,272633864,4194304
dd	272629768,4104,4194304,268435464
dd	4198400,268439552,8,272629760
dd	268439560,0,4096,4194312
dd	0,272629768,272633856,4096
dd	268435456,272633864,4198408,4194304
dd	272633864,8,268439552,4198408
dd	4194312,4198400,272629760,268439560
dd	4104,268435456,268435464,272633856
dd	134217728,65536,1024,134284320
dd	134283296,134218752,66592,134283264
dd	65536,32,134217760,66560
dd	134218784,134283296,134284288,0
dd	66560,134217728,65568,1056
dd	134218752,66592,0,134217760
dd	32,134218784,134284320,65568
dd	134283264,1024,1056,134284288
dd	134284288,134218784,65568,134283264
dd	65536,32,134217760,134218752
dd	134217728,66560,134284320,0
dd	66592,134217728,1024,65568
dd	134218784,1024,0,134284320
dd	134283296,134284288,1056,65536
dd	66560,134283296,134218752,1056
dd	32,66592,134283264,134217760
dd	2147483712,2097216,0,2149588992
dd	2097216,8192,2147491904,2097152
dd	8256,2149589056,2105344,2147483648
dd	2147491840,2147483712,2149580800,2105408
dd	2097152,2147491904,2149580864,0
dd	8192,64,2149588992,2149580864
dd	2149589056,2149580800,2147483648,8256
dd	64,2105344,2105408,2147491840
dd	8256,2147483648,2147491840,2105408
dd	2149588992,2097216,0,2147491840
dd	2147483648,8192,2149580864,2097152
dd	2097216,2149589056,2105344,64
dd	2149589056,2105344,2097152,2147491904
dd	2147483712,2149580800,2105408,0
dd	8192,2147483712,2147491904,2149588992
dd	2149580800,8256,64,2149580864
dd	16384,512,16777728,16777220
dd	16794116,16388,16896,0
dd	16777216,16777732,516,16793600
dd	4,16794112,16793600,516
dd	16777732,16384,16388,16794116
dd	0,16777728,16777220,16896
dd	16793604,16900,16794112,4
dd	16900,16793604,512,16777216
dd	16900,16793600,16793604,516
dd	16384,512,16777216,16793604
dd	16777732,16900,16896,0
dd	512,16777220,4,16777728
dd	0,16777732,16777728,16896
dd	516,16384,16794116,16777216
dd	16794112,4,16388,16794116
dd	16777220,16794112,16793600,16388
dd	545259648,545390592,131200,0
dd	537001984,8388736,545259520,545390720
dd	128,536870912,8519680,131200
dd	8519808,537002112,536871040,545259520
dd	131072,8519808,8388736,537001984
dd	545390720,536871040,0,8519680
dd	536870912,8388608,537002112,545259648
dd	8388608,131072,545390592,128
dd	8388608,131072,536871040,545390720
dd	131200,536870912,0,8519680
dd	545259648,537002112,537001984,8388736
dd	545390592,128,8388736,537001984
dd	545390720,8388608,545259520,536871040
dd	8519680,131200,537002112,545259520
dd	128,545390592,8519808,0
dd	536870912,545259648,131072,8519808
