;uInt longest_match_x64(
;    deflate_state *s,
;    IPos cur_match);                             /* current match */

; gvmat64.asm -- Asm portion of the optimized longest_match for 32 bits x86_64
;  (AMD64 on Athlon 64, Opteron, Phenom
;     and Intel EM64T on Pentium 4 with EM64T, Pentium D, Core 2 Duo, Core I5/I7)
; Copyright (C) 1995-2010 Jean-loup Gailly, Brian Raiter and Gilles Vollant.
;
; File written by Gilles Vollant, by converting to assembly the longest_match
;  from Jean-loup Gailly in deflate.c of zLib and infoZip zip.
;
;  and by taking inspiration on asm686 with masm, optimised assembly code
;        from Brian Raiter, written 1998
;
;  This software is provided 'as-is', without any express or implied
;  warranty.  In no event will the authors be held liable for any damages
;  arising from the use of this software.
;
;  Permission is granted to anyone to use this software for any purpose,
;  including commercial applications, and to alter it and redistribute it
;  freely, subject to the following restrictions:
;
;  1. The origin of this software must not be misrepresented; you must not
;     claim that you wrote the original software. If you use this software
;     in a product, an acknowledgment in the product documentation would be
;     appreciated but is not required.
;  2. Altered source versions must be plainly marked as such, and must not be
;     misrepresented as being the original software
;  3. This notice may not be removed or altered from any source distribution.
;
;
;
;         http://www.zlib.net
;         http://www.winimage.com/zLibDll
;         http://www.muppetlabs.com/~breadbox/software/assembly.html
;
; to compile this file for infozip Zip, I use option:
;   ml64.exe /Flgvmat64 /c /Zi /DINFOZIP gvmat64.asm
;
; to compile this file for zLib, I use option:
;   ml64.exe /Flgvmat64 /c /Zi gvmat64.asm
; Be carrefull to adapt zlib1222add below to your version of zLib
;   (if you use a version of zLib before 1.0.4 or after 1.2.2.2, change
;    value of zlib1222add later)
;
; This file compile with Microsoft Macro Assembler (x64) for AMD64
;
;   ml64.exe is given with Visual Studio 2005/2008/2010 and Windows WDK
;
;   (you can get Windows WDK with ml64 for AMD64 from
;      http://www.microsoft.com/whdc/Devtools/wdk/default.mspx for low price)
;


;uInt longest_match(s, cur_match)
;    deflate_state *s;
;    IPos cur_match;                             /* current match */
.code
longest_match PROC


;LocalVarsSize   equ 88
 LocalVarsSize   equ 72

; register used : rax,rbx,rcx,rdx,rsi,rdi,r8,r9,r10,r11,r12
; free register :  r14,r15
; register can be saved : rsp

 chainlenwmask   equ  rsp + 8 - LocalVarsSize    ; high word: current chain len
                                                 ; low word: s->wmask
;window          equ  rsp + xx - LocalVarsSize   ; local copy of s->window ; stored in r10
;windowbestlen   equ  rsp + xx - LocalVarsSize   ; s->window + bestlen , use r10+r11
;scanstart       equ  rsp + xx - LocalVarsSize   ; first two bytes of string ; stored in r12w
;scanend         equ  rsp + xx - LocalVarsSize   ; last two bytes of string use ebx
;scanalign       equ  rsp + xx - LocalVarsSize   ; dword-misalignment of string r13
;bestlen         equ  rsp + xx - LocalVarsSize   ; size of best match so far -> r11d
;scan            equ  rsp + xx - LocalVarsSize   ; ptr to string wanting match -> r9
IFDEF INFOZIP
ELSE
 nicematch       equ  (rsp + 16 - LocalVarsSize) ; a good enough match size
ENDIF

save_rdi        equ  rsp + 24 - LocalVarsSize
save_rsi        equ  rsp + 32 - LocalVarsSize
save_rbx        equ  rsp + 40 - LocalVarsSize
save_rbp        equ  rsp + 48 - LocalVarsSize
save_r12        equ  rsp + 56 - LocalVarsSize
save_r13        equ  rsp + 64 - LocalVarsSize
;save_r14        equ  rsp + 72 - LocalVarsSize
;save_r15        equ  rsp + 80 - LocalVarsSize


; summary of register usage
; scanend     ebx
; scanendw    bx
; chainlenwmask   edx
; curmatch    rsi
; curmatchd   esi
; windowbestlen   r8
; scanalign   r9
; scanalignd  r9d
; window      r10
; bestlen     r11
; bestlend    r11d
; scanstart   r12d
; scanstartw  r12w
; scan        r13
; nicematch   r14d
; limit       r15
; limitd      r15d
; prev        rcx

;  all the +4 offsets are due to the addition of pending_buf_size (in zlib
;  in the deflate_state structure since the asm code was first written
;  (if you compile with zlib 1.0.4 or older, remove the +4).
;  Note : these value are good with a 8 bytes boundary pack structure


    MAX_MATCH           equ     258
    MIN_MATCH           equ     3
    MIN_LOOKAHEAD       equ     (MAX_MATCH+MIN_MATCH+1)


;;; Offsets for fields in the deflate_state structure. These numbers
;;; are calculated from the definition of deflate_state, with the
;;; assumption that the compiler will dword-align the fields. (Thus,
;;; changing the definition of deflate_state could easily cause this
;;; program to crash horribly, without so much as a warning at
;;; compile time. Sigh.)

;  all the +zlib1222add offsets are due to the addition of fields
;  in zlib in the deflate_state structure since the asm code was first written
;  (if you compile with zlib 1.0.4 or older, use "zlib1222add equ (-4)").
;  (if you compile with zlib between 1.0.5 and 1.2.2.1, use "zlib1222add equ 0").
;  if you compile with zlib 1.2.2.2 or later , use "zlib1222add equ 8").


IFDEF INFOZIP

_DATA   SEGMENT
COMM    window_size:DWORD
; WMask ; 7fff
COMM    window:BYTE:010040H
COMM    prev:WORD:08000H
; MatchLen : unused
; PrevMatch : unused
COMM    strstart:DWORD
COMM    match_start:DWORD
; Lookahead : ignore
COMM    prev_length:DWORD ; PrevLen
COMM    max_chain_length:DWORD
COMM    good_match:DWORD
COMM    nice_match:DWORD
prev_ad equ OFFSET prev
window_ad equ OFFSET window
nicematch equ nice_match
_DATA ENDS
WMask equ 07fffh

ELSE

  IFNDEF zlib1222add
    zlib1222add equ 8
  ENDIF
dsWSize         equ 56+zlib1222add+(zlib1222add/2)
dsWMask         equ 64+zlib1222add+(zlib1222add/2)
dsWindow        equ 72+zlib1222add
dsPrev          equ 88+zlib1222add
dsMatchLen      equ 128+zlib1222add
dsPrevMatch     equ 132+zlib1222add
dsStrStart      equ 140+zlib1222add
dsMatchStart    equ 144+zlib1222add
dsLookahead     equ 148+zlib1222add
dsPrevLen       equ 152+zlib1222add
dsMaxChainLen   equ 156+zlib1222add
dsGoodMatch     equ 172+zlib1222add
dsNiceMatch     equ 176+zlib1222add

window_size     equ [ rcx + dsWSize]
WMask           equ [ rcx + dsWMask]
window_ad       equ [ rcx + dsWindow]
prev_ad         equ [ rcx + dsPrev]
strstart        equ [ rcx + dsStrStart]
match_start     equ [ rcx + dsMatchStart]
Lookahead       equ [ rcx + dsLookahead] ; 0ffffffffh on infozip
prev_length     equ [ rcx + dsPrevLen]
max_chain_length equ [ rcx + dsMaxChainLen]
good_match      equ [ rcx + dsGoodMatch]
nice_match      equ [ rcx + dsNiceMatch]
ENDIF

; parameter 1 in r8(deflate state s), param 2 in rdx (cur match)

; see http://weblogs.asp.net/oldnewthing/archive/2004/01/14/58579.aspx and
; http://msdn.microsoft.com/library/en-us/kmarch/hh/kmarch/64bitAMD_8e951dd2-ee77-4728-8702-55ce4b5dd24a.xml.asp
;
; All registers must be preserved across the call, except for
;   rax, rcx, rdx, r8, r9, r10, and r11, which are scratch.



;;; Save registers that the compiler may be using, and adjust esp to
;;; make room for our stack frame.


;;; Retrieve the function arguments. r8d will hold cur_match
;;; throughout the entire function. edx will hold the pointer to the
;;; deflate_state structure during the function's setup (before
;;; entering the main loop.

; parameter 1 in rcx (deflate_state* s), param 2 in edx -> r8 (cur match)

; this clear high 32 bits of r8, which can be garbage in both r8 and rdx

        mov [save_rdi],rdi
        mov [save_rsi],rsi
        mov [save_rbx],rbx
        mov [save_rbp],rbp
IFDEF INFOZIP
        mov r8d,ecx
ELSE
        mov r8d,edx
ENDIF
        mov [save_r12],r12
        mov [save_r13],r13
;        mov [save_r14],r14
;        mov [save_r15],r15


;;; uInt wmask = s->w_mask;
;;; unsigned chain_length = s->max_chain_length;
;;; if (s->prev_length >= s->good_match) {
;;;     chain_length >>= 2;
;;; }

        mov edi, prev_length
        mov esi, good_match
        mov eax, WMask
        mov ebx, max_chain_length
        cmp edi, esi
        jl  LastMatchGood
        shr ebx, 2
LastMatchGood:

;;; chainlen is decremented once beforehand so that the function can
;;; use the sign flag instead of the zero flag for the exit test.
;;; It is then shifted into the high word, to make room for the wmask
;;; value, which it will always accompany.

        dec ebx
        shl ebx, 16
        or  ebx, eax

;;; on zlib only
;;; if ((uInt)nice_match > s->lookahead) nice_match = s->lookahead;

IFDEF INFOZIP
        mov [chainlenwmask], ebx
; on infozip nice_match = [nice_match]
ELSE
        mov eax, nice_match
        mov [chainlenwmask], ebx
        mov r10d, Lookahead
        cmp r10d, eax
        cmovnl r10d, eax
        mov [nicematch],r10d
ENDIF

;;; register Bytef *scan = s->window + s->strstart;
        mov r10, window_ad
        mov ebp, strstart
        lea r13, [r10 + rbp]

;;; Determine how many bytes the scan ptr is off from being
;;; dword-aligned.

         mov r9,r13
         neg r13
         and r13,3

;;; IPos limit = s->strstart > (IPos)MAX_DIST(s) ?
;;;     s->strstart - (IPos)MAX_DIST(s) : NIL;
IFDEF INFOZIP
        mov eax,07efah ; MAX_DIST = (WSIZE-MIN_LOOKAHEAD) (0x8000-(3+8+1))
ELSE
        mov eax, window_size
        sub eax, MIN_LOOKAHEAD
ENDIF
        xor edi,edi
        sub ebp, eax

        mov r11d, prev_length

        cmovng ebp,edi

;;; int best_len = s->prev_length;


;;; Store the sum of s->window + best_len in esi locally, and in esi.

       lea  rsi,[r10+r11]

;;; register ush scan_start = *(ushf*)scan;
;;; register ush scan_end   = *(ushf*)(scan+best_len-1);
;;; Posf *prev = s->prev;

        movzx r12d,word ptr [r9]
        movzx ebx, word ptr [r9 + r11 - 1]

        mov rdi, prev_ad

;;; Jump into the main loop.

        mov edx, [chainlenwmask]

        cmp bx,word ptr [rsi + r8 - 1]
        jz  LookupLoopIsZero

LookupLoop1:
        and r8d, edx

        movzx   r8d, word ptr [rdi + r8*2]
        cmp r8d, ebp
        jbe LeaveNow
        sub edx, 00010000h
        js  LeaveNow

LoopEntry1:
        cmp bx,word ptr [rsi + r8 - 1]
        jz  LookupLoopIsZero

LookupLoop2:
        and r8d, edx

        movzx   r8d, word ptr [rdi + r8*2]
        cmp r8d, ebp
        jbe LeaveNow
        sub edx, 00010000h
        js  LeaveNow

LoopEntry2:
        cmp bx,word ptr [rsi + r8 - 1]
        jz  LookupLoopIsZero

LookupLoop4:
        and r8d, edx

        movzx   r8d, word ptr [rdi + r8*2]
        cmp r8d, ebp
        jbe LeaveNow
        sub edx, 00010000h
        js  LeaveNow

LoopEntry4:

        cmp bx,word ptr [rsi + r8 - 1]
        jnz LookupLoop1
        jmp LookupLoopIsZero


;;; do {
;;;     match = s->window + cur_match;
;;;     if (*(ushf*)(match+best_len-1) != scan_end ||
;;;         *(ushf*)match != scan_start) continue;
;;;     [...]
;;; } while ((cur_match = prev[cur_match & wmask]) > limit
;;;          && --chain_length != 0);
;;;
;;; Here is the inner loop of the function. The function will spend the
;;; majority of its time in this loop, and majority of that time will
;;; be spent in the first ten instructions.
;;;
;;; Within this loop:
;;; ebx = scanend
;;; r8d = curmatch
;;; edx = chainlenwmask - i.e., ((chainlen << 16) | wmask)
;;; esi = windowbestlen - i.e., (window + bestlen)
;;; edi = prev
;;; ebp = limit

LookupLoop:
        and r8d, edx

        movzx   r8d, word ptr [rdi + r8*2]
        cmp r8d, ebp
        jbe LeaveNow
        sub edx, 00010000h
        js  LeaveNow

LoopEntry:

        cmp bx,word ptr [rsi + r8 - 1]
        jnz LookupLoop1
LookupLoopIsZero:
        cmp     r12w, word ptr [r10 + r8]
        jnz LookupLoop1


;;; Store the current value of chainlen.
        mov [chainlenwmask], edx

;;; Point edi to the string under scrutiny, and esi to the string we
;;; are hoping to match it up with. In actuality, esi and edi are
;;; both pointed (MAX_MATCH_8 - scanalign) bytes ahead, and edx is
;;; initialized to -(MAX_MATCH_8 - scanalign).

        lea rsi,[r8+r10]
        mov rdx, 0fffffffffffffef8h; -(MAX_MATCH_8)
        lea rsi, [rsi + r13 + 0108h] ;MAX_MATCH_8]
        lea rdi, [r9 + r13 + 0108h] ;MAX_MATCH_8]

        prefetcht1 [rsi+rdx]
        prefetcht1 [rdi+rdx]


;;; Test the strings for equality, 8 bytes at a time. At the end,
;;; adjust rdx so that it is offset to the exact byte that mismatched.
;;;
;;; We already know at this point that the first three bytes of the
;;; strings match each other, and they can be safely passed over before
;;; starting the compare loop. So what this code does is skip over 0-3
;;; bytes, as much as necessary in order to dword-align the edi
;;; pointer. (rsi will still be misaligned three times out of four.)
;;;
;;; It should be confessed that this loop usually does not represent
;;; much of the total running time. Replacing it with a more
;;; straightforward "rep cmpsb" would not drastically degrade
;;; performance.


LoopCmps:
        mov rax, [rsi + rdx]
        xor rax, [rdi + rdx]
        jnz LeaveLoopCmps

        mov rax, [rsi + rdx + 8]
        xor rax, [rdi + rdx + 8]
        jnz LeaveLoopCmps8


        mov rax, [rsi + rdx + 8+8]
        xor rax, [rdi + rdx + 8+8]
        jnz LeaveLoopCmps16

        add rdx,8+8+8

        jnz short LoopCmps
        jmp short LenMaximum
LeaveLoopCmps16: add rdx,8
LeaveLoopCmps8: add rdx,8
LeaveLoopCmps:

        test    eax, 0000FFFFh
        jnz LenLower

        test eax,0ffffffffh

        jnz LenLower32

        add rdx,4
        shr rax,32
        or ax,ax
        jnz LenLower

LenLower32:
        shr eax,16
        add rdx,2
LenLower:   sub al, 1
        adc rdx, 0
;;; Calculate the length of the match. If it is longer than MAX_MATCH,
;;; then automatically accept it as the best possible match and leave.

        lea rax, [rdi + rdx]
        sub rax, r9
        cmp eax, MAX_MATCH
        jge LenMaximum

;;; If the length of the match is not longer than the best match we
;;; have so far, then forget it and return to the lookup loop.
;///////////////////////////////////

        cmp eax, r11d
        jg  LongerMatch

        lea rsi,[r10+r11]

        mov rdi, prev_ad
        mov edx, [chainlenwmask]
        jmp LookupLoop

;;;         s->match_start = cur_match;
;;;         best_len = len;
;;;         if (len >= nice_match) break;
;;;         scan_end = *(ushf*)(scan+best_len-1);

LongerMatch:
        mov r11d, eax
        mov match_start, r8d
        cmp eax, [nicematch]
        jge LeaveNow

        lea rsi,[r10+rax]

        movzx   ebx, word ptr [r9 + rax - 1]
        mov rdi, prev_ad
        mov edx, [chainlenwmask]
        jmp LookupLoop

;;; Accept the current string, with the maximum possible length.

LenMaximum:
        mov r11d,MAX_MATCH
        mov match_start, r8d

;;; if ((uInt)best_len <= s->lookahead) return (uInt)best_len;
;;; return s->lookahead;

LeaveNow:
IFDEF INFOZIP
        mov eax,r11d
ELSE
        mov eax, Lookahead
        cmp r11d, eax
        cmovng eax, r11d
ENDIF

;;; Restore the stack and return from whence we came.


        mov rsi,[save_rsi]
        mov rdi,[save_rdi]
        mov rbx,[save_rbx]
        mov rbp,[save_rbp]
        mov r12,[save_r12]
        mov r13,[save_r13]
;        mov r14,[save_r14]
;        mov r15,[save_r15]


        ret 0
; please don't remove this string !
; Your can freely use gvmat64 in any free or commercial app
; but it is far better don't remove the string in the binary!
    db     0dh,0ah,"asm686 with masm, optimised assembly code from Brian Raiter, written 1998, converted to amd 64 by Gilles Vollant 2005",0dh,0ah,0
longest_match   ENDP

match_init PROC
  ret 0
match_init ENDP


END
