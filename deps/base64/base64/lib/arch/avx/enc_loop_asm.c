// Apologies in advance for combining the preprocessor with inline assembly,
// two notoriously gnarly parts of C, but it was necessary to avoid a lot of
// code repetition. The preprocessor is used to template large sections of
// inline assembly that differ only in the registers used. If the code was
// written out by hand, it would become very large and hard to audit.

// Generate a block of inline assembly that loads register R0 from memory. The
// offset at which the register is loaded is set by the given round.
#define LOAD(R0, ROUND) \
	"vlddqu ("#ROUND" * 12)(%[src]), %["R0"] \n\t"

// Generate a block of inline assembly that deinterleaves and shuffles register
// R0 using preloaded constants. Outputs in R0 and R1.
#define SHUF(R0, R1, R2) \
	"vpshufb  %[lut0], %["R0"], %["R1"] \n\t" \
	"vpand    %["R1"], %[msk0], %["R2"] \n\t" \
	"vpand    %["R1"], %[msk2], %["R1"] \n\t" \
	"vpmulhuw %["R2"], %[msk1], %["R2"] \n\t" \
	"vpmullw  %["R1"], %[msk3], %["R1"] \n\t" \
	"vpor     %["R1"], %["R2"], %["R1"] \n\t"

// Generate a block of inline assembly that takes R0 and R1 and translates
// their contents to the base64 alphabet, using preloaded constants.
#define TRAN(R0, R1, R2) \
	"vpsubusb %[n51],  %["R1"], %["R0"] \n\t" \
	"vpcmpgtb %[n25],  %["R1"], %["R2"] \n\t" \
	"vpsubb   %["R2"], %["R0"], %["R0"] \n\t" \
	"vpshufb  %["R0"], %[lut1], %["R2"] \n\t" \
	"vpaddb   %["R1"], %["R2"], %["R0"] \n\t"

// Generate a block of inline assembly that stores the given register R0 at an
// offset set by the given round.
#define STOR(R0, ROUND) \
	"vmovdqu %["R0"], ("#ROUND" * 16)(%[dst]) \n\t"

// Generate a block of inline assembly that generates a single self-contained
// encoder round: fetch the data, process it, and store the result. Then update
// the source and destination pointers.
#define ROUND() \
	LOAD("a", 0) \
	SHUF("a", "b", "c") \
	TRAN("a", "b", "c") \
	STOR("a", 0) \
	"add $12, %[src] \n\t" \
	"add $16, %[dst] \n\t"

// Define a macro that initiates a three-way interleaved encoding round by
// preloading registers a, b and c from memory.
// The register graph shows which registers are in use during each step, and
// is a visual aid for choosing registers for that step. Symbol index:
//
//  +  indicates that a register is loaded by that step.
//  |  indicates that a register is in use and must not be touched.
//  -  indicates that a register is decommissioned by that step.
//  x  indicates that a register is used as a temporary by that step.
//  V  indicates that a register is an input or output to the macro.
//
#define ROUND_3_INIT() 			/*  a b c d e f  */ \
	LOAD("a",   0)			/*  +            */ \
	SHUF("a", "d", "e")		/*  |     + x    */ \
	LOAD("b",   1)			/*  | +   |      */ \
	TRAN("a", "d", "e")		/*  | |   - x    */ \
	LOAD("c",   2)			/*  V V V        */

// Define a macro that translates, shuffles and stores the input registers A, B
// and C, and preloads registers D, E and F for the next round.
// This macro can be arbitrarily daisy-chained by feeding output registers D, E
// and F back into the next round as input registers A, B and C. The macro
// carefully interleaves memory operations with data operations for optimal
// pipelined performance.

#define ROUND_3(ROUND, A,B,C,D,E,F) 	/*  A B C D E F  */ \
	LOAD(D, (ROUND + 3))		/*  V V V +      */ \
	SHUF(B, E, F)			/*  | | | | + x  */ \
	STOR(A, (ROUND + 0))		/*  - | | | |    */ \
	TRAN(B, E, F)			/*    | | | - x  */ \
	LOAD(E, (ROUND + 4))		/*    | | | +    */ \
	SHUF(C, A, F)			/*  + | | | | x  */ \
	STOR(B, (ROUND + 1))		/*  | - | | |    */ \
	TRAN(C, A, F)			/*  -   | | | x  */ \
	LOAD(F, (ROUND + 5))		/*      | | | +  */ \
	SHUF(D, A, B)			/*  + x | | | |  */ \
	STOR(C, (ROUND + 2))		/*  |   - | | |  */ \
	TRAN(D, A, B)			/*  - x   V V V  */

// Define a macro that terminates a ROUND_3 macro by taking pre-loaded
// registers D, E and F, and translating, shuffling and storing them.
#define ROUND_3_END(ROUND, A,B,C,D,E,F)	/*  A B C D E F  */ \
	SHUF(E, A, B)			/*  + x   V V V  */ \
	STOR(D, (ROUND + 3))		/*  |     - | |  */ \
	TRAN(E, A, B)			/*  - x     | |  */ \
	SHUF(F, C, D)			/*      + x | |  */ \
	STOR(E, (ROUND + 4))		/*      |   - |  */ \
	TRAN(F, C, D)			/*      - x   |  */ \
	STOR(F, (ROUND + 5))		/*            -  */

// Define a type A round. Inputs are a, b, and c, outputs are d, e, and f.
#define ROUND_3_A(ROUND) \
	ROUND_3(ROUND, "a", "b", "c", "d", "e", "f")

// Define a type B round. Inputs and outputs are swapped with regard to type A.
#define ROUND_3_B(ROUND) \
	ROUND_3(ROUND, "d", "e", "f", "a", "b", "c")

// Terminating macro for a type A round.
#define ROUND_3_A_LAST(ROUND) \
	ROUND_3_A(ROUND) \
	ROUND_3_END(ROUND, "a", "b", "c", "d", "e", "f")

// Terminating macro for a type B round.
#define ROUND_3_B_LAST(ROUND) \
	ROUND_3_B(ROUND) \
	ROUND_3_END(ROUND, "d", "e", "f", "a", "b", "c")

// Suppress clang's warning that the literal string in the asm statement is
// overlong (longer than the ISO-mandated minimum size of 4095 bytes for C99
// compilers). It may be true, but the goal here is not C99 portability.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverlength-strings"

static inline void
enc_loop_avx (const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen)
{
	// For a clearer explanation of the algorithm used by this function,
	// please refer to the plain (not inline assembly) implementation. This
	// function follows the same basic logic.

	if (*slen < 16) {
		return;
	}

	// Process blocks of 12 bytes at a time. Input is read in blocks of 16
	// bytes, so "reserve" four bytes from the input buffer to ensure that
	// we never read beyond the end of the input buffer.
	size_t rounds = (*slen - 4) / 12;

	*slen -= rounds * 12;   // 12 bytes consumed per round
	*olen += rounds * 16;   // 16 bytes produced per round

	// Number of times to go through the 36x loop.
	size_t loops = rounds / 36;

	// Number of rounds remaining after the 36x loop.
	rounds %= 36;

	// Lookup tables.
	const __m128i lut0 = _mm_set_epi8(
		10, 11,  9, 10,  7,  8,  6,  7,  4,  5,  3,  4,  1,  2,  0,  1);

	const __m128i lut1 = _mm_setr_epi8(
		65, 71, -4, -4, -4, -4, -4, -4, -4, -4, -4, -4, -19, -16, 0, 0);

	// Temporary registers.
	__m128i a, b, c, d, e, f;

	__asm__ volatile (

		// If there are 36 rounds or more, enter a 36x unrolled loop of
		// interleaved encoding rounds. The rounds interleave memory
		// operations (load/store) with data operations (table lookups,
		// etc) to maximize pipeline throughput.
		"    test %[loops], %[loops] \n\t"
		"    jz   18f                \n\t"
		"    jmp  36f                \n\t"
		"                            \n\t"
		".balign 64                  \n\t"
		"36: " ROUND_3_INIT()
		"    " ROUND_3_A( 0)
		"    " ROUND_3_B( 3)
		"    " ROUND_3_A( 6)
		"    " ROUND_3_B( 9)
		"    " ROUND_3_A(12)
		"    " ROUND_3_B(15)
		"    " ROUND_3_A(18)
		"    " ROUND_3_B(21)
		"    " ROUND_3_A(24)
		"    " ROUND_3_B(27)
		"    " ROUND_3_A_LAST(30)
		"    add $(12 * 36), %[src] \n\t"
		"    add $(16 * 36), %[dst] \n\t"
		"    dec %[loops]           \n\t"
		"    jnz 36b                \n\t"

		// Enter an 18x unrolled loop for rounds of 18 or more.
		"18: cmp $18, %[rounds] \n\t"
		"    jl  9f             \n\t"
		"    " ROUND_3_INIT()
		"    " ROUND_3_A(0)
		"    " ROUND_3_B(3)
		"    " ROUND_3_A(6)
		"    " ROUND_3_B(9)
		"    " ROUND_3_A_LAST(12)
		"    sub $18,        %[rounds] \n\t"
		"    add $(12 * 18), %[src]    \n\t"
		"    add $(16 * 18), %[dst]    \n\t"

		// Enter a 9x unrolled loop for rounds of 9 or more.
		"9:  cmp $9, %[rounds] \n\t"
		"    jl  6f            \n\t"
		"    " ROUND_3_INIT()
		"    " ROUND_3_A(0)
		"    " ROUND_3_B_LAST(3)
		"    sub $9,        %[rounds] \n\t"
		"    add $(12 * 9), %[src]    \n\t"
		"    add $(16 * 9), %[dst]    \n\t"

		// Enter a 6x unrolled loop for rounds of 6 or more.
		"6:  cmp $6, %[rounds] \n\t"
		"    jl  55f           \n\t"
		"    " ROUND_3_INIT()
		"    " ROUND_3_A_LAST(0)
		"    sub $6,        %[rounds] \n\t"
		"    add $(12 * 6), %[src]    \n\t"
		"    add $(16 * 6), %[dst]    \n\t"

		// Dispatch the remaining rounds 0..5.
		"55: cmp $3, %[rounds] \n\t"
		"    jg  45f           \n\t"
		"    je  3f            \n\t"
		"    cmp $1, %[rounds] \n\t"
		"    jg  2f            \n\t"
		"    je  1f            \n\t"
		"    jmp 0f            \n\t"

		"45: cmp $4, %[rounds] \n\t"
		"    je  4f            \n\t"

		// Block of non-interlaced encoding rounds, which can each
		// individually be jumped to. Rounds fall through to the next.
		"5: " ROUND()
		"4: " ROUND()
		"3: " ROUND()
		"2: " ROUND()
		"1: " ROUND()
		"0: \n\t"

		// Outputs (modified).
		: [rounds] "+r"  (rounds),
		  [loops]  "+r"  (loops),
		  [src]    "+r"  (*s),
		  [dst]    "+r"  (*o),
		  [a]      "=&x" (a),
		  [b]      "=&x" (b),
		  [c]      "=&x" (c),
		  [d]      "=&x" (d),
		  [e]      "=&x" (e),
		  [f]      "=&x" (f)

		// Inputs (not modified).
		: [lut0] "x" (lut0),
		  [lut1] "x" (lut1),
		  [msk0] "x" (_mm_set1_epi32(0x0FC0FC00)),
		  [msk1] "x" (_mm_set1_epi32(0x04000040)),
		  [msk2] "x" (_mm_set1_epi32(0x003F03F0)),
		  [msk3] "x" (_mm_set1_epi32(0x01000010)),
		  [n51]  "x" (_mm_set1_epi8(51)),
		  [n25]  "x" (_mm_set1_epi8(25))

		// Clobbers.
		: "cc", "memory"
	);
}

#pragma GCC diagnostic pop
