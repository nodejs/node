/*
** Definitions for target CPU.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

#ifndef _LJ_TARGET_H
#define _LJ_TARGET_H

#include "lj_def.h"
#include "lj_arch.h"

/* -- Registers and spill slots ------------------------------------------- */

/* Register type (uint8_t in ir->r). */
typedef uint32_t Reg;

/* The hi-bit is NOT set for an allocated register. This means the value
** can be directly used without masking. The hi-bit is set for a register
** allocation hint or for RID_INIT, RID_SINK or RID_SUNK.
*/
#define RID_NONE		0x80
#define RID_MASK		0x7f
#define RID_INIT		(RID_NONE|RID_MASK)
#define RID_SINK		(RID_INIT-1)
#define RID_SUNK		(RID_INIT-2)

#define ra_noreg(r)		((r) & RID_NONE)
#define ra_hasreg(r)		(!((r) & RID_NONE))

/* The ra_hashint() macro assumes a previous test for ra_noreg(). */
#define ra_hashint(r)		((r) < RID_SUNK)
#define ra_gethint(r)		((Reg)((r) & RID_MASK))
#define ra_sethint(rr, r)	rr = (uint8_t)((r)|RID_NONE)
#define ra_samehint(r1, r2)	(ra_gethint((r1)^(r2)) == 0)

/* Spill slot 0 means no spill slot has been allocated. */
#define SPS_NONE		0

#define ra_hasspill(s)		((s) != SPS_NONE)

/* Combined register and spill slot (uint16_t in ir->prev). */
typedef uint32_t RegSP;

#define REGSP(r, s)		((r) + ((s) << 8))
#define REGSP_HINT(r)		((r)|RID_NONE)
#define REGSP_INIT		REGSP(RID_INIT, 0)

#define regsp_reg(rs)		((rs) & 255)
#define regsp_spill(rs)		((rs) >> 8)
#define regsp_used(rs) \
  (((rs) & ~REGSP(RID_MASK, 0)) != REGSP(RID_NONE, 0))

/* -- Register sets ------------------------------------------------------- */

/* Bitset for registers. 32 registers suffice for most architectures.
** Note that one set holds bits for both GPRs and FPRs.
*/
#if LJ_TARGET_PPC || LJ_TARGET_MIPS
typedef uint64_t RegSet;
#else
typedef uint32_t RegSet;
#endif

#define RID2RSET(r)		(((RegSet)1) << (r))
#define RSET_EMPTY		((RegSet)0)
#define RSET_RANGE(lo, hi)	((RID2RSET((hi)-(lo))-1) << (lo))

#define rset_test(rs, r)	((int)((rs) >> (r)) & 1)
#define rset_set(rs, r)		(rs |= RID2RSET(r))
#define rset_clear(rs, r)	(rs &= ~RID2RSET(r))
#define rset_exclude(rs, r)	(rs & ~RID2RSET(r))
#if LJ_TARGET_PPC || LJ_TARGET_MIPS
#define rset_picktop(rs)	((Reg)(__builtin_clzll(rs)^63))
#define rset_pickbot(rs)	((Reg)__builtin_ctzll(rs))
#else
#define rset_picktop(rs)	((Reg)lj_fls(rs))
#define rset_pickbot(rs)	((Reg)lj_ffs(rs))
#endif

/* -- Register allocation cost -------------------------------------------- */

/* The register allocation heuristic keeps track of the cost for allocating
** a specific register:
**
** A free register (obviously) has a cost of 0 and a 1-bit in the free mask.
**
** An already allocated register has the (non-zero) IR reference in the lowest
** bits and the result of a blended cost-model in the higher bits.
**
** The allocator first checks the free mask for a hit. Otherwise an (unrolled)
** linear search for the minimum cost is used. The search doesn't need to
** keep track of the position of the minimum, which makes it very fast.
** The lowest bits of the minimum cost show the desired IR reference whose
** register is the one to evict.
**
** Without the cost-model this degenerates to the standard heuristics for
** (reverse) linear-scan register allocation. Since code generation is done
** in reverse, a live interval extends from the last use to the first def.
** For an SSA IR the IR reference is the first (and only) def and thus
** trivially marks the end of the interval. The LSRA heuristics says to pick
** the register whose live interval has the furthest extent, i.e. the lowest
** IR reference in our case.
**
** A cost-model should take into account other factors, like spill-cost and
** restore- or rematerialization-cost, which depend on the kind of instruction.
** E.g. constants have zero spill costs, variant instructions have higher
** costs than invariants and PHIs should preferably never be spilled.
**
** Here's a first cut at simple, but effective blended cost-model for R-LSRA:
** - Due to careful design of the IR, constants already have lower IR
**   references than invariants and invariants have lower IR references
**   than variants.
** - The cost in the upper 16 bits is the sum of the IR reference and a
**   weighted score. The score currently only takes into account whether
**   the IRT_ISPHI bit is set in the instruction type.
** - The PHI weight is the minimum distance (in IR instructions) a PHI
**   reference has to be further apart from a non-PHI reference to be spilled.
** - It should be a power of two (for speed) and must be between 2 and 32768.
**   Good values for the PHI weight seem to be between 40 and 150.
** - Further study is required.
*/
#define REGCOST_PHI_WEIGHT	64

/* Cost for allocating a specific register. */
typedef uint32_t RegCost;

/* Note: assumes 16 bit IRRef1. */
#define REGCOST(cost, ref)	((RegCost)(ref) + ((RegCost)(cost) << 16))
#define regcost_ref(rc)		((IRRef1)(rc))

#define REGCOST_T(t) \
  ((RegCost)((t)&IRT_ISPHI) * (((RegCost)(REGCOST_PHI_WEIGHT)<<16)/IRT_ISPHI))
#define REGCOST_REF_T(ref, t)	(REGCOST((ref), (ref)) + REGCOST_T((t)))

/* -- Target-specific definitions ----------------------------------------- */

#if LJ_TARGET_X86ORX64
#include "lj_target_x86.h"
#elif LJ_TARGET_ARM
#include "lj_target_arm.h"
#elif LJ_TARGET_PPC
#include "lj_target_ppc.h"
#elif LJ_TARGET_MIPS
#include "lj_target_mips.h"
#else
#error "Missing include for target CPU"
#endif

#ifdef EXITSTUBS_PER_GROUP
/* Return the address of an exit stub. */
static LJ_AINLINE char *exitstub_addr_(char **group, uint32_t exitno)
{
  lua_assert(group[exitno / EXITSTUBS_PER_GROUP] != NULL);
  return (char *)group[exitno / EXITSTUBS_PER_GROUP] +
	 EXITSTUB_SPACING*(exitno % EXITSTUBS_PER_GROUP);
}
/* Avoid dependence on lj_jit.h if only including lj_target.h. */
#define exitstub_addr(J, exitno) \
  ((MCode *)exitstub_addr_((char **)((J)->exitstubgroup), (exitno)))
#endif

#endif
