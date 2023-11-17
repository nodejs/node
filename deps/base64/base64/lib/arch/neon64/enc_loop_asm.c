// Apologies in advance for combining the preprocessor with inline assembly,
// two notoriously gnarly parts of C, but it was necessary to avoid a lot of
// code repetition. The preprocessor is used to template large sections of
// inline assembly that differ only in the registers used. If the code was
// written out by hand, it would become very large and hard to audit.

// Generate a block of inline assembly that loads three user-defined registers
// A, B, C from memory and deinterleaves them, post-incrementing the src
// pointer. The register set should be sequential.
#define LOAD(A, B, C) \
	"ld3 {"A".16b, "B".16b, "C".16b}, [%[src]], #48 \n\t"

// Generate a block of inline assembly that takes three deinterleaved registers
// and shuffles the bytes. The output is in temporary registers t0..t3.
#define SHUF(A, B, C) \
	"ushr %[t0].16b, "A".16b,   #2         \n\t" \
	"ushr %[t1].16b, "B".16b,   #4         \n\t" \
	"ushr %[t2].16b, "C".16b,   #6         \n\t" \
	"sli  %[t1].16b, "A".16b,   #4         \n\t" \
	"sli  %[t2].16b, "B".16b,   #2         \n\t" \
	"and  %[t1].16b, %[t1].16b, %[n63].16b \n\t" \
	"and  %[t2].16b, %[t2].16b, %[n63].16b \n\t" \
	"and  %[t3].16b, "C".16b,   %[n63].16b \n\t"

// Generate a block of inline assembly that takes temporary registers t0..t3
// and translates them to the base64 alphabet, using a table loaded into
// v8..v11. The output is in user-defined registers A..D.
#define TRAN(A, B, C, D) \
	"tbl "A".16b, {v8.16b-v11.16b}, %[t0].16b \n\t" \
	"tbl "B".16b, {v8.16b-v11.16b}, %[t1].16b \n\t" \
	"tbl "C".16b, {v8.16b-v11.16b}, %[t2].16b \n\t" \
	"tbl "D".16b, {v8.16b-v11.16b}, %[t3].16b \n\t"

// Generate a block of inline assembly that interleaves four registers and
// stores them, post-incrementing the destination pointer.
#define STOR(A, B, C, D) \
	"st4 {"A".16b, "B".16b, "C".16b, "D".16b}, [%[dst]], #64 \n\t"

// Generate a block of inline assembly that generates a single self-contained
// encoder round: fetch the data, process it, and store the result.
#define ROUND() \
	LOAD("v12", "v13", "v14") \
	SHUF("v12", "v13", "v14") \
	TRAN("v12", "v13", "v14", "v15") \
	STOR("v12", "v13", "v14", "v15")

// Generate a block of assembly that generates a type A interleaved encoder
// round. It uses registers that were loaded by the previous type B round, and
// in turn loads registers for the next type B round.
#define ROUND_A() \
	SHUF("v2",  "v3",  "v4") \
	LOAD("v12", "v13", "v14") \
	TRAN("v2",  "v3",  "v4", "v5") \
	STOR("v2",  "v3",  "v4", "v5")

// Type B interleaved encoder round. Same as type A, but register sets swapped.
#define ROUND_B() \
	SHUF("v12", "v13", "v14") \
	LOAD("v2",  "v3",  "v4") \
	TRAN("v12", "v13", "v14", "v15") \
	STOR("v12", "v13", "v14", "v15")

// The first type A round needs to load its own registers.
#define ROUND_A_FIRST() \
	LOAD("v2", "v3", "v4") \
	ROUND_A()

// The last type B round omits the load for the next step.
#define ROUND_B_LAST() \
	SHUF("v12", "v13", "v14") \
	TRAN("v12", "v13", "v14", "v15") \
	STOR("v12", "v13", "v14", "v15")

// Suppress clang's warning that the literal string in the asm statement is
// overlong (longer than the ISO-mandated minimum size of 4095 bytes for C99
// compilers). It may be true, but the goal here is not C99 portability.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverlength-strings"

static inline void
enc_loop_neon64 (const uint8_t **s, size_t *slen, uint8_t **o, size_t *olen)
{
	size_t rounds = *slen / 48;

	if (rounds == 0) {
		return;
	}

	*slen -= rounds * 48;	// 48 bytes consumed per round.
	*olen += rounds * 64;	// 64 bytes produced per round.

	// Number of times to go through the 8x loop.
	size_t loops = rounds / 8;

	// Number of rounds remaining after the 8x loop.
	rounds %= 8;

	// Temporary registers, used as scratch space.
	uint8x16_t tmp0, tmp1, tmp2, tmp3;

	__asm__ volatile (

		// Load the encoding table into v8..v11.
		"    ld1 {v8.16b-v11.16b}, [%[tbl]] \n\t"

		// If there are eight rounds or more, enter an 8x unrolled loop
		// of interleaved encoding rounds. The rounds interleave memory
		// operations (load/store) with data operations to maximize
		// pipeline throughput.
		"    cbz %[loops], 4f \n\t"

		// The SIMD instructions do not touch the flags.
		"88: subs %[loops], %[loops], #1 \n\t"
		"    " ROUND_A_FIRST()
		"    " ROUND_B()
		"    " ROUND_A()
		"    " ROUND_B()
		"    " ROUND_A()
		"    " ROUND_B()
		"    " ROUND_A()
		"    " ROUND_B_LAST()
		"    b.ne 88b \n\t"

		// Enter a 4x unrolled loop for rounds of 4 or more.
		"4:  cmp  %[rounds], #4 \n\t"
		"    b.lt 30f           \n\t"
		"    " ROUND_A_FIRST()
		"    " ROUND_B()
		"    " ROUND_A()
		"    " ROUND_B_LAST()
		"    sub %[rounds], %[rounds], #4 \n\t"

		// Dispatch the remaining rounds 0..3.
		"30: cbz  %[rounds], 0f \n\t"
		"    cmp  %[rounds], #2 \n\t"
		"    b.eq 2f            \n\t"
		"    b.lt 1f            \n\t"

		// Block of non-interlaced encoding rounds, which can each
		// individually be jumped to. Rounds fall through to the next.
		"3:  " ROUND()
		"2:  " ROUND()
		"1:  " ROUND()
		"0:  \n\t"

		// Outputs (modified).
		: [loops] "+r"  (loops),
		  [src]   "+r"  (*s),
		  [dst]   "+r"  (*o),
		  [t0]    "=&w" (tmp0),
		  [t1]    "=&w" (tmp1),
		  [t2]    "=&w" (tmp2),
		  [t3]    "=&w" (tmp3)

		// Inputs (not modified).
		: [rounds] "r" (rounds),
		  [tbl]    "r" (base64_table_enc_6bit),
		  [n63]    "w" (vdupq_n_u8(63))

		// Clobbers.
		: "v2",  "v3",  "v4",  "v5",
		  "v8",  "v9",  "v10", "v11",
		  "v12", "v13", "v14", "v15",
		  "cc", "memory"
	);
}

#pragma GCC diagnostic pop
