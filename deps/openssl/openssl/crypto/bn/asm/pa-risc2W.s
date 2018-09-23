;
; PA-RISC 64-bit implementation of bn_asm code
;
; This code is approximately 2x faster than the C version
; for RSA/DSA.
;
; See http://devresource.hp.com/  for more details on the PA-RISC
; architecture.  Also see the book "PA-RISC 2.0 Architecture"
; by Gerry Kane for information on the instruction set architecture.
;
; Code written by Chris Ruemmler (with some help from the HP C
; compiler).
;
; The code compiles with HP's assembler
;

	.level	2.0W
	.space	$TEXT$
	.subspa	$CODE$,QUAD=0,ALIGN=8,ACCESS=0x2c,CODE_ONLY

;
; Global Register definitions used for the routines.
;
; Some information about HP's runtime architecture for 64-bits.
;
; "Caller save" means the calling function must save the register
; if it wants the register to be preserved.
; "Callee save" means if a function uses the register, it must save
; the value before using it.
;
; For the floating point registers 
;
;    "caller save" registers: fr4-fr11, fr22-fr31
;    "callee save" registers: fr12-fr21
;    "special" registers: fr0-fr3 (status and exception registers)
;
; For the integer registers
;     value zero             :  r0
;     "caller save" registers: r1,r19-r26
;     "callee save" registers: r3-r18
;     return register        :  r2  (rp)
;     return values          ; r28  (ret0,ret1)
;     Stack pointer          ; r30  (sp) 
;     global data pointer    ; r27  (dp)
;     argument pointer       ; r29  (ap)
;     millicode return ptr   ; r31  (also a caller save register)


;
; Arguments to the routines
;
r_ptr       .reg %r26
a_ptr       .reg %r25
b_ptr       .reg %r24
num         .reg %r24
w           .reg %r23
n           .reg %r23


;
; Globals used in some routines
;

top_overflow .reg %r29
high_mask    .reg %r22    ; value 0xffffffff80000000L


;------------------------------------------------------------------------------
;
; bn_mul_add_words
;
;BN_ULONG bn_mul_add_words(BN_ULONG *r_ptr, BN_ULONG *a_ptr, 
;								int num, BN_ULONG w)
;
; arg0 = r_ptr
; arg1 = a_ptr
; arg2 = num
; arg3 = w
;
; Local register definitions
;

fm1          .reg %fr22
fm           .reg %fr23
ht_temp      .reg %fr24
ht_temp_1    .reg %fr25
lt_temp      .reg %fr26
lt_temp_1    .reg %fr27
fm1_1        .reg %fr28
fm_1         .reg %fr29

fw_h         .reg %fr7L
fw_l         .reg %fr7R
fw           .reg %fr7

fht_0        .reg %fr8L
flt_0        .reg %fr8R
t_float_0    .reg %fr8

fht_1        .reg %fr9L
flt_1        .reg %fr9R
t_float_1    .reg %fr9

tmp_0        .reg %r31
tmp_1        .reg %r21
m_0          .reg %r20 
m_1          .reg %r19 
ht_0         .reg %r1  
ht_1         .reg %r3
lt_0         .reg %r4
lt_1         .reg %r5
m1_0         .reg %r6 
m1_1         .reg %r7 
rp_val       .reg %r8
rp_val_1     .reg %r9

bn_mul_add_words
	.export	bn_mul_add_words,entry,NO_RELOCATION,LONG_RETURN
	.proc
	.callinfo frame=128
    .entry
	.align 64

    STD     %r3,0(%sp)          ; save r3  
    STD     %r4,8(%sp)          ; save r4  
	NOP                         ; Needed to make the loop 16-byte aligned
	NOP                         ; Needed to make the loop 16-byte aligned

    STD     %r5,16(%sp)         ; save r5  
    STD     %r6,24(%sp)         ; save r6  
    STD     %r7,32(%sp)         ; save r7  
    STD     %r8,40(%sp)         ; save r8  

    STD     %r9,48(%sp)         ; save r9  
    COPY    %r0,%ret0           ; return 0 by default
    DEPDI,Z 1,31,1,top_overflow ; top_overflow = 1 << 32    
	STD     w,56(%sp)           ; store w on stack

    CMPIB,>= 0,num,bn_mul_add_words_exit  ; if (num <= 0) then exit
	LDO     128(%sp),%sp       ; bump stack

	;
	; The loop is unrolled twice, so if there is only 1 number
    ; then go straight to the cleanup code.
	;
	CMPIB,= 1,num,bn_mul_add_words_single_top
	FLDD    -72(%sp),fw     ; load up w into fp register fw (fw_h/fw_l)

	;
	; This loop is unrolled 2 times (64-byte aligned as well)
	;
	; PA-RISC 2.0 chips have two fully pipelined multipliers, thus
    ; two 32-bit mutiplies can be issued per cycle.
    ; 
bn_mul_add_words_unroll2

    FLDD    0(a_ptr),t_float_0       ; load up 64-bit value (fr8L) ht(L)/lt(R)
    FLDD    8(a_ptr),t_float_1       ; load up 64-bit value (fr8L) ht(L)/lt(R)
    LDD     0(r_ptr),rp_val          ; rp[0]
    LDD     8(r_ptr),rp_val_1        ; rp[1]

    XMPYU   fht_0,fw_l,fm1           ; m1[0] = fht_0*fw_l
    XMPYU   fht_1,fw_l,fm1_1         ; m1[1] = fht_1*fw_l
    FSTD    fm1,-16(%sp)             ; -16(sp) = m1[0]
    FSTD    fm1_1,-48(%sp)           ; -48(sp) = m1[1]

    XMPYU   flt_0,fw_h,fm            ; m[0] = flt_0*fw_h
    XMPYU   flt_1,fw_h,fm_1          ; m[1] = flt_1*fw_h
    FSTD    fm,-8(%sp)               ; -8(sp) = m[0]
    FSTD    fm_1,-40(%sp)            ; -40(sp) = m[1]

    XMPYU   fht_0,fw_h,ht_temp       ; ht_temp   = fht_0*fw_h
    XMPYU   fht_1,fw_h,ht_temp_1     ; ht_temp_1 = fht_1*fw_h
    FSTD    ht_temp,-24(%sp)         ; -24(sp)   = ht_temp
    FSTD    ht_temp_1,-56(%sp)       ; -56(sp)   = ht_temp_1

    XMPYU   flt_0,fw_l,lt_temp       ; lt_temp = lt*fw_l
    XMPYU   flt_1,fw_l,lt_temp_1     ; lt_temp = lt*fw_l
    FSTD    lt_temp,-32(%sp)         ; -32(sp) = lt_temp 
    FSTD    lt_temp_1,-64(%sp)       ; -64(sp) = lt_temp_1 

    LDD     -8(%sp),m_0              ; m[0] 
    LDD     -40(%sp),m_1             ; m[1]
    LDD     -16(%sp),m1_0            ; m1[0]
    LDD     -48(%sp),m1_1            ; m1[1]

    LDD     -24(%sp),ht_0            ; ht[0]
    LDD     -56(%sp),ht_1            ; ht[1]
    ADD,L   m1_0,m_0,tmp_0           ; tmp_0 = m[0] + m1[0]; 
    ADD,L   m1_1,m_1,tmp_1           ; tmp_1 = m[1] + m1[1]; 

    LDD     -32(%sp),lt_0            
    LDD     -64(%sp),lt_1            
    CMPCLR,*>>= tmp_0,m1_0, %r0      ; if (m[0] < m1[0])
    ADD,L   ht_0,top_overflow,ht_0   ; ht[0] += (1<<32)

    CMPCLR,*>>= tmp_1,m1_1,%r0       ; if (m[1] < m1[1])
    ADD,L   ht_1,top_overflow,ht_1   ; ht[1] += (1<<32)
    EXTRD,U tmp_0,31,32,m_0          ; m[0]>>32  
    DEPD,Z  tmp_0,31,32,m1_0         ; m1[0] = m[0]<<32 

    EXTRD,U tmp_1,31,32,m_1          ; m[1]>>32  
    DEPD,Z  tmp_1,31,32,m1_1         ; m1[1] = m[1]<<32 
    ADD,L   ht_0,m_0,ht_0            ; ht[0]+= (m[0]>>32)
    ADD,L   ht_1,m_1,ht_1            ; ht[1]+= (m[1]>>32)

    ADD     lt_0,m1_0,lt_0           ; lt[0] = lt[0]+m1[0];
	ADD,DC  ht_0,%r0,ht_0            ; ht[0]++
    ADD     lt_1,m1_1,lt_1           ; lt[1] = lt[1]+m1[1];
    ADD,DC  ht_1,%r0,ht_1            ; ht[1]++

    ADD    %ret0,lt_0,lt_0           ; lt[0] = lt[0] + c;
	ADD,DC  ht_0,%r0,ht_0            ; ht[0]++
    ADD     lt_0,rp_val,lt_0         ; lt[0] = lt[0]+rp[0]
    ADD,DC  ht_0,%r0,ht_0            ; ht[0]++

	LDO    -2(num),num               ; num = num - 2;
    ADD     ht_0,lt_1,lt_1           ; lt[1] = lt[1] + ht_0 (c);
    ADD,DC  ht_1,%r0,ht_1            ; ht[1]++
    STD     lt_0,0(r_ptr)            ; rp[0] = lt[0]

    ADD     lt_1,rp_val_1,lt_1       ; lt[1] = lt[1]+rp[1]
    ADD,DC  ht_1,%r0,%ret0           ; ht[1]++
    LDO     16(a_ptr),a_ptr          ; a_ptr += 2

    STD     lt_1,8(r_ptr)            ; rp[1] = lt[1]
	CMPIB,<= 2,num,bn_mul_add_words_unroll2 ; go again if more to do
    LDO     16(r_ptr),r_ptr          ; r_ptr += 2

    CMPIB,=,N 0,num,bn_mul_add_words_exit ; are we done, or cleanup last one

	;
	; Top of loop aligned on 64-byte boundary
	;
bn_mul_add_words_single_top
    FLDD    0(a_ptr),t_float_0        ; load up 64-bit value (fr8L) ht(L)/lt(R)
    LDD     0(r_ptr),rp_val           ; rp[0]
    LDO     8(a_ptr),a_ptr            ; a_ptr++
    XMPYU   fht_0,fw_l,fm1            ; m1 = ht*fw_l
    FSTD    fm1,-16(%sp)              ; -16(sp) = m1
    XMPYU   flt_0,fw_h,fm             ; m = lt*fw_h
    FSTD    fm,-8(%sp)                ; -8(sp) = m
    XMPYU   fht_0,fw_h,ht_temp        ; ht_temp = ht*fw_h
    FSTD    ht_temp,-24(%sp)          ; -24(sp) = ht
    XMPYU   flt_0,fw_l,lt_temp        ; lt_temp = lt*fw_l
    FSTD    lt_temp,-32(%sp)          ; -32(sp) = lt 

    LDD     -8(%sp),m_0               
    LDD    -16(%sp),m1_0              ; m1 = temp1 
    ADD,L   m_0,m1_0,tmp_0            ; tmp_0 = m + m1; 
    LDD     -24(%sp),ht_0             
    LDD     -32(%sp),lt_0             

    CMPCLR,*>>= tmp_0,m1_0,%r0        ; if (m < m1)
    ADD,L   ht_0,top_overflow,ht_0    ; ht += (1<<32)

    EXTRD,U tmp_0,31,32,m_0           ; m>>32  
    DEPD,Z  tmp_0,31,32,m1_0          ; m1 = m<<32 

    ADD,L   ht_0,m_0,ht_0             ; ht+= (m>>32)
    ADD     lt_0,m1_0,tmp_0           ; tmp_0 = lt+m1;
    ADD,DC  ht_0,%r0,ht_0             ; ht++
    ADD     %ret0,tmp_0,lt_0          ; lt = lt + c;
    ADD,DC  ht_0,%r0,ht_0             ; ht++
    ADD     lt_0,rp_val,lt_0          ; lt = lt+rp[0]
    ADD,DC  ht_0,%r0,%ret0            ; ht++
    STD     lt_0,0(r_ptr)             ; rp[0] = lt

bn_mul_add_words_exit
    .EXIT
    LDD     -80(%sp),%r9              ; restore r9  
    LDD     -88(%sp),%r8              ; restore r8  
    LDD     -96(%sp),%r7              ; restore r7  
    LDD     -104(%sp),%r6             ; restore r6  
    LDD     -112(%sp),%r5             ; restore r5  
    LDD     -120(%sp),%r4             ; restore r4  
    BVE     (%rp)
    LDD,MB  -128(%sp),%r3             ; restore r3
	.PROCEND	;in=23,24,25,26,29;out=28;

;----------------------------------------------------------------------------
;
;BN_ULONG bn_mul_words(BN_ULONG *rp, BN_ULONG *ap, int num, BN_ULONG w)
;
; arg0 = rp
; arg1 = ap
; arg2 = num
; arg3 = w

bn_mul_words
	.proc
	.callinfo frame=128
    .entry
	.EXPORT	bn_mul_words,ENTRY,PRIV_LEV=3,NO_RELOCATION,LONG_RETURN
	.align 64

    STD     %r3,0(%sp)          ; save r3  
    STD     %r4,8(%sp)          ; save r4  
    STD     %r5,16(%sp)         ; save r5  
    STD     %r6,24(%sp)         ; save r6  

    STD     %r7,32(%sp)         ; save r7  
    COPY    %r0,%ret0           ; return 0 by default
    DEPDI,Z 1,31,1,top_overflow ; top_overflow = 1 << 32    
	STD     w,56(%sp)           ; w on stack

    CMPIB,>= 0,num,bn_mul_words_exit
	LDO     128(%sp),%sp       ; bump stack

	;
	; See if only 1 word to do, thus just do cleanup
	;
	CMPIB,= 1,num,bn_mul_words_single_top
	FLDD    -72(%sp),fw     ; load up w into fp register fw (fw_h/fw_l)

	;
	; This loop is unrolled 2 times (64-byte aligned as well)
	;
	; PA-RISC 2.0 chips have two fully pipelined multipliers, thus
    ; two 32-bit mutiplies can be issued per cycle.
    ; 
bn_mul_words_unroll2

    FLDD    0(a_ptr),t_float_0        ; load up 64-bit value (fr8L) ht(L)/lt(R)
    FLDD    8(a_ptr),t_float_1        ; load up 64-bit value (fr8L) ht(L)/lt(R)
    XMPYU   fht_0,fw_l,fm1            ; m1[0] = fht_0*fw_l
    XMPYU   fht_1,fw_l,fm1_1          ; m1[1] = ht*fw_l

    FSTD    fm1,-16(%sp)              ; -16(sp) = m1
    FSTD    fm1_1,-48(%sp)            ; -48(sp) = m1
    XMPYU   flt_0,fw_h,fm             ; m = lt*fw_h
    XMPYU   flt_1,fw_h,fm_1           ; m = lt*fw_h

    FSTD    fm,-8(%sp)                ; -8(sp) = m
    FSTD    fm_1,-40(%sp)             ; -40(sp) = m
    XMPYU   fht_0,fw_h,ht_temp        ; ht_temp = fht_0*fw_h
    XMPYU   fht_1,fw_h,ht_temp_1      ; ht_temp = ht*fw_h

    FSTD    ht_temp,-24(%sp)          ; -24(sp) = ht
    FSTD    ht_temp_1,-56(%sp)        ; -56(sp) = ht
    XMPYU   flt_0,fw_l,lt_temp        ; lt_temp = lt*fw_l
    XMPYU   flt_1,fw_l,lt_temp_1      ; lt_temp = lt*fw_l

    FSTD    lt_temp,-32(%sp)          ; -32(sp) = lt 
    FSTD    lt_temp_1,-64(%sp)        ; -64(sp) = lt 
    LDD     -8(%sp),m_0               
    LDD     -40(%sp),m_1              

    LDD    -16(%sp),m1_0              
    LDD    -48(%sp),m1_1              
    LDD     -24(%sp),ht_0             
    LDD     -56(%sp),ht_1             

    ADD,L   m1_0,m_0,tmp_0            ; tmp_0 = m + m1; 
    ADD,L   m1_1,m_1,tmp_1            ; tmp_1 = m + m1; 
    LDD     -32(%sp),lt_0             
    LDD     -64(%sp),lt_1             

    CMPCLR,*>>= tmp_0,m1_0, %r0       ; if (m < m1)
    ADD,L   ht_0,top_overflow,ht_0    ; ht += (1<<32)
    CMPCLR,*>>= tmp_1,m1_1,%r0        ; if (m < m1)
    ADD,L   ht_1,top_overflow,ht_1    ; ht += (1<<32)

    EXTRD,U tmp_0,31,32,m_0           ; m>>32  
    DEPD,Z  tmp_0,31,32,m1_0          ; m1 = m<<32 
    EXTRD,U tmp_1,31,32,m_1           ; m>>32  
    DEPD,Z  tmp_1,31,32,m1_1          ; m1 = m<<32 

    ADD,L   ht_0,m_0,ht_0             ; ht+= (m>>32)
    ADD,L   ht_1,m_1,ht_1             ; ht+= (m>>32)
    ADD     lt_0,m1_0,lt_0            ; lt = lt+m1;
	ADD,DC  ht_0,%r0,ht_0             ; ht++

    ADD     lt_1,m1_1,lt_1            ; lt = lt+m1;
    ADD,DC  ht_1,%r0,ht_1             ; ht++
    ADD    %ret0,lt_0,lt_0            ; lt = lt + c (ret0);
	ADD,DC  ht_0,%r0,ht_0             ; ht++

    ADD     ht_0,lt_1,lt_1            ; lt = lt + c (ht_0)
    ADD,DC  ht_1,%r0,ht_1             ; ht++
    STD     lt_0,0(r_ptr)             ; rp[0] = lt
    STD     lt_1,8(r_ptr)             ; rp[1] = lt

	COPY    ht_1,%ret0                ; carry = ht
	LDO    -2(num),num                ; num = num - 2;
    LDO     16(a_ptr),a_ptr           ; ap += 2
	CMPIB,<= 2,num,bn_mul_words_unroll2
    LDO     16(r_ptr),r_ptr           ; rp++

    CMPIB,=,N 0,num,bn_mul_words_exit ; are we done?

	;
	; Top of loop aligned on 64-byte boundary
	;
bn_mul_words_single_top
    FLDD    0(a_ptr),t_float_0        ; load up 64-bit value (fr8L) ht(L)/lt(R)

    XMPYU   fht_0,fw_l,fm1            ; m1 = ht*fw_l
    FSTD    fm1,-16(%sp)              ; -16(sp) = m1
    XMPYU   flt_0,fw_h,fm             ; m = lt*fw_h
    FSTD    fm,-8(%sp)                ; -8(sp) = m
    XMPYU   fht_0,fw_h,ht_temp        ; ht_temp = ht*fw_h
    FSTD    ht_temp,-24(%sp)          ; -24(sp) = ht
    XMPYU   flt_0,fw_l,lt_temp        ; lt_temp = lt*fw_l
    FSTD    lt_temp,-32(%sp)          ; -32(sp) = lt 

    LDD     -8(%sp),m_0               
    LDD    -16(%sp),m1_0              
    ADD,L   m_0,m1_0,tmp_0            ; tmp_0 = m + m1; 
    LDD     -24(%sp),ht_0             
    LDD     -32(%sp),lt_0             

    CMPCLR,*>>= tmp_0,m1_0,%r0        ; if (m < m1)
    ADD,L   ht_0,top_overflow,ht_0    ; ht += (1<<32)

    EXTRD,U tmp_0,31,32,m_0           ; m>>32  
    DEPD,Z  tmp_0,31,32,m1_0          ; m1 = m<<32 

    ADD,L   ht_0,m_0,ht_0             ; ht+= (m>>32)
    ADD     lt_0,m1_0,lt_0            ; lt= lt+m1;
    ADD,DC  ht_0,%r0,ht_0             ; ht++

    ADD     %ret0,lt_0,lt_0           ; lt = lt + c;
    ADD,DC  ht_0,%r0,ht_0             ; ht++

    COPY    ht_0,%ret0                ; copy carry
    STD     lt_0,0(r_ptr)             ; rp[0] = lt

bn_mul_words_exit
    .EXIT
    LDD     -96(%sp),%r7              ; restore r7  
    LDD     -104(%sp),%r6             ; restore r6  
    LDD     -112(%sp),%r5             ; restore r5  
    LDD     -120(%sp),%r4             ; restore r4  
    BVE     (%rp)
    LDD,MB  -128(%sp),%r3             ; restore r3
	.PROCEND	;in=23,24,25,26,29;out=28;

;----------------------------------------------------------------------------
;
;void bn_sqr_words(BN_ULONG *rp, BN_ULONG *ap, int num)
;
; arg0 = rp
; arg1 = ap
; arg2 = num
;

bn_sqr_words
	.proc
	.callinfo FRAME=128,ENTRY_GR=%r3,ARGS_SAVED,ORDERING_AWARE
	.EXPORT	bn_sqr_words,ENTRY,PRIV_LEV=3,NO_RELOCATION,LONG_RETURN
    .entry
	.align 64

    STD     %r3,0(%sp)          ; save r3  
    STD     %r4,8(%sp)          ; save r4  
	NOP
    STD     %r5,16(%sp)         ; save r5  

    CMPIB,>= 0,num,bn_sqr_words_exit
	LDO     128(%sp),%sp       ; bump stack

	;
	; If only 1, the goto straight to cleanup
	;
	CMPIB,= 1,num,bn_sqr_words_single_top
    DEPDI,Z -1,32,33,high_mask   ; Create Mask 0xffffffff80000000L

	;
	; This loop is unrolled 2 times (64-byte aligned as well)
	;

bn_sqr_words_unroll2
    FLDD    0(a_ptr),t_float_0        ; a[0]
    FLDD    8(a_ptr),t_float_1        ; a[1]
    XMPYU   fht_0,flt_0,fm            ; m[0]
    XMPYU   fht_1,flt_1,fm_1          ; m[1]

    FSTD    fm,-24(%sp)               ; store m[0]
    FSTD    fm_1,-56(%sp)             ; store m[1]
    XMPYU   flt_0,flt_0,lt_temp       ; lt[0]
    XMPYU   flt_1,flt_1,lt_temp_1     ; lt[1]

    FSTD    lt_temp,-16(%sp)          ; store lt[0]
    FSTD    lt_temp_1,-48(%sp)        ; store lt[1]
    XMPYU   fht_0,fht_0,ht_temp       ; ht[0]
    XMPYU   fht_1,fht_1,ht_temp_1     ; ht[1]

    FSTD    ht_temp,-8(%sp)           ; store ht[0]
    FSTD    ht_temp_1,-40(%sp)        ; store ht[1]
    LDD     -24(%sp),m_0             
    LDD     -56(%sp),m_1              

    AND     m_0,high_mask,tmp_0       ; m[0] & Mask
    AND     m_1,high_mask,tmp_1       ; m[1] & Mask
    DEPD,Z  m_0,30,31,m_0             ; m[0] << 32+1
    DEPD,Z  m_1,30,31,m_1             ; m[1] << 32+1

    LDD     -16(%sp),lt_0        
    LDD     -48(%sp),lt_1        
    EXTRD,U tmp_0,32,33,tmp_0         ; tmp_0 = m[0]&Mask >> 32-1
    EXTRD,U tmp_1,32,33,tmp_1         ; tmp_1 = m[1]&Mask >> 32-1

    LDD     -8(%sp),ht_0            
    LDD     -40(%sp),ht_1           
    ADD,L   ht_0,tmp_0,ht_0           ; ht[0] += tmp_0
    ADD,L   ht_1,tmp_1,ht_1           ; ht[1] += tmp_1

    ADD     lt_0,m_0,lt_0             ; lt = lt+m
    ADD,DC  ht_0,%r0,ht_0             ; ht[0]++
    STD     lt_0,0(r_ptr)             ; rp[0] = lt[0]
    STD     ht_0,8(r_ptr)             ; rp[1] = ht[1]

    ADD     lt_1,m_1,lt_1             ; lt = lt+m
    ADD,DC  ht_1,%r0,ht_1             ; ht[1]++
    STD     lt_1,16(r_ptr)            ; rp[2] = lt[1]
    STD     ht_1,24(r_ptr)            ; rp[3] = ht[1]

	LDO    -2(num),num                ; num = num - 2;
    LDO     16(a_ptr),a_ptr           ; ap += 2
	CMPIB,<= 2,num,bn_sqr_words_unroll2
    LDO     32(r_ptr),r_ptr           ; rp += 4

    CMPIB,=,N 0,num,bn_sqr_words_exit ; are we done?

	;
	; Top of loop aligned on 64-byte boundary
	;
bn_sqr_words_single_top
    FLDD    0(a_ptr),t_float_0        ; load up 64-bit value (fr8L) ht(L)/lt(R)

    XMPYU   fht_0,flt_0,fm            ; m
    FSTD    fm,-24(%sp)               ; store m

    XMPYU   flt_0,flt_0,lt_temp       ; lt
    FSTD    lt_temp,-16(%sp)          ; store lt

    XMPYU   fht_0,fht_0,ht_temp       ; ht
    FSTD    ht_temp,-8(%sp)           ; store ht

    LDD     -24(%sp),m_0              ; load m
    AND     m_0,high_mask,tmp_0       ; m & Mask
    DEPD,Z  m_0,30,31,m_0             ; m << 32+1
    LDD     -16(%sp),lt_0             ; lt

    LDD     -8(%sp),ht_0              ; ht
    EXTRD,U tmp_0,32,33,tmp_0         ; tmp_0 = m&Mask >> 32-1
    ADD     m_0,lt_0,lt_0             ; lt = lt+m
    ADD,L   ht_0,tmp_0,ht_0           ; ht += tmp_0
    ADD,DC  ht_0,%r0,ht_0             ; ht++

    STD     lt_0,0(r_ptr)             ; rp[0] = lt
    STD     ht_0,8(r_ptr)             ; rp[1] = ht

bn_sqr_words_exit
    .EXIT
    LDD     -112(%sp),%r5       ; restore r5  
    LDD     -120(%sp),%r4       ; restore r4  
    BVE     (%rp)
    LDD,MB  -128(%sp),%r3 
	.PROCEND	;in=23,24,25,26,29;out=28;


;----------------------------------------------------------------------------
;
;BN_ULONG bn_add_words(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n)
;
; arg0 = rp 
; arg1 = ap
; arg2 = bp 
; arg3 = n

t  .reg %r22
b  .reg %r21
l  .reg %r20

bn_add_words
	.proc
    .entry
	.callinfo
	.EXPORT	bn_add_words,ENTRY,PRIV_LEV=3,NO_RELOCATION,LONG_RETURN
	.align 64

    CMPIB,>= 0,n,bn_add_words_exit
    COPY    %r0,%ret0           ; return 0 by default

	;
	; If 2 or more numbers do the loop
	;
	CMPIB,= 1,n,bn_add_words_single_top
	NOP

	;
	; This loop is unrolled 2 times (64-byte aligned as well)
	;
bn_add_words_unroll2
	LDD     0(a_ptr),t
	LDD     0(b_ptr),b
	ADD     t,%ret0,t                    ; t = t+c;
	ADD,DC  %r0,%r0,%ret0                ; set c to carry
	ADD     t,b,l                        ; l = t + b[0]
	ADD,DC  %ret0,%r0,%ret0              ; c+= carry
	STD     l,0(r_ptr)

	LDD     8(a_ptr),t
	LDD     8(b_ptr),b
	ADD     t,%ret0,t                     ; t = t+c;
	ADD,DC  %r0,%r0,%ret0                 ; set c to carry
	ADD     t,b,l                         ; l = t + b[0]
	ADD,DC  %ret0,%r0,%ret0               ; c+= carry
	STD     l,8(r_ptr)

	LDO     -2(n),n
	LDO     16(a_ptr),a_ptr
	LDO     16(b_ptr),b_ptr

	CMPIB,<= 2,n,bn_add_words_unroll2
	LDO     16(r_ptr),r_ptr

    CMPIB,=,N 0,n,bn_add_words_exit ; are we done?

bn_add_words_single_top
	LDD     0(a_ptr),t
	LDD     0(b_ptr),b

	ADD     t,%ret0,t                 ; t = t+c;
	ADD,DC  %r0,%r0,%ret0             ; set c to carry (could use CMPCLR??)
	ADD     t,b,l                     ; l = t + b[0]
	ADD,DC  %ret0,%r0,%ret0           ; c+= carry
	STD     l,0(r_ptr)

bn_add_words_exit
    .EXIT
    BVE     (%rp)
	NOP
	.PROCEND	;in=23,24,25,26,29;out=28;

;----------------------------------------------------------------------------
;
;BN_ULONG bn_sub_words(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b, int n)
;
; arg0 = rp 
; arg1 = ap
; arg2 = bp 
; arg3 = n

t1       .reg %r22
t2       .reg %r21
sub_tmp1 .reg %r20
sub_tmp2 .reg %r19


bn_sub_words
	.proc
	.callinfo 
	.EXPORT	bn_sub_words,ENTRY,PRIV_LEV=3,NO_RELOCATION,LONG_RETURN
    .entry
	.align 64

    CMPIB,>=  0,n,bn_sub_words_exit
    COPY    %r0,%ret0           ; return 0 by default

	;
	; If 2 or more numbers do the loop
	;
	CMPIB,= 1,n,bn_sub_words_single_top
	NOP

	;
	; This loop is unrolled 2 times (64-byte aligned as well)
	;
bn_sub_words_unroll2
	LDD     0(a_ptr),t1
	LDD     0(b_ptr),t2
	SUB     t1,t2,sub_tmp1           ; t3 = t1-t2; 
	SUB     sub_tmp1,%ret0,sub_tmp1  ; t3 = t3- c; 

	CMPCLR,*>> t1,t2,sub_tmp2        ; clear if t1 > t2
	LDO      1(%r0),sub_tmp2
	
	CMPCLR,*= t1,t2,%r0
	COPY    sub_tmp2,%ret0
	STD     sub_tmp1,0(r_ptr)

	LDD     8(a_ptr),t1
	LDD     8(b_ptr),t2
	SUB     t1,t2,sub_tmp1            ; t3 = t1-t2; 
	SUB     sub_tmp1,%ret0,sub_tmp1   ; t3 = t3- c; 
	CMPCLR,*>> t1,t2,sub_tmp2         ; clear if t1 > t2
	LDO      1(%r0),sub_tmp2
	
	CMPCLR,*= t1,t2,%r0
	COPY    sub_tmp2,%ret0
	STD     sub_tmp1,8(r_ptr)

	LDO     -2(n),n
	LDO     16(a_ptr),a_ptr
	LDO     16(b_ptr),b_ptr

	CMPIB,<= 2,n,bn_sub_words_unroll2
	LDO     16(r_ptr),r_ptr

    CMPIB,=,N 0,n,bn_sub_words_exit ; are we done?

bn_sub_words_single_top
	LDD     0(a_ptr),t1
	LDD     0(b_ptr),t2
	SUB     t1,t2,sub_tmp1            ; t3 = t1-t2; 
	SUB     sub_tmp1,%ret0,sub_tmp1   ; t3 = t3- c; 
	CMPCLR,*>> t1,t2,sub_tmp2         ; clear if t1 > t2
	LDO      1(%r0),sub_tmp2
	
	CMPCLR,*= t1,t2,%r0
	COPY    sub_tmp2,%ret0

	STD     sub_tmp1,0(r_ptr)

bn_sub_words_exit
    .EXIT
    BVE     (%rp)
	NOP
	.PROCEND	;in=23,24,25,26,29;out=28;

;------------------------------------------------------------------------------
;
; unsigned long bn_div_words(unsigned long h, unsigned long l, unsigned long d)
;
; arg0 = h
; arg1 = l
; arg2 = d
;
; This is mainly just modified assembly from the compiler, thus the
; lack of variable names.
;
;------------------------------------------------------------------------------
bn_div_words
	.proc
	.callinfo CALLER,FRAME=272,ENTRY_GR=%r10,SAVE_RP,ARGS_SAVED,ORDERING_AWARE
	.EXPORT	bn_div_words,ENTRY,PRIV_LEV=3,NO_RELOCATION,LONG_RETURN
	.IMPORT	BN_num_bits_word,CODE,NO_RELOCATION
	.IMPORT	__iob,DATA
	.IMPORT	fprintf,CODE,NO_RELOCATION
	.IMPORT	abort,CODE,NO_RELOCATION
	.IMPORT	$$div2U,MILLICODE
    .entry
    STD     %r2,-16(%r30)   
    STD,MA  %r3,352(%r30)   
    STD     %r4,-344(%r30)  
    STD     %r5,-336(%r30)  
    STD     %r6,-328(%r30)  
    STD     %r7,-320(%r30)  
    STD     %r8,-312(%r30)  
    STD     %r9,-304(%r30)  
    STD     %r10,-296(%r30)

    STD     %r27,-288(%r30)             ; save gp

    COPY    %r24,%r3           ; save d 
    COPY    %r26,%r4           ; save h (high 64-bits)
    LDO      -1(%r0),%ret0     ; return -1 by default	

    CMPB,*=  %r0,%arg2,$D3     ; if (d == 0)
    COPY    %r25,%r5           ; save l (low 64-bits)

    LDO     -48(%r30),%r29     ; create ap 
    .CALL   ;in=26,29;out=28;
    B,L     BN_num_bits_word,%r2 
    COPY    %r3,%r26        
    LDD     -288(%r30),%r27    ; restore gp 
    LDI     64,%r21 

    CMPB,=  %r21,%ret0,$00000012   ;if (i == 64) (forward) 
    COPY    %ret0,%r24             ; i   
    MTSARCM %r24    
    DEPDI,Z -1,%sar,1,%r29  
    CMPB,*<<,N %r29,%r4,bn_div_err_case ; if (h > 1<<i) (forward) 

$00000012
    SUBI    64,%r24,%r31                       ; i = 64 - i;
    CMPCLR,*<< %r4,%r3,%r0                     ; if (h >= d)
    SUB     %r4,%r3,%r4                        ; h -= d
    CMPB,=  %r31,%r0,$0000001A                 ; if (i)
    COPY    %r0,%r10                           ; ret = 0
    MTSARCM %r31                               ; i to shift
    DEPD,Z  %r3,%sar,64,%r3                    ; d <<= i;
    SUBI    64,%r31,%r19                       ; 64 - i; redundent
    MTSAR   %r19                               ; (64 -i) to shift
    SHRPD   %r4,%r5,%sar,%r4                   ; l>> (64-i)
    MTSARCM %r31                               ; i to shift
    DEPD,Z  %r5,%sar,64,%r5                    ; l <<= i;

$0000001A
    DEPDI,Z -1,31,32,%r19                      
    EXTRD,U %r3,31,32,%r6                      ; dh=(d&0xfff)>>32
    EXTRD,U %r3,63,32,%r8                      ; dl = d&0xffffff
    LDO     2(%r0),%r9
    STD    %r3,-280(%r30)                      ; "d" to stack

$0000001C
    DEPDI,Z -1,63,32,%r29                      ; 
    EXTRD,U %r4,31,32,%r31                     ; h >> 32
    CMPB,*=,N  %r31,%r6,$D2     	       ; if ((h>>32) != dh)(forward) div
    COPY    %r4,%r26       
    EXTRD,U %r4,31,32,%r25 
    COPY    %r6,%r24      
    .CALL   ;in=23,24,25,26;out=20,21,22,28,29; (MILLICALL)
    B,L     $$div2U,%r2     
    EXTRD,U %r6,31,32,%r23  
    DEPD    %r28,31,32,%r29 
$D2
    STD     %r29,-272(%r30)                   ; q
    AND     %r5,%r19,%r24                   ; t & 0xffffffff00000000;
    EXTRD,U %r24,31,32,%r24                 ; ??? 
    FLDD    -272(%r30),%fr7                 ; q
    FLDD    -280(%r30),%fr8                 ; d
    XMPYU   %fr8L,%fr7L,%fr10  
    FSTD    %fr10,-256(%r30)   
    XMPYU   %fr8L,%fr7R,%fr22  
    FSTD    %fr22,-264(%r30)   
    XMPYU   %fr8R,%fr7L,%fr11 
    XMPYU   %fr8R,%fr7R,%fr23
    FSTD    %fr11,-232(%r30)
    FSTD    %fr23,-240(%r30)
    LDD     -256(%r30),%r28
    DEPD,Z  %r28,31,32,%r2 
    LDD     -264(%r30),%r20
    ADD,L   %r20,%r2,%r31   
    LDD     -232(%r30),%r22 
    DEPD,Z  %r22,31,32,%r22 
    LDD     -240(%r30),%r21 
    B       $00000024       ; enter loop  
    ADD,L   %r21,%r22,%r23 

$0000002A
    LDO     -1(%r29),%r29   
    SUB     %r23,%r8,%r23   
$00000024
    SUB     %r4,%r31,%r25   
    AND     %r25,%r19,%r26  
    CMPB,*<>,N      %r0,%r26,$00000046  ; (forward)
    DEPD,Z  %r25,31,32,%r20 
    OR      %r20,%r24,%r21  
    CMPB,*<<,N  %r21,%r23,$0000002A ;(backward) 
    SUB     %r31,%r6,%r31   
;-------------Break path---------------------

$00000046
    DEPD,Z  %r23,31,32,%r25              ;tl
    EXTRD,U %r23,31,32,%r26              ;t
    AND     %r25,%r19,%r24               ;tl = (tl<<32)&0xfffffff0000000L
    ADD,L   %r31,%r26,%r31               ;th += t; 
    CMPCLR,*>>=     %r5,%r24,%r0         ;if (l<tl)
    LDO     1(%r31),%r31                 ; th++;
    CMPB,*<<=,N     %r31,%r4,$00000036   ;if (n < th) (forward)
    LDO     -1(%r29),%r29                ;q--; 
    ADD,L   %r4,%r3,%r4                  ;h += d;
$00000036
    ADDIB,=,N       -1,%r9,$D1 ;if (--count == 0) break (forward) 
    SUB     %r5,%r24,%r28                ; l -= tl;
    SUB     %r4,%r31,%r24                ; h -= th;
    SHRPD   %r24,%r28,32,%r4             ; h = ((h<<32)|(l>>32));
    DEPD,Z  %r29,31,32,%r10              ; ret = q<<32
    b      $0000001C
    DEPD,Z  %r28,31,32,%r5               ; l = l << 32 

$D1
    OR      %r10,%r29,%r28           ; ret |= q
$D3
    LDD     -368(%r30),%r2  
$D0
    LDD     -296(%r30),%r10 
    LDD     -304(%r30),%r9  
    LDD     -312(%r30),%r8  
    LDD     -320(%r30),%r7  
    LDD     -328(%r30),%r6  
    LDD     -336(%r30),%r5  
    LDD     -344(%r30),%r4  
    BVE     (%r2)   
        .EXIT
    LDD,MB  -352(%r30),%r3 

bn_div_err_case
    MFIA    %r6     
    ADDIL   L'bn_div_words-bn_div_err_case,%r6,%r1 
    LDO     R'bn_div_words-bn_div_err_case(%r1),%r6  
    ADDIL   LT'__iob,%r27,%r1       
    LDD     RT'__iob(%r1),%r26      
    ADDIL   L'C$4-bn_div_words,%r6,%r1    
    LDO     R'C$4-bn_div_words(%r1),%r25  
    LDO     64(%r26),%r26   
    .CALL           ;in=24,25,26,29;out=28;
    B,L     fprintf,%r2    
    LDO     -48(%r30),%r29 
    LDD     -288(%r30),%r27
    .CALL           ;in=29;
    B,L     abort,%r2      
    LDO     -48(%r30),%r29 
    LDD     -288(%r30),%r27
    B       $D0         
    LDD     -368(%r30),%r2  
	.PROCEND	;in=24,25,26,29;out=28;

;----------------------------------------------------------------------------
;
; Registers to hold 64-bit values to manipulate.  The "L" part
; of the register corresponds to the upper 32-bits, while the "R"
; part corresponds to the lower 32-bits
; 
; Note, that when using b6 and b7, the code must save these before
; using them because they are callee save registers 
; 
;
; Floating point registers to use to save values that
; are manipulated.  These don't collide with ftemp1-6 and
; are all caller save registers
;
a0        .reg %fr22
a0L       .reg %fr22L
a0R       .reg %fr22R

a1        .reg %fr23
a1L       .reg %fr23L
a1R       .reg %fr23R

a2        .reg %fr24
a2L       .reg %fr24L
a2R       .reg %fr24R

a3        .reg %fr25
a3L       .reg %fr25L
a3R       .reg %fr25R

a4        .reg %fr26
a4L       .reg %fr26L
a4R       .reg %fr26R

a5        .reg %fr27
a5L       .reg %fr27L
a5R       .reg %fr27R

a6        .reg %fr28
a6L       .reg %fr28L
a6R       .reg %fr28R

a7        .reg %fr29
a7L       .reg %fr29L
a7R       .reg %fr29R

b0        .reg %fr30
b0L       .reg %fr30L
b0R       .reg %fr30R

b1        .reg %fr31
b1L       .reg %fr31L
b1R       .reg %fr31R

;
; Temporary floating point variables, these are all caller save
; registers
;
ftemp1    .reg %fr4
ftemp2    .reg %fr5
ftemp3    .reg %fr6
ftemp4    .reg %fr7

;
; The B set of registers when used.
;

b2        .reg %fr8
b2L       .reg %fr8L
b2R       .reg %fr8R

b3        .reg %fr9
b3L       .reg %fr9L
b3R       .reg %fr9R

b4        .reg %fr10
b4L       .reg %fr10L
b4R       .reg %fr10R

b5        .reg %fr11
b5L       .reg %fr11L
b5R       .reg %fr11R

b6        .reg %fr12
b6L       .reg %fr12L
b6R       .reg %fr12R

b7        .reg %fr13
b7L       .reg %fr13L
b7R       .reg %fr13R

c1           .reg %r21   ; only reg
temp1        .reg %r20   ; only reg
temp2        .reg %r19   ; only reg
temp3        .reg %r31   ; only reg

m1           .reg %r28   
c2           .reg %r23   
high_one     .reg %r1
ht           .reg %r6
lt           .reg %r5
m            .reg %r4
c3           .reg %r3

SQR_ADD_C  .macro  A0L,A0R,C1,C2,C3
    XMPYU   A0L,A0R,ftemp1       ; m
    FSTD    ftemp1,-24(%sp)      ; store m

    XMPYU   A0R,A0R,ftemp2       ; lt
    FSTD    ftemp2,-16(%sp)      ; store lt

    XMPYU   A0L,A0L,ftemp3       ; ht
    FSTD    ftemp3,-8(%sp)       ; store ht

    LDD     -24(%sp),m           ; load m
    AND     m,high_mask,temp2    ; m & Mask
    DEPD,Z  m,30,31,temp3        ; m << 32+1
    LDD     -16(%sp),lt          ; lt

    LDD     -8(%sp),ht           ; ht
    EXTRD,U temp2,32,33,temp1    ; temp1 = m&Mask >> 32-1
    ADD     temp3,lt,lt          ; lt = lt+m
    ADD,L   ht,temp1,ht          ; ht += temp1
    ADD,DC  ht,%r0,ht            ; ht++

    ADD     C1,lt,C1             ; c1=c1+lt
    ADD,DC  ht,%r0,ht            ; ht++

    ADD     C2,ht,C2             ; c2=c2+ht
    ADD,DC  C3,%r0,C3            ; c3++
.endm

SQR_ADD_C2 .macro  A0L,A0R,A1L,A1R,C1,C2,C3
    XMPYU   A0L,A1R,ftemp1          ; m1 = bl*ht
    FSTD    ftemp1,-16(%sp)         ;
    XMPYU   A0R,A1L,ftemp2          ; m = bh*lt
    FSTD    ftemp2,-8(%sp)          ;
    XMPYU   A0R,A1R,ftemp3          ; lt = bl*lt
    FSTD    ftemp3,-32(%sp)
    XMPYU   A0L,A1L,ftemp4          ; ht = bh*ht
    FSTD    ftemp4,-24(%sp)         ;

    LDD     -8(%sp),m               ; r21 = m
    LDD     -16(%sp),m1             ; r19 = m1
    ADD,L   m,m1,m                  ; m+m1

    DEPD,Z  m,31,32,temp3           ; (m+m1<<32)
    LDD     -24(%sp),ht             ; r24 = ht

    CMPCLR,*>>= m,m1,%r0            ; if (m < m1)
    ADD,L   ht,high_one,ht          ; ht+=high_one

    EXTRD,U m,31,32,temp1           ; m >> 32
    LDD     -32(%sp),lt             ; lt
    ADD,L   ht,temp1,ht             ; ht+= m>>32
    ADD     lt,temp3,lt             ; lt = lt+m1
    ADD,DC  ht,%r0,ht               ; ht++

    ADD     ht,ht,ht                ; ht=ht+ht;
    ADD,DC  C3,%r0,C3               ; add in carry (c3++)

    ADD     lt,lt,lt                ; lt=lt+lt;
    ADD,DC  ht,%r0,ht               ; add in carry (ht++)

    ADD     C1,lt,C1                ; c1=c1+lt
    ADD,DC,*NUV ht,%r0,ht           ; add in carry (ht++)
    LDO     1(C3),C3              ; bump c3 if overflow,nullify otherwise

    ADD     C2,ht,C2                ; c2 = c2 + ht
    ADD,DC  C3,%r0,C3             ; add in carry (c3++)
.endm

;
;void bn_sqr_comba8(BN_ULONG *r, BN_ULONG *a)
; arg0 = r_ptr
; arg1 = a_ptr
;

bn_sqr_comba8
	.PROC
	.CALLINFO FRAME=128,ENTRY_GR=%r3,ARGS_SAVED,ORDERING_AWARE
	.EXPORT	bn_sqr_comba8,ENTRY,PRIV_LEV=3,NO_RELOCATION,LONG_RETURN
    .ENTRY
	.align 64

    STD     %r3,0(%sp)          ; save r3
    STD     %r4,8(%sp)          ; save r4
    STD     %r5,16(%sp)         ; save r5
    STD     %r6,24(%sp)         ; save r6

	;
	; Zero out carries
	;
	COPY     %r0,c1
	COPY     %r0,c2
	COPY     %r0,c3

	LDO      128(%sp),%sp       ; bump stack
    DEPDI,Z -1,32,33,high_mask   ; Create Mask 0xffffffff80000000L
    DEPDI,Z  1,31,1,high_one     ; Create Value  1 << 32

	;
	; Load up all of the values we are going to use
	;
    FLDD     0(a_ptr),a0       
    FLDD     8(a_ptr),a1       
    FLDD    16(a_ptr),a2       
    FLDD    24(a_ptr),a3       
    FLDD    32(a_ptr),a4       
    FLDD    40(a_ptr),a5       
    FLDD    48(a_ptr),a6       
    FLDD    56(a_ptr),a7       

	SQR_ADD_C a0L,a0R,c1,c2,c3
	STD     c1,0(r_ptr)          ; r[0] = c1;
	COPY    %r0,c1

	SQR_ADD_C2 a1L,a1R,a0L,a0R,c2,c3,c1
	STD     c2,8(r_ptr)          ; r[1] = c2;
	COPY    %r0,c2

	SQR_ADD_C a1L,a1R,c3,c1,c2
	SQR_ADD_C2 a2L,a2R,a0L,a0R,c3,c1,c2
	STD     c3,16(r_ptr)            ; r[2] = c3;
	COPY    %r0,c3

	SQR_ADD_C2 a3L,a3R,a0L,a0R,c1,c2,c3
	SQR_ADD_C2 a2L,a2R,a1L,a1R,c1,c2,c3
	STD     c1,24(r_ptr)           ; r[3] = c1;
	COPY    %r0,c1

	SQR_ADD_C a2L,a2R,c2,c3,c1
	SQR_ADD_C2 a3L,a3R,a1L,a1R,c2,c3,c1
	SQR_ADD_C2 a4L,a4R,a0L,a0R,c2,c3,c1
	STD     c2,32(r_ptr)          ; r[4] = c2;
	COPY    %r0,c2

	SQR_ADD_C2 a5L,a5R,a0L,a0R,c3,c1,c2
	SQR_ADD_C2 a4L,a4R,a1L,a1R,c3,c1,c2
	SQR_ADD_C2 a3L,a3R,a2L,a2R,c3,c1,c2
	STD     c3,40(r_ptr)          ; r[5] = c3;
	COPY    %r0,c3

	SQR_ADD_C a3L,a3R,c1,c2,c3
	SQR_ADD_C2 a4L,a4R,a2L,a2R,c1,c2,c3
	SQR_ADD_C2 a5L,a5R,a1L,a1R,c1,c2,c3
	SQR_ADD_C2 a6L,a6R,a0L,a0R,c1,c2,c3
	STD     c1,48(r_ptr)          ; r[6] = c1;
	COPY    %r0,c1

	SQR_ADD_C2 a7L,a7R,a0L,a0R,c2,c3,c1
	SQR_ADD_C2 a6L,a6R,a1L,a1R,c2,c3,c1
	SQR_ADD_C2 a5L,a5R,a2L,a2R,c2,c3,c1
	SQR_ADD_C2 a4L,a4R,a3L,a3R,c2,c3,c1
	STD     c2,56(r_ptr)          ; r[7] = c2;
	COPY    %r0,c2

	SQR_ADD_C a4L,a4R,c3,c1,c2
	SQR_ADD_C2 a5L,a5R,a3L,a3R,c3,c1,c2
	SQR_ADD_C2 a6L,a6R,a2L,a2R,c3,c1,c2
	SQR_ADD_C2 a7L,a7R,a1L,a1R,c3,c1,c2
	STD     c3,64(r_ptr)          ; r[8] = c3;
	COPY    %r0,c3

	SQR_ADD_C2 a7L,a7R,a2L,a2R,c1,c2,c3
	SQR_ADD_C2 a6L,a6R,a3L,a3R,c1,c2,c3
	SQR_ADD_C2 a5L,a5R,a4L,a4R,c1,c2,c3
	STD     c1,72(r_ptr)          ; r[9] = c1;
	COPY    %r0,c1

	SQR_ADD_C a5L,a5R,c2,c3,c1
	SQR_ADD_C2 a6L,a6R,a4L,a4R,c2,c3,c1
	SQR_ADD_C2 a7L,a7R,a3L,a3R,c2,c3,c1
	STD     c2,80(r_ptr)          ; r[10] = c2;
	COPY    %r0,c2

	SQR_ADD_C2 a7L,a7R,a4L,a4R,c3,c1,c2
	SQR_ADD_C2 a6L,a6R,a5L,a5R,c3,c1,c2
	STD     c3,88(r_ptr)          ; r[11] = c3;
	COPY    %r0,c3
	
	SQR_ADD_C a6L,a6R,c1,c2,c3
	SQR_ADD_C2 a7L,a7R,a5L,a5R,c1,c2,c3
	STD     c1,96(r_ptr)          ; r[12] = c1;
	COPY    %r0,c1

	SQR_ADD_C2 a7L,a7R,a6L,a6R,c2,c3,c1
	STD     c2,104(r_ptr)         ; r[13] = c2;
	COPY    %r0,c2

	SQR_ADD_C a7L,a7R,c3,c1,c2
	STD     c3, 112(r_ptr)       ; r[14] = c3
	STD     c1, 120(r_ptr)       ; r[15] = c1

    .EXIT
    LDD     -104(%sp),%r6        ; restore r6
    LDD     -112(%sp),%r5        ; restore r5
    LDD     -120(%sp),%r4        ; restore r4
    BVE     (%rp)
    LDD,MB  -128(%sp),%r3

	.PROCEND	

;-----------------------------------------------------------------------------
;
;void bn_sqr_comba4(BN_ULONG *r, BN_ULONG *a)
; arg0 = r_ptr
; arg1 = a_ptr
;

bn_sqr_comba4
	.proc
	.callinfo FRAME=128,ENTRY_GR=%r3,ARGS_SAVED,ORDERING_AWARE
	.EXPORT	bn_sqr_comba4,ENTRY,PRIV_LEV=3,NO_RELOCATION,LONG_RETURN
    .entry
	.align 64
    STD     %r3,0(%sp)          ; save r3
    STD     %r4,8(%sp)          ; save r4
    STD     %r5,16(%sp)         ; save r5
    STD     %r6,24(%sp)         ; save r6

	;
	; Zero out carries
	;
	COPY     %r0,c1
	COPY     %r0,c2
	COPY     %r0,c3

	LDO      128(%sp),%sp       ; bump stack
    DEPDI,Z -1,32,33,high_mask   ; Create Mask 0xffffffff80000000L
    DEPDI,Z  1,31,1,high_one     ; Create Value  1 << 32

	;
	; Load up all of the values we are going to use
	;
    FLDD     0(a_ptr),a0       
    FLDD     8(a_ptr),a1       
    FLDD    16(a_ptr),a2       
    FLDD    24(a_ptr),a3       
    FLDD    32(a_ptr),a4       
    FLDD    40(a_ptr),a5       
    FLDD    48(a_ptr),a6       
    FLDD    56(a_ptr),a7       

	SQR_ADD_C a0L,a0R,c1,c2,c3

	STD     c1,0(r_ptr)          ; r[0] = c1;
	COPY    %r0,c1

	SQR_ADD_C2 a1L,a1R,a0L,a0R,c2,c3,c1

	STD     c2,8(r_ptr)          ; r[1] = c2;
	COPY    %r0,c2

	SQR_ADD_C a1L,a1R,c3,c1,c2
	SQR_ADD_C2 a2L,a2R,a0L,a0R,c3,c1,c2

	STD     c3,16(r_ptr)            ; r[2] = c3;
	COPY    %r0,c3

	SQR_ADD_C2 a3L,a3R,a0L,a0R,c1,c2,c3
	SQR_ADD_C2 a2L,a2R,a1L,a1R,c1,c2,c3

	STD     c1,24(r_ptr)           ; r[3] = c1;
	COPY    %r0,c1

	SQR_ADD_C a2L,a2R,c2,c3,c1
	SQR_ADD_C2 a3L,a3R,a1L,a1R,c2,c3,c1

	STD     c2,32(r_ptr)           ; r[4] = c2;
	COPY    %r0,c2

	SQR_ADD_C2 a3L,a3R,a2L,a2R,c3,c1,c2
	STD     c3,40(r_ptr)           ; r[5] = c3;
	COPY    %r0,c3

	SQR_ADD_C a3L,a3R,c1,c2,c3
	STD     c1,48(r_ptr)           ; r[6] = c1;
	STD     c2,56(r_ptr)           ; r[7] = c2;

    .EXIT
    LDD     -104(%sp),%r6        ; restore r6
    LDD     -112(%sp),%r5        ; restore r5
    LDD     -120(%sp),%r4        ; restore r4
    BVE     (%rp)
    LDD,MB  -128(%sp),%r3

	.PROCEND	


;---------------------------------------------------------------------------

MUL_ADD_C  .macro  A0L,A0R,B0L,B0R,C1,C2,C3
    XMPYU   A0L,B0R,ftemp1        ; m1 = bl*ht
    FSTD    ftemp1,-16(%sp)       ;
    XMPYU   A0R,B0L,ftemp2        ; m = bh*lt
    FSTD    ftemp2,-8(%sp)        ;
    XMPYU   A0R,B0R,ftemp3        ; lt = bl*lt
    FSTD    ftemp3,-32(%sp)
    XMPYU   A0L,B0L,ftemp4        ; ht = bh*ht
    FSTD    ftemp4,-24(%sp)       ;

    LDD     -8(%sp),m             ; r21 = m
    LDD     -16(%sp),m1           ; r19 = m1
    ADD,L   m,m1,m                ; m+m1

    DEPD,Z  m,31,32,temp3         ; (m+m1<<32)
    LDD     -24(%sp),ht           ; r24 = ht

    CMPCLR,*>>= m,m1,%r0          ; if (m < m1)
    ADD,L   ht,high_one,ht        ; ht+=high_one

    EXTRD,U m,31,32,temp1         ; m >> 32
    LDD     -32(%sp),lt           ; lt
    ADD,L   ht,temp1,ht           ; ht+= m>>32
    ADD     lt,temp3,lt           ; lt = lt+m1
    ADD,DC  ht,%r0,ht             ; ht++

    ADD     C1,lt,C1              ; c1=c1+lt
    ADD,DC  ht,%r0,ht             ; bump c3 if overflow,nullify otherwise

    ADD     C2,ht,C2              ; c2 = c2 + ht
    ADD,DC  C3,%r0,C3             ; add in carry (c3++)
.endm


;
;void bn_mul_comba8(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b)
; arg0 = r_ptr
; arg1 = a_ptr
; arg2 = b_ptr
;

bn_mul_comba8
	.proc
	.callinfo FRAME=128,ENTRY_GR=%r3,ARGS_SAVED,ORDERING_AWARE
	.EXPORT	bn_mul_comba8,ENTRY,PRIV_LEV=3,NO_RELOCATION,LONG_RETURN
    .entry
	.align 64

    STD     %r3,0(%sp)          ; save r3
    STD     %r4,8(%sp)          ; save r4
    STD     %r5,16(%sp)         ; save r5
    STD     %r6,24(%sp)         ; save r6
    FSTD    %fr12,32(%sp)       ; save r6
    FSTD    %fr13,40(%sp)       ; save r7

	;
	; Zero out carries
	;
	COPY     %r0,c1
	COPY     %r0,c2
	COPY     %r0,c3

	LDO      128(%sp),%sp       ; bump stack
    DEPDI,Z  1,31,1,high_one     ; Create Value  1 << 32

	;
	; Load up all of the values we are going to use
	;
    FLDD      0(a_ptr),a0       
    FLDD      8(a_ptr),a1       
    FLDD     16(a_ptr),a2       
    FLDD     24(a_ptr),a3       
    FLDD     32(a_ptr),a4       
    FLDD     40(a_ptr),a5       
    FLDD     48(a_ptr),a6       
    FLDD     56(a_ptr),a7       

    FLDD      0(b_ptr),b0       
    FLDD      8(b_ptr),b1       
    FLDD     16(b_ptr),b2       
    FLDD     24(b_ptr),b3       
    FLDD     32(b_ptr),b4       
    FLDD     40(b_ptr),b5       
    FLDD     48(b_ptr),b6       
    FLDD     56(b_ptr),b7       

	MUL_ADD_C a0L,a0R,b0L,b0R,c1,c2,c3
	STD       c1,0(r_ptr)
	COPY      %r0,c1

	MUL_ADD_C a0L,a0R,b1L,b1R,c2,c3,c1
	MUL_ADD_C a1L,a1R,b0L,b0R,c2,c3,c1
	STD       c2,8(r_ptr)
	COPY      %r0,c2

	MUL_ADD_C a2L,a2R,b0L,b0R,c3,c1,c2
	MUL_ADD_C a1L,a1R,b1L,b1R,c3,c1,c2
	MUL_ADD_C a0L,a0R,b2L,b2R,c3,c1,c2
	STD       c3,16(r_ptr)
	COPY      %r0,c3

	MUL_ADD_C a0L,a0R,b3L,b3R,c1,c2,c3
	MUL_ADD_C a1L,a1R,b2L,b2R,c1,c2,c3
	MUL_ADD_C a2L,a2R,b1L,b1R,c1,c2,c3
	MUL_ADD_C a3L,a3R,b0L,b0R,c1,c2,c3
	STD       c1,24(r_ptr)
	COPY      %r0,c1

	MUL_ADD_C a4L,a4R,b0L,b0R,c2,c3,c1
	MUL_ADD_C a3L,a3R,b1L,b1R,c2,c3,c1
	MUL_ADD_C a2L,a2R,b2L,b2R,c2,c3,c1
	MUL_ADD_C a1L,a1R,b3L,b3R,c2,c3,c1
	MUL_ADD_C a0L,a0R,b4L,b4R,c2,c3,c1
	STD       c2,32(r_ptr)
	COPY      %r0,c2

	MUL_ADD_C a0L,a0R,b5L,b5R,c3,c1,c2
	MUL_ADD_C a1L,a1R,b4L,b4R,c3,c1,c2
	MUL_ADD_C a2L,a2R,b3L,b3R,c3,c1,c2
	MUL_ADD_C a3L,a3R,b2L,b2R,c3,c1,c2
	MUL_ADD_C a4L,a4R,b1L,b1R,c3,c1,c2
	MUL_ADD_C a5L,a5R,b0L,b0R,c3,c1,c2
	STD       c3,40(r_ptr)
	COPY      %r0,c3

	MUL_ADD_C a6L,a6R,b0L,b0R,c1,c2,c3
	MUL_ADD_C a5L,a5R,b1L,b1R,c1,c2,c3
	MUL_ADD_C a4L,a4R,b2L,b2R,c1,c2,c3
	MUL_ADD_C a3L,a3R,b3L,b3R,c1,c2,c3
	MUL_ADD_C a2L,a2R,b4L,b4R,c1,c2,c3
	MUL_ADD_C a1L,a1R,b5L,b5R,c1,c2,c3
	MUL_ADD_C a0L,a0R,b6L,b6R,c1,c2,c3
	STD       c1,48(r_ptr)
	COPY      %r0,c1
	
	MUL_ADD_C a0L,a0R,b7L,b7R,c2,c3,c1
	MUL_ADD_C a1L,a1R,b6L,b6R,c2,c3,c1
	MUL_ADD_C a2L,a2R,b5L,b5R,c2,c3,c1
	MUL_ADD_C a3L,a3R,b4L,b4R,c2,c3,c1
	MUL_ADD_C a4L,a4R,b3L,b3R,c2,c3,c1
	MUL_ADD_C a5L,a5R,b2L,b2R,c2,c3,c1
	MUL_ADD_C a6L,a6R,b1L,b1R,c2,c3,c1
	MUL_ADD_C a7L,a7R,b0L,b0R,c2,c3,c1
	STD       c2,56(r_ptr)
	COPY      %r0,c2

	MUL_ADD_C a7L,a7R,b1L,b1R,c3,c1,c2
	MUL_ADD_C a6L,a6R,b2L,b2R,c3,c1,c2
	MUL_ADD_C a5L,a5R,b3L,b3R,c3,c1,c2
	MUL_ADD_C a4L,a4R,b4L,b4R,c3,c1,c2
	MUL_ADD_C a3L,a3R,b5L,b5R,c3,c1,c2
	MUL_ADD_C a2L,a2R,b6L,b6R,c3,c1,c2
	MUL_ADD_C a1L,a1R,b7L,b7R,c3,c1,c2
	STD       c3,64(r_ptr)
	COPY      %r0,c3

	MUL_ADD_C a2L,a2R,b7L,b7R,c1,c2,c3
	MUL_ADD_C a3L,a3R,b6L,b6R,c1,c2,c3
	MUL_ADD_C a4L,a4R,b5L,b5R,c1,c2,c3
	MUL_ADD_C a5L,a5R,b4L,b4R,c1,c2,c3
	MUL_ADD_C a6L,a6R,b3L,b3R,c1,c2,c3
	MUL_ADD_C a7L,a7R,b2L,b2R,c1,c2,c3
	STD       c1,72(r_ptr)
	COPY      %r0,c1

	MUL_ADD_C a7L,a7R,b3L,b3R,c2,c3,c1
	MUL_ADD_C a6L,a6R,b4L,b4R,c2,c3,c1
	MUL_ADD_C a5L,a5R,b5L,b5R,c2,c3,c1
	MUL_ADD_C a4L,a4R,b6L,b6R,c2,c3,c1
	MUL_ADD_C a3L,a3R,b7L,b7R,c2,c3,c1
	STD       c2,80(r_ptr)
	COPY      %r0,c2

	MUL_ADD_C a4L,a4R,b7L,b7R,c3,c1,c2
	MUL_ADD_C a5L,a5R,b6L,b6R,c3,c1,c2
	MUL_ADD_C a6L,a6R,b5L,b5R,c3,c1,c2
	MUL_ADD_C a7L,a7R,b4L,b4R,c3,c1,c2
	STD       c3,88(r_ptr)
	COPY      %r0,c3

	MUL_ADD_C a7L,a7R,b5L,b5R,c1,c2,c3
	MUL_ADD_C a6L,a6R,b6L,b6R,c1,c2,c3
	MUL_ADD_C a5L,a5R,b7L,b7R,c1,c2,c3
	STD       c1,96(r_ptr)
	COPY      %r0,c1

	MUL_ADD_C a6L,a6R,b7L,b7R,c2,c3,c1
	MUL_ADD_C a7L,a7R,b6L,b6R,c2,c3,c1
	STD       c2,104(r_ptr)
	COPY      %r0,c2

	MUL_ADD_C a7L,a7R,b7L,b7R,c3,c1,c2
	STD       c3,112(r_ptr)
	STD       c1,120(r_ptr)

    .EXIT
    FLDD    -88(%sp),%fr13 
    FLDD    -96(%sp),%fr12 
    LDD     -104(%sp),%r6        ; restore r6
    LDD     -112(%sp),%r5        ; restore r5
    LDD     -120(%sp),%r4        ; restore r4
    BVE     (%rp)
    LDD,MB  -128(%sp),%r3

	.PROCEND	

;-----------------------------------------------------------------------------
;
;void bn_mul_comba4(BN_ULONG *r, BN_ULONG *a, BN_ULONG *b)
; arg0 = r_ptr
; arg1 = a_ptr
; arg2 = b_ptr
;

bn_mul_comba4
	.proc
	.callinfo FRAME=128,ENTRY_GR=%r3,ARGS_SAVED,ORDERING_AWARE
	.EXPORT	bn_mul_comba4,ENTRY,PRIV_LEV=3,NO_RELOCATION,LONG_RETURN
    .entry
	.align 64

    STD     %r3,0(%sp)          ; save r3
    STD     %r4,8(%sp)          ; save r4
    STD     %r5,16(%sp)         ; save r5
    STD     %r6,24(%sp)         ; save r6
    FSTD    %fr12,32(%sp)       ; save r6
    FSTD    %fr13,40(%sp)       ; save r7

	;
	; Zero out carries
	;
	COPY     %r0,c1
	COPY     %r0,c2
	COPY     %r0,c3

	LDO      128(%sp),%sp       ; bump stack
    DEPDI,Z  1,31,1,high_one     ; Create Value  1 << 32

	;
	; Load up all of the values we are going to use
	;
    FLDD      0(a_ptr),a0       
    FLDD      8(a_ptr),a1       
    FLDD     16(a_ptr),a2       
    FLDD     24(a_ptr),a3       

    FLDD      0(b_ptr),b0       
    FLDD      8(b_ptr),b1       
    FLDD     16(b_ptr),b2       
    FLDD     24(b_ptr),b3       

	MUL_ADD_C a0L,a0R,b0L,b0R,c1,c2,c3
	STD       c1,0(r_ptr)
	COPY      %r0,c1

	MUL_ADD_C a0L,a0R,b1L,b1R,c2,c3,c1
	MUL_ADD_C a1L,a1R,b0L,b0R,c2,c3,c1
	STD       c2,8(r_ptr)
	COPY      %r0,c2

	MUL_ADD_C a2L,a2R,b0L,b0R,c3,c1,c2
	MUL_ADD_C a1L,a1R,b1L,b1R,c3,c1,c2
	MUL_ADD_C a0L,a0R,b2L,b2R,c3,c1,c2
	STD       c3,16(r_ptr)
	COPY      %r0,c3

	MUL_ADD_C a0L,a0R,b3L,b3R,c1,c2,c3
	MUL_ADD_C a1L,a1R,b2L,b2R,c1,c2,c3
	MUL_ADD_C a2L,a2R,b1L,b1R,c1,c2,c3
	MUL_ADD_C a3L,a3R,b0L,b0R,c1,c2,c3
	STD       c1,24(r_ptr)
	COPY      %r0,c1

	MUL_ADD_C a3L,a3R,b1L,b1R,c2,c3,c1
	MUL_ADD_C a2L,a2R,b2L,b2R,c2,c3,c1
	MUL_ADD_C a1L,a1R,b3L,b3R,c2,c3,c1
	STD       c2,32(r_ptr)
	COPY      %r0,c2

	MUL_ADD_C a2L,a2R,b3L,b3R,c3,c1,c2
	MUL_ADD_C a3L,a3R,b2L,b2R,c3,c1,c2
	STD       c3,40(r_ptr)
	COPY      %r0,c3

	MUL_ADD_C a3L,a3R,b3L,b3R,c1,c2,c3
	STD       c1,48(r_ptr)
	STD       c2,56(r_ptr)

    .EXIT
    FLDD    -88(%sp),%fr13 
    FLDD    -96(%sp),%fr12 
    LDD     -104(%sp),%r6        ; restore r6
    LDD     -112(%sp),%r5        ; restore r5
    LDD     -120(%sp),%r4        ; restore r4
    BVE     (%rp)
    LDD,MB  -128(%sp),%r3

	.PROCEND	


	.SPACE	$TEXT$
	.SUBSPA	$CODE$
	.SPACE	$PRIVATE$,SORT=16
	.IMPORT	$global$,DATA
	.SPACE	$TEXT$
	.SUBSPA	$CODE$
	.SUBSPA	$LIT$,ACCESS=0x2c
C$4
	.ALIGN	8
	.STRINGZ	"Division would overflow (%d)\n"
	.END
