/*
** Trace compiler error messages.
** Copyright (C) 2005-2013 Mike Pall. See Copyright Notice in luajit.h
*/

/* This file may be included multiple times with different TREDEF macros. */

/* Recording. */
TREDEF(RECERR,	"error thrown or hook called during recording")
TREDEF(TRACEOV,	"trace too long")
TREDEF(STACKOV,	"trace too deep")
TREDEF(SNAPOV,	"too many snapshots")
TREDEF(BLACKL,	"blacklisted")
TREDEF(NYIBC,	"NYI: bytecode %d")

/* Recording loop ops. */
TREDEF(LLEAVE,	"leaving loop in root trace")
TREDEF(LINNER,	"inner loop in root trace")
TREDEF(LUNROLL,	"loop unroll limit reached")

/* Recording calls/returns. */
TREDEF(BADTYPE,	"bad argument type")
TREDEF(CJITOFF,	"call to JIT-disabled function")
TREDEF(CUNROLL,	"call unroll limit reached")
TREDEF(DOWNREC,	"down-recursion, restarting")
TREDEF(NYICF,	"NYI: C function %p")
TREDEF(NYIFF,	"NYI: FastFunc %s")
TREDEF(NYIFFU,	"NYI: unsupported variant of FastFunc %s")
TREDEF(NYIRETL,	"NYI: return to lower frame")

/* Recording indexed load/store. */
TREDEF(STORENN,	"store with nil or NaN key")
TREDEF(NOMM,	"missing metamethod")
TREDEF(IDXLOOP,	"looping index lookup")
TREDEF(NYITMIX,	"NYI: mixed sparse/dense table")

/* Recording C data operations. */
TREDEF(NOCACHE,	"symbol not in cache")
TREDEF(NYICONV,	"NYI: unsupported C type conversion")
TREDEF(NYICALL,	"NYI: unsupported C function type")

/* Optimizations. */
TREDEF(GFAIL,	"guard would always fail")
TREDEF(PHIOV,	"too many PHIs")
TREDEF(TYPEINS,	"persistent type instability")

/* Assembler. */
TREDEF(MCODEAL,	"failed to allocate mcode memory")
TREDEF(MCODEOV,	"machine code too long")
TREDEF(MCODELM,	"hit mcode limit (retrying)")
TREDEF(SPILLOV,	"too many spill slots")
TREDEF(BADRA,	"inconsistent register allocation")
TREDEF(NYIIR,	"NYI: cannot assemble IR instruction %d")
TREDEF(NYIPHI,	"NYI: PHI shuffling too complex")
TREDEF(NYICOAL,	"NYI: register coalescing too complex")

#undef TREDEF

/* Detecting unused error messages:
   awk -F, '/^TREDEF/ { gsub(/TREDEF./, ""); printf "grep -q LJ_TRERR_%s *.[ch] || echo %s\n", $1, $1}' lj_traceerr.h | sh
*/
