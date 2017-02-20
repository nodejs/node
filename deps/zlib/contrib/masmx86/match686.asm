; match686.asm -- Asm portion of the optimized longest_match for 32 bits x86
; Copyright (C) 1995-1996 Jean-loup Gailly, Brian Raiter and Gilles Vollant.
; File written by Gilles Vollant, by converting match686.S from Brian Raiter
; for MASM. This is as assembly version of longest_match
;  from Jean-loup Gailly in deflate.c
;
;         http://www.zlib.net
;         http://www.winimage.com/zLibDll
;         http://www.muppetlabs.com/~breadbox/software/assembly.html
;
; For Visual C++ 4.x and higher and ML 6.x and higher
;   ml.exe is distributed in
;  http://www.microsoft.com/downloads/details.aspx?FamilyID=7a1c9da0-0510-44a2-b042-7ef370530c64
;
; this file contain two implementation of longest_match
;
;  this longest_match was written by Brian raiter (1998), optimized for Pentium Pro
;   (and the faster known version of match_init on modern Core 2 Duo and AMD Phenom)
;
;  for using an assembly version of longest_match, you need define ASMV in project
;
;    compile the asm file running
;           ml /coff /Zi /c /Flmatch686.lst match686.asm
;    and do not include match686.obj in your project
;
; note: contrib of zLib 1.2.3 and earlier contained both a deprecated version for
;  Pentium (prior Pentium Pro) and this version for Pentium Pro and modern processor
;  with autoselect (with cpu detection code)
;  if you want support the old pentium optimization, you can still use these version
;
; this file is not optimized for old pentium, but it compatible with all x86 32 bits
; processor (starting 80386)
;
;
; see below : zlib1222add must be adjuster if you use a zlib version < 1.2.2.2

;uInt longest_match(s, cur_match)
;    deflate_state *s;
;    IPos cur_match;                             /* current match */

    NbStack         equ     76
    cur_match       equ     dword ptr[esp+NbStack-0]
    str_s           equ     dword ptr[esp+NbStack-4]
; 5 dword on top (ret,ebp,esi,edi,ebx)
    adrret          equ     dword ptr[esp+NbStack-8]
    pushebp         equ     dword ptr[esp+NbStack-12]
    pushedi         equ     dword ptr[esp+NbStack-16]
    pushesi         equ     dword ptr[esp+NbStack-20]
    pushebx         equ     dword ptr[esp+NbStack-24]

    chain_length    equ     dword ptr [esp+NbStack-28]
    limit           equ     dword ptr [esp+NbStack-32]
    best_len        equ     dword ptr [esp+NbStack-36]
    window          equ     dword ptr [esp+NbStack-40]
    prev            equ     dword ptr [esp+NbStack-44]
    scan_start      equ      word ptr [esp+NbStack-48]
    wmask           equ     dword ptr [esp+NbStack-52]
    match_start_ptr equ     dword ptr [esp+NbStack-56]
    nice_match      equ     dword ptr [esp+NbStack-60]
    scan            equ     dword ptr [esp+NbStack-64]

    windowlen       equ     dword ptr [esp+NbStack-68]
    match_start     equ     dword ptr [esp+NbStack-72]
    strend          equ     dword ptr [esp+NbStack-76]
    NbStackAdd      equ     (NbStack-24)

    .386p

    name    gvmatch
    .MODEL  FLAT



;  all the +zlib1222add offsets are due to the addition of fields
;  in zlib in the deflate_state structure since the asm code was first written
;  (if you compile with zlib 1.0.4 or older, use "zlib1222add equ (-4)").
;  (if you compile with zlib between 1.0.5 and 1.2.2.1, use "zlib1222add equ 0").
;  if you compile with zlib 1.2.2.2 or later , use "zlib1222add equ 8").

    zlib1222add         equ     8

;  Note : these value are good with a 8 bytes boundary pack structure
    dep_chain_length    equ     74h+zlib1222add
    dep_window          equ     30h+zlib1222add
    dep_strstart        equ     64h+zlib1222add
    dep_prev_length     equ     70h+zlib1222add
    dep_nice_match      equ     88h+zlib1222add
    dep_w_size          equ     24h+zlib1222add
    dep_prev            equ     38h+zlib1222add
    dep_w_mask          equ     2ch+zlib1222add
    dep_good_match      equ     84h+zlib1222add
    dep_match_start     equ     68h+zlib1222add
    dep_lookahead       equ     6ch+zlib1222add


_TEXT                   segment

IFDEF NOUNDERLINE
            public  longest_match
            public  match_init
ELSE
            public  _longest_match
            public  _match_init
ENDIF

    MAX_MATCH           equ     258
    MIN_MATCH           equ     3
    MIN_LOOKAHEAD       equ     (MAX_MATCH+MIN_MATCH+1)



MAX_MATCH       equ     258
MIN_MATCH       equ     3
MIN_LOOKAHEAD   equ     (MAX_MATCH + MIN_MATCH + 1)
MAX_MATCH_8_     equ     ((MAX_MATCH + 7) AND 0FFF0h)


;;; stack frame offsets

chainlenwmask   equ  esp + 0    ; high word: current chain len
                    ; low word: s->wmask
window      equ  esp + 4    ; local copy of s->window
windowbestlen   equ  esp + 8    ; s->window + bestlen
scanstart   equ  esp + 16   ; first two bytes of string
scanend     equ  esp + 12   ; last two bytes of string
scanalign   equ  esp + 20   ; dword-misalignment of string
nicematch   equ  esp + 24   ; a good enough match size
bestlen     equ  esp + 28   ; size of best match so far
scan        equ  esp + 32   ; ptr to string wanting match

LocalVarsSize   equ 36
;   saved ebx   byte esp + 36
;   saved edi   byte esp + 40
;   saved esi   byte esp + 44
;   saved ebp   byte esp + 48
;   return address  byte esp + 52
deflatestate    equ  esp + 56   ; the function arguments
curmatch    equ  esp + 60

;;; Offsets for fields in the deflate_state structure. These numbers
;;; are calculated from the definition of deflate_state, with the
;;; assumption that the compiler will dword-align the fields. (Thus,
;;; changing the definition of deflate_state could easily cause this
;;; program to crash horribly, without so much as a warning at
;;; compile time. Sigh.)

dsWSize     equ 36+zlib1222add
dsWMask     equ 44+zlib1222add
dsWindow    equ 48+zlib1222add
dsPrev      equ 56+zlib1222add
dsMatchLen  equ 88+zlib1222add
dsPrevMatch equ 92+zlib1222add
dsStrStart  equ 100+zlib1222add
dsMatchStart    equ 104+zlib1222add
dsLookahead equ 108+zlib1222add
dsPrevLen   equ 112+zlib1222add
dsMaxChainLen   equ 116+zlib1222add
dsGoodMatch equ 132+zlib1222add
dsNiceMatch equ 136+zlib1222add


;;; match686.asm -- Pentium-Pro-optimized version of longest_match()
;;; Written for zlib 1.1.2
;;; Copyright (C) 1998 Brian Raiter <breadbox@muppetlabs.com>
;;; You can look at http://www.muppetlabs.com/~breadbox/software/assembly.html
;;;
;;
;;  This software is provided 'as-is', without any express or implied
;;  warranty.  In no event will the authors be held liable for any damages
;;  arising from the use of this software.
;;
;;  Permission is granted to anyone to use this software for any purpose,
;;  including commercial applications, and to alter it and redistribute it
;;  freely, subject to the following restrictions:
;;
;;  1. The origin of this software must not be misrepresented; you must not
;;     claim that you wrote the original software. If you use this software
;;     in a product, an acknowledgment in the product documentation would be
;;     appreciated but is not required.
;;  2. Altered source versions must be plainly marked as such, and must not be
;;     misrepresented as being the original software
;;  3. This notice may not be removed or altered from any source distribution.
;;

;GLOBAL _longest_match, _match_init


;SECTION    .text

;;; uInt longest_match(deflate_state *deflatestate, IPos curmatch)

;_longest_match:
    IFDEF NOUNDERLINE
    longest_match       proc near
    ELSE
    _longest_match      proc near
    ENDIF
.FPO (9, 4, 0, 0, 1, 0)

;;; Save registers that the compiler may be using, and adjust esp to
;;; make room for our stack frame.

        push    ebp
        push    edi
        push    esi
        push    ebx
        sub esp, LocalVarsSize

;;; Retrieve the function arguments. ecx will hold cur_match
;;; throughout the entire function. edx will hold the pointer to the
;;; deflate_state structure during the function's setup (before
;;; entering the main loop.

        mov edx, [deflatestate]
        mov ecx, [curmatch]

;;; uInt wmask = s->w_mask;
;;; unsigned chain_length = s->max_chain_length;
;;; if (s->prev_length >= s->good_match) {
;;;     chain_length >>= 2;
;;; }

        mov eax, [edx + dsPrevLen]
        mov ebx, [edx + dsGoodMatch]
        cmp eax, ebx
        mov eax, [edx + dsWMask]
        mov ebx, [edx + dsMaxChainLen]
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
        mov [chainlenwmask], ebx

;;; if ((uInt)nice_match > s->lookahead) nice_match = s->lookahead;

        mov eax, [edx + dsNiceMatch]
        mov ebx, [edx + dsLookahead]
        cmp ebx, eax
        jl  LookaheadLess
        mov ebx, eax
LookaheadLess:  mov [nicematch], ebx

;;; register Bytef *scan = s->window + s->strstart;

        mov esi, [edx + dsWindow]
        mov [window], esi
        mov ebp, [edx + dsStrStart]
        lea edi, [esi + ebp]
        mov [scan], edi

;;; Determine how many bytes the scan ptr is off from being
;;; dword-aligned.

        mov eax, edi
        neg eax
        and eax, 3
        mov [scanalign], eax

;;; IPos limit = s->strstart > (IPos)MAX_DIST(s) ?
;;;     s->strstart - (IPos)MAX_DIST(s) : NIL;

        mov eax, [edx + dsWSize]
        sub eax, MIN_LOOKAHEAD
        sub ebp, eax
        jg  LimitPositive
        xor ebp, ebp
LimitPositive:

;;; int best_len = s->prev_length;

        mov eax, [edx + dsPrevLen]
        mov [bestlen], eax

;;; Store the sum of s->window + best_len in esi locally, and in esi.

        add esi, eax
        mov [windowbestlen], esi

;;; register ush scan_start = *(ushf*)scan;
;;; register ush scan_end   = *(ushf*)(scan+best_len-1);
;;; Posf *prev = s->prev;

        movzx   ebx, word ptr [edi]
        mov [scanstart], ebx
        movzx   ebx, word ptr [edi + eax - 1]
        mov [scanend], ebx
        mov edi, [edx + dsPrev]

;;; Jump into the main loop.

        mov edx, [chainlenwmask]
        jmp short LoopEntry

align 4

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
;;; ecx = curmatch
;;; edx = chainlenwmask - i.e., ((chainlen << 16) | wmask)
;;; esi = windowbestlen - i.e., (window + bestlen)
;;; edi = prev
;;; ebp = limit

LookupLoop:
        and ecx, edx
        movzx   ecx, word ptr [edi + ecx*2]
        cmp ecx, ebp
        jbe LeaveNow
        sub edx, 00010000h
        js  LeaveNow
LoopEntry:  movzx   eax, word ptr [esi + ecx - 1]
        cmp eax, ebx
        jnz LookupLoop
        mov eax, [window]
        movzx   eax, word ptr [eax + ecx]
        cmp eax, [scanstart]
        jnz LookupLoop

;;; Store the current value of chainlen.

        mov [chainlenwmask], edx

;;; Point edi to the string under scrutiny, and esi to the string we
;;; are hoping to match it up with. In actuality, esi and edi are
;;; both pointed (MAX_MATCH_8 - scanalign) bytes ahead, and edx is
;;; initialized to -(MAX_MATCH_8 - scanalign).

        mov esi, [window]
        mov edi, [scan]
        add esi, ecx
        mov eax, [scanalign]
        mov edx, 0fffffef8h; -(MAX_MATCH_8)
        lea edi, [edi + eax + 0108h] ;MAX_MATCH_8]
        lea esi, [esi + eax + 0108h] ;MAX_MATCH_8]

;;; Test the strings for equality, 8 bytes at a time. At the end,
;;; adjust edx so that it is offset to the exact byte that mismatched.
;;;
;;; We already know at this point that the first three bytes of the
;;; strings match each other, and they can be safely passed over before
;;; starting the compare loop. So what this code does is skip over 0-3
;;; bytes, as much as necessary in order to dword-align the edi
;;; pointer. (esi will still be misaligned three times out of four.)
;;;
;;; It should be confessed that this loop usually does not represent
;;; much of the total running time. Replacing it with a more
;;; straightforward "rep cmpsb" would not drastically degrade
;;; performance.

LoopCmps:
        mov eax, [esi + edx]
        xor eax, [edi + edx]
        jnz LeaveLoopCmps
        mov eax, [esi + edx + 4]
        xor eax, [edi + edx + 4]
        jnz LeaveLoopCmps4
        add edx, 8
        jnz LoopCmps
        jmp short LenMaximum
LeaveLoopCmps4: add edx, 4
LeaveLoopCmps:  test    eax, 0000FFFFh
        jnz LenLower
        add edx,  2
        shr eax, 16
LenLower:   sub al, 1
        adc edx, 0

;;; Calculate the length of the match. If it is longer than MAX_MATCH,
;;; then automatically accept it as the best possible match and leave.

        lea eax, [edi + edx]
        mov edi, [scan]
        sub eax, edi
        cmp eax, MAX_MATCH
        jge LenMaximum

;;; If the length of the match is not longer than the best match we
;;; have so far, then forget it and return to the lookup loop.

        mov edx, [deflatestate]
        mov ebx, [bestlen]
        cmp eax, ebx
        jg  LongerMatch
        mov esi, [windowbestlen]
        mov edi, [edx + dsPrev]
        mov ebx, [scanend]
        mov edx, [chainlenwmask]
        jmp LookupLoop

;;;         s->match_start = cur_match;
;;;         best_len = len;
;;;         if (len >= nice_match) break;
;;;         scan_end = *(ushf*)(scan+best_len-1);

LongerMatch:    mov ebx, [nicematch]
        mov [bestlen], eax
        mov [edx + dsMatchStart], ecx
        cmp eax, ebx
        jge LeaveNow
        mov esi, [window]
        add esi, eax
        mov [windowbestlen], esi
        movzx   ebx, word ptr [edi + eax - 1]
        mov edi, [edx + dsPrev]
        mov [scanend], ebx
        mov edx, [chainlenwmask]
        jmp LookupLoop

;;; Accept the current string, with the maximum possible length.

LenMaximum: mov edx, [deflatestate]
        mov dword ptr [bestlen], MAX_MATCH
        mov [edx + dsMatchStart], ecx

;;; if ((uInt)best_len <= s->lookahead) return (uInt)best_len;
;;; return s->lookahead;

LeaveNow:
        mov edx, [deflatestate]
        mov ebx, [bestlen]
        mov eax, [edx + dsLookahead]
        cmp ebx, eax
        jg  LookaheadRet
        mov eax, ebx
LookaheadRet:

;;; Restore the stack and return from whence we came.

        add esp, LocalVarsSize
        pop ebx
        pop esi
        pop edi
        pop ebp

        ret
; please don't remove this string !
; Your can freely use match686 in any free or commercial app if you don't remove the string in the binary!
    db     0dh,0ah,"asm686 with masm, optimised assembly code from Brian Raiter, written 1998",0dh,0ah


    IFDEF NOUNDERLINE
    longest_match       endp
    ELSE
    _longest_match      endp
    ENDIF

    IFDEF NOUNDERLINE
    match_init      proc near
                    ret
    match_init      endp
    ELSE
    _match_init     proc near
                    ret
    _match_init     endp
    ENDIF


_TEXT   ends
end
