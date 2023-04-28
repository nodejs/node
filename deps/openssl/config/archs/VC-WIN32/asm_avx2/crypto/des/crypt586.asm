%ifidn __OUTPUT_FORMAT__,obj
section	code	use32 class=code align=64
%elifidn __OUTPUT_FORMAT__,win32
$@feat.00 equ 1
section	.text	code align=64
%else
section	.text	code
%endif
extern	_DES_SPtrans
global	_fcrypt_body
align	16
_fcrypt_body:
L$_fcrypt_body_begin:
	push	ebp
	push	ebx
	push	esi
	push	edi
	; 
	; Load the 2 words
	xor	edi,edi
	xor	esi,esi
	lea	edx,[_DES_SPtrans]
	push	edx
	mov	ebp,DWORD [28+esp]
	push	DWORD 25
L$000start:
	; 
	; Round 0
	mov	eax,DWORD [36+esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [ebp]
	xor	eax,ebx
	mov	ecx,DWORD [4+ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 1
	mov	eax,DWORD [36+esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [8+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [12+ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 2
	mov	eax,DWORD [36+esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [16+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [20+ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 3
	mov	eax,DWORD [36+esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [24+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [28+ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 4
	mov	eax,DWORD [36+esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [32+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [36+ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 5
	mov	eax,DWORD [36+esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [40+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [44+ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 6
	mov	eax,DWORD [36+esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [48+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [52+ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 7
	mov	eax,DWORD [36+esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [56+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [60+ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 8
	mov	eax,DWORD [36+esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [64+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [68+ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 9
	mov	eax,DWORD [36+esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [72+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [76+ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 10
	mov	eax,DWORD [36+esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [80+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [84+ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 11
	mov	eax,DWORD [36+esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [88+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [92+ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 12
	mov	eax,DWORD [36+esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [96+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [100+ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 13
	mov	eax,DWORD [36+esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [104+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [108+ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 14
	mov	eax,DWORD [36+esp]
	mov	edx,esi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,esi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [112+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [116+ebp]
	xor	eax,esi
	xor	edx,esi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	edi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	edi,ebx
	mov	ebp,DWORD [32+esp]
	; 
	; Round 15
	mov	eax,DWORD [36+esp]
	mov	edx,edi
	shr	edx,16
	mov	ecx,DWORD [40+esp]
	xor	edx,edi
	and	eax,edx
	and	edx,ecx
	mov	ebx,eax
	shl	ebx,16
	mov	ecx,edx
	shl	ecx,16
	xor	eax,ebx
	xor	edx,ecx
	mov	ebx,DWORD [120+ebp]
	xor	eax,ebx
	mov	ecx,DWORD [124+ebp]
	xor	eax,edi
	xor	edx,edi
	xor	edx,ecx
	and	eax,0xfcfcfcfc
	xor	ebx,ebx
	and	edx,0xcfcfcfcf
	xor	ecx,ecx
	mov	bl,al
	mov	cl,ah
	ror	edx,4
	mov	ebp,DWORD [4+esp]
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
	mov	ebx,DWORD [0x600+ebx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x700+ecx*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x400+eax*1+ebp]
	xor	esi,ebx
	mov	ebx,DWORD [0x500+edx*1+ebp]
	xor	esi,ebx
	mov	ebp,DWORD [32+esp]
	mov	ebx,DWORD [esp]
	mov	eax,edi
	dec	ebx
	mov	edi,esi
	mov	esi,eax
	mov	DWORD [esp],ebx
	jnz	NEAR L$000start
	; 
	; FP
	mov	edx,DWORD [28+esp]
	ror	edi,1
	mov	eax,esi
	xor	esi,edi
	and	esi,0xaaaaaaaa
	xor	eax,esi
	xor	edi,esi
	; 
	rol	eax,23
	mov	esi,eax
	xor	eax,edi
	and	eax,0x03fc03fc
	xor	esi,eax
	xor	edi,eax
	; 
	rol	esi,10
	mov	eax,esi
	xor	esi,edi
	and	esi,0x33333333
	xor	eax,esi
	xor	edi,esi
	; 
	rol	edi,18
	mov	esi,edi
	xor	edi,eax
	and	edi,0xfff0000f
	xor	esi,edi
	xor	eax,edi
	; 
	rol	esi,12
	mov	edi,esi
	xor	esi,eax
	and	esi,0xf0f0f0f0
	xor	edi,esi
	xor	eax,esi
	; 
	ror	eax,4
	mov	DWORD [edx],eax
	mov	DWORD [4+edx],edi
	add	esp,8
	pop	edi
	pop	esi
	pop	ebx
	pop	ebp
	ret
