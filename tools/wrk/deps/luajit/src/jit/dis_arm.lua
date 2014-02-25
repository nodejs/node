----------------------------------------------------------------------------
-- LuaJIT ARM disassembler module.
--
-- Copyright (C) 2005-2013 Mike Pall. All rights reserved.
-- Released under the MIT license. See Copyright Notice in luajit.h
----------------------------------------------------------------------------
-- This is a helper module used by the LuaJIT machine code dumper module.
--
-- It disassembles most user-mode ARMv7 instructions
-- NYI: Advanced SIMD and VFP instructions.
------------------------------------------------------------------------------

local type = type
local sub, byte, format = string.sub, string.byte, string.format
local match, gmatch, gsub = string.match, string.gmatch, string.gsub
local concat = table.concat
local bit = require("bit")
local band, bor, ror, tohex = bit.band, bit.bor, bit.ror, bit.tohex
local lshift, rshift, arshift = bit.lshift, bit.rshift, bit.arshift

------------------------------------------------------------------------------
-- Opcode maps
------------------------------------------------------------------------------

local map_loadc = {
  shift = 8, mask = 15,
  [10] = {
    shift = 20, mask = 1,
    [0] = {
      shift = 23, mask = 3,
      [0] = "vmovFmDN", "vstmFNdr",
      _ = {
	shift = 21, mask = 1,
	[0] = "vstrFdl",
	{ shift = 16, mask = 15, [13] = "vpushFdr", _ = "vstmdbFNdr", }
      },
    },
    {
      shift = 23, mask = 3,
      [0] = "vmovFDNm",
      { shift = 16, mask = 15, [13] = "vpopFdr", _ = "vldmFNdr", },
      _ = {
	shift = 21, mask = 1,
	[0] = "vldrFdl", "vldmdbFNdr",
      },
    },
  },
  [11] = {
    shift = 20, mask = 1,
    [0] = {
      shift = 23, mask = 3,
      [0] = "vmovGmDN", "vstmGNdr",
      _ = {
	shift = 21, mask = 1,
	[0] = "vstrGdl",
	{ shift = 16, mask = 15, [13] = "vpushGdr", _ = "vstmdbGNdr", }
      },
    },
    {
      shift = 23, mask = 3,
      [0] = "vmovGDNm",
      { shift = 16, mask = 15, [13] = "vpopGdr", _ = "vldmGNdr", },
      _ = {
	shift = 21, mask = 1,
	[0] = "vldrGdl", "vldmdbGNdr",
      },
    },
  },
  _ = {
    shift = 0, mask = 0 -- NYI ldc, mcrr, mrrc.
  },
}

local map_vfps = {
  shift = 6, mask = 0x2c001,
  [0] = "vmlaF.dnm", "vmlsF.dnm",
  [0x04000] = "vnmlsF.dnm", [0x04001] = "vnmlaF.dnm",
  [0x08000] = "vmulF.dnm", [0x08001] = "vnmulF.dnm",
  [0x0c000] = "vaddF.dnm", [0x0c001] = "vsubF.dnm",
  [0x20000] = "vdivF.dnm",
  [0x24000] = "vfnmsF.dnm", [0x24001] = "vfnmaF.dnm",
  [0x28000] = "vfmaF.dnm", [0x28001] = "vfmsF.dnm",
  [0x2c000] = "vmovF.dY",
  [0x2c001] = {
    shift = 7, mask = 0x1e01,
    [0] = "vmovF.dm", "vabsF.dm",
    [0x0200] = "vnegF.dm", [0x0201] = "vsqrtF.dm",
    [0x0800] = "vcmpF.dm", [0x0801] = "vcmpeF.dm",
    [0x0a00] = "vcmpzF.d", [0x0a01] = "vcmpzeF.d",
    [0x0e01] = "vcvtG.dF.m",
    [0x1000] = "vcvt.f32.u32Fdm", [0x1001] = "vcvt.f32.s32Fdm",
    [0x1800] = "vcvtr.u32F.dm", [0x1801] = "vcvt.u32F.dm",
    [0x1a00] = "vcvtr.s32F.dm", [0x1a01] = "vcvt.s32F.dm",
  },
}

local map_vfpd = {
  shift = 6, mask = 0x2c001,
  [0] = "vmlaG.dnm", "vmlsG.dnm",
  [0x04000] = "vnmlsG.dnm", [0x04001] = "vnmlaG.dnm",
  [0x08000] = "vmulG.dnm", [0x08001] = "vnmulG.dnm",
  [0x0c000] = "vaddG.dnm", [0x0c001] = "vsubG.dnm",
  [0x20000] = "vdivG.dnm",
  [0x24000] = "vfnmsG.dnm", [0x24001] = "vfnmaG.dnm",
  [0x28000] = "vfmaG.dnm", [0x28001] = "vfmsG.dnm",
  [0x2c000] = "vmovG.dY",
  [0x2c001] = {
    shift = 7, mask = 0x1e01,
    [0] = "vmovG.dm", "vabsG.dm",
    [0x0200] = "vnegG.dm", [0x0201] = "vsqrtG.dm",
    [0x0800] = "vcmpG.dm", [0x0801] = "vcmpeG.dm",
    [0x0a00] = "vcmpzG.d", [0x0a01] = "vcmpzeG.d",
    [0x0e01] = "vcvtF.dG.m",
    [0x1000] = "vcvt.f64.u32GdFm", [0x1001] = "vcvt.f64.s32GdFm",
    [0x1800] = "vcvtr.u32FdG.m", [0x1801] = "vcvt.u32FdG.m",
    [0x1a00] = "vcvtr.s32FdG.m", [0x1a01] = "vcvt.s32FdG.m",
  },
}

local map_datac = {
  shift = 24, mask = 1,
  [0] = {
    shift = 4, mask = 1,
    [0] = {
      shift = 8, mask = 15,
      [10] = map_vfps,
      [11] = map_vfpd,
      -- NYI cdp, mcr, mrc.
    },
    {
      shift = 8, mask = 15,
      [10] = {
	shift = 20, mask = 15,
	[0] = "vmovFnD", "vmovFDn",
	[14] = "vmsrD",
	[15] = { shift = 12, mask = 15, [15] = "vmrs", _ = "vmrsD", },
      },
    },
  },
  "svcT",
}

local map_loadcu = {
  shift = 0, mask = 0, -- NYI unconditional CP load/store.
}

local map_datacu = {
  shift = 0, mask = 0, -- NYI unconditional CP data.
}

local map_simddata = {
  shift = 0, mask = 0, -- NYI SIMD data.
}

local map_simdload = {
  shift = 0, mask = 0, -- NYI SIMD load/store, preload.
}

local map_preload = {
  shift = 0, mask = 0, -- NYI preload.
}

local map_media = {
  shift = 20, mask = 31,
  [0] = false,
  { --01
    shift = 5, mask = 7,
    [0] = "sadd16DNM", "sasxDNM", "ssaxDNM", "ssub16DNM",
    "sadd8DNM", false, false, "ssub8DNM",
  },
  { --02
    shift = 5, mask = 7,
    [0] = "qadd16DNM", "qasxDNM", "qsaxDNM", "qsub16DNM",
    "qadd8DNM", false, false, "qsub8DNM",
  },
  { --03
    shift = 5, mask = 7,
    [0] = "shadd16DNM", "shasxDNM", "shsaxDNM", "shsub16DNM",
    "shadd8DNM", false, false, "shsub8DNM",
  },
  false,
  { --05
    shift = 5, mask = 7,
    [0] = "uadd16DNM", "uasxDNM", "usaxDNM", "usub16DNM",
    "uadd8DNM", false, false, "usub8DNM",
  },
  { --06
    shift = 5, mask = 7,
    [0] = "uqadd16DNM", "uqasxDNM", "uqsaxDNM", "uqsub16DNM",
    "uqadd8DNM", false, false, "uqsub8DNM",
  },
  { --07
    shift = 5, mask = 7,
    [0] = "uhadd16DNM", "uhasxDNM", "uhsaxDNM", "uhsub16DNM",
    "uhadd8DNM", false, false, "uhsub8DNM",
  },
  { --08
    shift = 5, mask = 7,
    [0] = "pkhbtDNMU", false, "pkhtbDNMU",
    { shift = 16, mask = 15, [15] = "sxtb16DMU", _ = "sxtab16DNMU", },
    "pkhbtDNMU", "selDNM", "pkhtbDNMU",
  },
  false,
  { --0a
    shift = 5, mask = 7,
    [0] = "ssatDxMu", "ssat16DxM", "ssatDxMu",
    { shift = 16, mask = 15, [15] = "sxtbDMU", _ = "sxtabDNMU", },
    "ssatDxMu", false, "ssatDxMu",
  },
  { --0b
    shift = 5, mask = 7,
    [0] = "ssatDxMu", "revDM", "ssatDxMu",
    { shift = 16, mask = 15, [15] = "sxthDMU", _ = "sxtahDNMU", },
    "ssatDxMu", "rev16DM", "ssatDxMu",
  },
  { --0c
    shift = 5, mask = 7,
    [3] = { shift = 16, mask = 15, [15] = "uxtb16DMU", _ = "uxtab16DNMU", },
  },
  false,
  { --0e
    shift = 5, mask = 7,
    [0] = "usatDwMu", "usat16DwM", "usatDwMu",
    { shift = 16, mask = 15, [15] = "uxtbDMU", _ = "uxtabDNMU", },
    "usatDwMu", false, "usatDwMu",
  },
  { --0f
    shift = 5, mask = 7,
    [0] = "usatDwMu", "rbitDM", "usatDwMu",
    { shift = 16, mask = 15, [15] = "uxthDMU", _ = "uxtahDNMU", },
    "usatDwMu", "revshDM", "usatDwMu",
  },
  { --10
    shift = 12, mask = 15,
    [15] = {
      shift = 5, mask = 7,
      "smuadNMS", "smuadxNMS", "smusdNMS", "smusdxNMS",
    },
    _ = {
      shift = 5, mask = 7,
      [0] = "smladNMSD", "smladxNMSD", "smlsdNMSD", "smlsdxNMSD",
    },
  },
  false, false, false,
  { --14
    shift = 5, mask = 7,
    [0] = "smlaldDNMS", "smlaldxDNMS", "smlsldDNMS", "smlsldxDNMS",
  },
  { --15
    shift = 5, mask = 7,
    [0] = { shift = 12, mask = 15, [15] = "smmulNMS", _ = "smmlaNMSD", },
    { shift = 12, mask = 15, [15] = "smmulrNMS", _ = "smmlarNMSD", },
    false, false, false, false,
    "smmlsNMSD", "smmlsrNMSD",
  },
  false, false,
  { --18
    shift = 5, mask = 7,
    [0] = { shift = 12, mask = 15, [15] = "usad8NMS", _ = "usada8NMSD", },
  },
  false,
  { --1a
    shift = 5, mask = 3, [2] = "sbfxDMvw",
  },
  { --1b
    shift = 5, mask = 3, [2] = "sbfxDMvw",
  },
  { --1c
    shift = 5, mask = 3,
    [0] = { shift = 0, mask = 15, [15] = "bfcDvX", _ = "bfiDMvX", },
  },
  { --1d
    shift = 5, mask = 3,
    [0] = { shift = 0, mask = 15, [15] = "bfcDvX", _ = "bfiDMvX", },
  },
  { --1e
    shift = 5, mask = 3, [2] = "ubfxDMvw",
  },
  { --1f
    shift = 5, mask = 3, [2] = "ubfxDMvw",
  },
}

local map_load = {
  shift = 21, mask = 9,
  {
    shift = 20, mask = 5,
    [0] = "strtDL", "ldrtDL", [4] = "strbtDL", [5] = "ldrbtDL",
  },
  _ = {
    shift = 20, mask = 5,
    [0] = "strDL", "ldrDL", [4] = "strbDL", [5] = "ldrbDL",
  }
}

local map_load1 = {
  shift = 4, mask = 1,
  [0] = map_load, map_media,
}

local map_loadm = {
  shift = 20, mask = 1,
  [0] = {
    shift = 23, mask = 3,
    [0] = "stmdaNR", "stmNR",
    { shift = 16, mask = 63, [45] = "pushR", _ = "stmdbNR", }, "stmibNR",
  },
  {
    shift = 23, mask = 3,
    [0] = "ldmdaNR", { shift = 16, mask = 63, [61] = "popR", _ = "ldmNR", },
    "ldmdbNR", "ldmibNR",
  },
}

local map_data = {
  shift = 21, mask = 15,
  [0] = "andDNPs", "eorDNPs", "subDNPs", "rsbDNPs",
  "addDNPs", "adcDNPs", "sbcDNPs", "rscDNPs",
  "tstNP", "teqNP", "cmpNP", "cmnNP",
  "orrDNPs", "movDPs", "bicDNPs", "mvnDPs",
}

local map_mul = {
  shift = 21, mask = 7,
  [0] = "mulNMSs", "mlaNMSDs", "umaalDNMS", "mlsDNMS",
  "umullDNMSs", "umlalDNMSs", "smullDNMSs", "smlalDNMSs",
}

local map_sync = {
  shift = 20, mask = 15, -- NYI: brackets around N. R(D+1) for ldrexd/strexd.
  [0] = "swpDMN", false, false, false,
  "swpbDMN", false, false, false,
  "strexDMN", "ldrexDN", "strexdDN", "ldrexdDN",
  "strexbDMN", "ldrexbDN", "strexhDN", "ldrexhDN",
}

local map_mulh = {
  shift = 21, mask = 3,
  [0] = { shift = 5, mask = 3,
    [0] = "smlabbNMSD", "smlatbNMSD", "smlabtNMSD", "smlattNMSD", },
  { shift = 5, mask = 3,
    [0] = "smlawbNMSD", "smulwbNMS", "smlawtNMSD", "smulwtNMS", },
  { shift = 5, mask = 3,
    [0] = "smlalbbDNMS", "smlaltbDNMS", "smlalbtDNMS", "smlalttDNMS", },
  { shift = 5, mask = 3,
    [0] = "smulbbNMS", "smultbNMS", "smulbtNMS", "smulttNMS", },
}

local map_misc = {
  shift = 4, mask = 7,
  -- NYI: decode PSR bits of msr.
  [0] = { shift = 21, mask = 1, [0] = "mrsD", "msrM", },
  { shift = 21, mask = 3, "bxM", false, "clzDM", },
  { shift = 21, mask = 3, "bxjM", },
  { shift = 21, mask = 3, "blxM", },
  false,
  { shift = 21, mask = 3, [0] = "qaddDMN", "qsubDMN", "qdaddDMN", "qdsubDMN", },
  false,
  { shift = 21, mask = 3, "bkptK", },
}

local map_datar = {
  shift = 4, mask = 9,
  [9] = {
    shift = 5, mask = 3,
    [0] = { shift = 24, mask = 1, [0] = map_mul, map_sync, },
    { shift = 20, mask = 1, [0] = "strhDL", "ldrhDL", },
    { shift = 20, mask = 1, [0] = "ldrdDL", "ldrsbDL", },
    { shift = 20, mask = 1, [0] = "strdDL", "ldrshDL", },
  },
  _ = {
    shift = 20, mask = 25,
    [16] = { shift = 7, mask = 1, [0] = map_misc, map_mulh, },
    _ = {
      shift = 0, mask = 0xffffffff,
      [bor(0xe1a00000)] = "nop",
      _ = map_data,
    }
  },
}

local map_datai = {
  shift = 20, mask = 31, -- NYI: decode PSR bits of msr. Decode imm12.
  [16] = "movwDW", [20] = "movtDW",
  [18] = { shift = 0, mask = 0xf00ff, [0] = "nopv6", _ = "msrNW", },
  [22] = "msrNW",
  _ = map_data,
}

local map_branch = {
  shift = 24, mask = 1,
  [0] = "bB", "blB"
}

local map_condins = {
  [0] = map_datar, map_datai, map_load, map_load1,
  map_loadm, map_branch, map_loadc, map_datac
}

-- NYI: setend.
local map_uncondins = {
  [0] = false, map_simddata, map_simdload, map_preload,
  false, "blxB", map_loadcu, map_datacu,
}

------------------------------------------------------------------------------

local map_gpr = {
  [0] = "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc",
}

local map_cond = {
  [0] = "eq", "ne", "hs", "lo", "mi", "pl", "vs", "vc",
  "hi", "ls", "ge", "lt", "gt", "le", "al",
}

local map_shift = { [0] = "lsl", "lsr", "asr", "ror", }

------------------------------------------------------------------------------

-- Output a nicely formatted line with an opcode and operands.
local function putop(ctx, text, operands)
  local pos = ctx.pos
  local extra = ""
  if ctx.rel then
    local sym = ctx.symtab[ctx.rel]
    if sym then
      extra = "\t->"..sym
    elseif band(ctx.op, 0x0e000000) ~= 0x0a000000 then
      extra = "\t; 0x"..tohex(ctx.rel)
    end
  end
  if ctx.hexdump > 0 then
    ctx.out(format("%08x  %s  %-5s %s%s\n",
	    ctx.addr+pos, tohex(ctx.op), text, concat(operands, ", "), extra))
  else
    ctx.out(format("%08x  %-5s %s%s\n",
	    ctx.addr+pos, text, concat(operands, ", "), extra))
  end
  ctx.pos = pos + 4
end

-- Fallback for unknown opcodes.
local function unknown(ctx)
  return putop(ctx, ".long", { "0x"..tohex(ctx.op) })
end

-- Format operand 2 of load/store opcodes.
local function fmtload(ctx, op, pos)
  local base = map_gpr[band(rshift(op, 16), 15)]
  local x, ofs
  local ext = (band(op, 0x04000000) == 0)
  if not ext and band(op, 0x02000000) == 0 then
    ofs = band(op, 4095)
    if band(op, 0x00800000) == 0 then ofs = -ofs end
    if base == "pc" then ctx.rel = ctx.addr + pos + 8 + ofs end
    ofs = "#"..ofs
  elseif ext and band(op, 0x00400000) ~= 0 then
    ofs = band(op, 15) + band(rshift(op, 4), 0xf0)
    if band(op, 0x00800000) == 0 then ofs = -ofs end
    if base == "pc" then ctx.rel = ctx.addr + pos + 8 + ofs end
    ofs = "#"..ofs
  else
    ofs = map_gpr[band(op, 15)]
    if ext or band(op, 0xfe0) == 0 then
    elseif band(op, 0xfe0) == 0x60 then
      ofs = format("%s, rrx", ofs)
    else
      local sh = band(rshift(op, 7), 31)
      if sh == 0 then sh = 32 end
      ofs = format("%s, %s #%d", ofs, map_shift[band(rshift(op, 5), 3)], sh)
    end
    if band(op, 0x00800000) == 0 then ofs = "-"..ofs end
  end
  if ofs == "#0" then
    x = format("[%s]", base)
  elseif band(op, 0x01000000) == 0 then
    x = format("[%s], %s", base, ofs)
  else
    x = format("[%s, %s]", base, ofs)
  end
  if band(op, 0x01200000) == 0x01200000 then x = x.."!" end
  return x
end

-- Format operand 2 of vector load/store opcodes.
local function fmtvload(ctx, op, pos)
  local base = map_gpr[band(rshift(op, 16), 15)]
  local ofs = band(op, 255)*4
  if band(op, 0x00800000) == 0 then ofs = -ofs end
  if base == "pc" then ctx.rel = ctx.addr + pos + 8 + ofs end
  if ofs == 0 then
    return format("[%s]", base)
  else
    return format("[%s, #%d]", base, ofs)
  end
end

local function fmtvr(op, vr, sh0, sh1)
  if vr == "s" then
    return format("s%d", 2*band(rshift(op, sh0), 15)+band(rshift(op, sh1), 1))
  else
    return format("d%d", band(rshift(op, sh0), 15)+band(rshift(op, sh1-4), 16))
  end
end

-- Disassemble a single instruction.
local function disass_ins(ctx)
  local pos = ctx.pos
  local b0, b1, b2, b3 = byte(ctx.code, pos+1, pos+4)
  local op = bor(lshift(b3, 24), lshift(b2, 16), lshift(b1, 8), b0)
  local operands = {}
  local suffix = ""
  local last, name, pat
  local vr
  ctx.op = op
  ctx.rel = nil

  local cond = rshift(op, 28)
  local opat
  if cond == 15 then
    opat = map_uncondins[band(rshift(op, 25), 7)]
  else
    if cond ~= 14 then suffix = map_cond[cond] end
    opat = map_condins[band(rshift(op, 25), 7)]
  end
  while type(opat) ~= "string" do
    if not opat then return unknown(ctx) end
    opat = opat[band(rshift(op, opat.shift), opat.mask)] or opat._
  end
  name, pat = match(opat, "^([a-z0-9]*)(.*)")
  if sub(pat, 1, 1) == "." then
    local s2, p2 = match(pat, "^([a-z0-9.]*)(.*)")
    suffix = suffix..s2
    pat = p2
  end

  for p in gmatch(pat, ".") do
    local x = nil
    if p == "D" then
      x = map_gpr[band(rshift(op, 12), 15)]
    elseif p == "N" then
      x = map_gpr[band(rshift(op, 16), 15)]
    elseif p == "S" then
      x = map_gpr[band(rshift(op, 8), 15)]
    elseif p == "M" then
      x = map_gpr[band(op, 15)]
    elseif p == "d" then
      x = fmtvr(op, vr, 12, 22)
    elseif p == "n" then
      x = fmtvr(op, vr, 16, 7)
    elseif p == "m" then
      x = fmtvr(op, vr, 0, 5)
    elseif p == "P" then
      if band(op, 0x02000000) ~= 0 then
	x = ror(band(op, 255), 2*band(rshift(op, 8), 15))
      else
	x = map_gpr[band(op, 15)]
	if band(op, 0xff0) ~= 0 then
	  operands[#operands+1] = x
	  local s = map_shift[band(rshift(op, 5), 3)]
	  local r = nil
	  if band(op, 0xf90) == 0 then
	    if s == "ror" then s = "rrx" else r = "#32" end
	  elseif band(op, 0x10) == 0 then
	    r = "#"..band(rshift(op, 7), 31)
	  else
	    r = map_gpr[band(rshift(op, 8), 15)]
	  end
	  if name == "mov" then name = s; x = r
	  elseif r then x = format("%s %s", s, r)
	  else x = s end
	end
      end
    elseif p == "L" then
      x = fmtload(ctx, op, pos)
    elseif p == "l" then
      x = fmtvload(ctx, op, pos)
    elseif p == "B" then
      local addr = ctx.addr + pos + 8 + arshift(lshift(op, 8), 6)
      if cond == 15 then addr = addr + band(rshift(op, 23), 2) end
      ctx.rel = addr
      x = "0x"..tohex(addr)
    elseif p == "F" then
      vr = "s"
    elseif p == "G" then
      vr = "d"
    elseif p == "." then
      suffix = suffix..(vr == "s" and ".f32" or ".f64")
    elseif p == "R" then
      if band(op, 0x00200000) ~= 0 and #operands == 1 then
	operands[1] = operands[1].."!"
      end
      local t = {}
      for i=0,15 do
	if band(rshift(op, i), 1) == 1 then t[#t+1] = map_gpr[i] end
      end
      x = "{"..concat(t, ", ").."}"
    elseif p == "r" then
      if band(op, 0x00200000) ~= 0 and #operands == 2 then
	operands[1] = operands[1].."!"
      end
      local s = tonumber(sub(last, 2))
      local n = band(op, 255)
      if vr == "d" then n = rshift(n, 1) end
      operands[#operands] = format("{%s-%s%d}", last, vr, s+n-1)
    elseif p == "W" then
      x = band(op, 0x0fff) + band(rshift(op, 4), 0xf000)
    elseif p == "T" then
      x = "#0x"..tohex(band(op, 0x00ffffff), 6)
    elseif p == "U" then
      x = band(rshift(op, 7), 31)
      if x == 0 then x = nil end
    elseif p == "u" then
      x = band(rshift(op, 7), 31)
      if band(op, 0x40) == 0 then
	if x == 0 then x = nil else x = "lsl #"..x end
      else
	if x == 0 then x = "asr #32" else x = "asr #"..x end
      end
    elseif p == "v" then
      x = band(rshift(op, 7), 31)
    elseif p == "w" then
      x = band(rshift(op, 16), 31)
    elseif p == "x" then
      x = band(rshift(op, 16), 31) + 1
    elseif p == "X" then
      x = band(rshift(op, 16), 31) - last + 1
    elseif p == "Y" then
      x = band(rshift(op, 12), 0xf0) + band(op, 0x0f)
    elseif p == "K" then
      x = "#0x"..tohex(band(rshift(op, 4), 0x0000fff0) + band(op, 15), 4)
    elseif p == "s" then
      if band(op, 0x00100000) ~= 0 then suffix = "s"..suffix end
    else
      assert(false)
    end
    if x then
      last = x
      if type(x) == "number" then x = "#"..x end
      operands[#operands+1] = x
    end
  end

  return putop(ctx, name..suffix, operands)
end

------------------------------------------------------------------------------

-- Disassemble a block of code.
local function disass_block(ctx, ofs, len)
  if not ofs then ofs = 0 end
  local stop = len and ofs+len or #ctx.code
  ctx.pos = ofs
  ctx.rel = nil
  while ctx.pos < stop do disass_ins(ctx) end
end

-- Extended API: create a disassembler context. Then call ctx:disass(ofs, len).
local function create_(code, addr, out)
  local ctx = {}
  ctx.code = code
  ctx.addr = addr or 0
  ctx.out = out or io.write
  ctx.symtab = {}
  ctx.disass = disass_block
  ctx.hexdump = 8
  return ctx
end

-- Simple API: disassemble code (a string) at address and output via out.
local function disass_(code, addr, out)
  create_(code, addr, out):disass()
end

-- Return register name for RID.
local function regname_(r)
  if r < 16 then return map_gpr[r] end
  return "d"..(r-16)
end

-- Public module functions.
module(...)

create = create_
disass = disass_
regname = regname_

