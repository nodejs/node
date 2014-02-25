----------------------------------------------------------------------------
-- LuaJIT MIPS disassembler module.
--
-- Copyright (C) 2005-2013 Mike Pall. All rights reserved.
-- Released under the MIT/X license. See Copyright Notice in luajit.h
----------------------------------------------------------------------------
-- This is a helper module used by the LuaJIT machine code dumper module.
--
-- It disassembles all standard MIPS32R1/R2 instructions.
-- Default mode is big-endian, but see: dis_mipsel.lua
------------------------------------------------------------------------------

local type = type
local sub, byte, format = string.sub, string.byte, string.format
local match, gmatch, gsub = string.match, string.gmatch, string.gsub
local concat = table.concat
local bit = require("bit")
local band, bor, tohex = bit.band, bit.bor, bit.tohex
local lshift, rshift, arshift = bit.lshift, bit.rshift, bit.arshift

------------------------------------------------------------------------------
-- Primary and extended opcode maps
------------------------------------------------------------------------------

local map_movci = { shift = 16, mask = 1, [0] = "movfDSC", "movtDSC", }
local map_srl = { shift = 21, mask = 1, [0] = "srlDTA", "rotrDTA", }
local map_srlv = { shift = 6, mask = 1, [0] = "srlvDTS", "rotrvDTS", }

local map_special = {
  shift = 0, mask = 63,
  [0] = { shift = 0, mask = -1, [0] = "nop", _ = "sllDTA" },
  map_movci,	map_srl,	"sraDTA",
  "sllvDTS",	false,		map_srlv,	"sravDTS",
  "jrS",	"jalrD1S",	"movzDST",	"movnDST",
  "syscallY",	"breakY",	false,		"sync",
  "mfhiD",	"mthiS",	"mfloD",	"mtloS",
  false,	false,		false,		false,
  "multST",	"multuST",	"divST",	"divuST",
  false,	false,		false,		false,
  "addDST",	"addu|moveDST0", "subDST",	"subu|neguDS0T",
  "andDST",	"orDST",	"xorDST",	"nor|notDST0",
  false,	false,		"sltDST",	"sltuDST",
  false,	false,		false,		false,
  "tgeSTZ",	"tgeuSTZ",	"tltSTZ",	"tltuSTZ",
  "teqSTZ",	false,		"tneSTZ",
}

local map_special2 = {
  shift = 0, mask = 63,
  [0] = "maddST", "madduST",	"mulDST",	false,
  "msubST",	"msubuST",
  [32] = "clzDS", [33] = "cloDS",
  [63] = "sdbbpY",
}

local map_bshfl = {
  shift = 6, mask = 31,
  [2] = "wsbhDT",
  [16] = "sebDT",
  [24] = "sehDT",
}

local map_special3 = {
  shift = 0, mask = 63,
  [0] = "extTSAK", [4] = "insTSAL",
  [32] = map_bshfl,
  [59] = "rdhwrTD",
}

local map_regimm = {
  shift = 16, mask = 31,
  [0] = "bltzSB",	"bgezSB",	"bltzlSB",	"bgezlSB",
  false,	false,		false,		false,
  "tgeiSI",	"tgeiuSI",	"tltiSI",	"tltiuSI",
  "teqiSI",	false,		"tneiSI",	false,
  "bltzalSB",	"bgezalSB",	"bltzallSB",	"bgezallSB",
  false,	false,		false,		false,
  false,	false,		false,		false,
  false,	false,		false,		"synciSO",
}

local map_cop0 = {
  shift = 25, mask = 1,
  [0] = {
    shift = 21, mask = 15,
    [0] = "mfc0TDW", [4] = "mtc0TDW",
    [10] = "rdpgprDT",
    [11] = { shift = 5, mask = 1, [0] = "diT0", "eiT0", },
    [14] = "wrpgprDT",
  }, {
    shift = 0, mask = 63,
    [1] = "tlbr", [2] = "tlbwi", [6] = "tlbwr", [8] = "tlbp",
    [24] = "eret", [31] = "deret",
    [32] = "wait",
  },
}

local map_cop1s = {
  shift = 0, mask = 63,
  [0] = "add.sFGH",	"sub.sFGH",	"mul.sFGH",	"div.sFGH",
  "sqrt.sFG",		"abs.sFG",	"mov.sFG",	"neg.sFG",
  "round.l.sFG",	"trunc.l.sFG",	"ceil.l.sFG",	"floor.l.sFG",
  "round.w.sFG",	"trunc.w.sFG",	"ceil.w.sFG",	"floor.w.sFG",
  false,
  { shift = 16, mask = 1, [0] = "movf.sFGC", "movt.sFGC" },
  "movz.sFGT",	"movn.sFGT",
  false,	"recip.sFG",	"rsqrt.sFG",	false,
  false,	false,		false,		false,
  false,	false,		false,		false,
  false,	"cvt.d.sFG",	false,		false,
  "cvt.w.sFG",	"cvt.l.sFG",	"cvt.ps.sFGH",	false,
  false,	false,		false,		false,
  false,	false,		false,		false,
  "c.f.sVGH",	"c.un.sVGH",	"c.eq.sVGH",	"c.ueq.sVGH",
  "c.olt.sVGH",	"c.ult.sVGH",	"c.ole.sVGH",	"c.ule.sVGH",
  "c.sf.sVGH",	"c.ngle.sVGH",	"c.seq.sVGH",	"c.ngl.sVGH",
  "c.lt.sVGH",	"c.nge.sVGH",	"c.le.sVGH",	"c.ngt.sVGH",
}

local map_cop1d = {
  shift = 0, mask = 63,
  [0] = "add.dFGH",	"sub.dFGH",	"mul.dFGH",	"div.dFGH",
  "sqrt.dFG",		"abs.dFG",	"mov.dFG",	"neg.dFG",
  "round.l.dFG",	"trunc.l.dFG",	"ceil.l.dFG",	"floor.l.dFG",
  "round.w.dFG",	"trunc.w.dFG",	"ceil.w.dFG",	"floor.w.dFG",
  false,
  { shift = 16, mask = 1, [0] = "movf.dFGC", "movt.dFGC" },
  "movz.dFGT",	"movn.dFGT",
  false,	"recip.dFG",	"rsqrt.dFG",	false,
  false,	false,		false,		false,
  false,	false,		false,		false,
  "cvt.s.dFG",	false,		false,		false,
  "cvt.w.dFG",	"cvt.l.dFG",	false,		false,
  false,	false,		false,		false,
  false,	false,		false,		false,
  "c.f.dVGH",	"c.un.dVGH",	"c.eq.dVGH",	"c.ueq.dVGH",
  "c.olt.dVGH",	"c.ult.dVGH",	"c.ole.dVGH",	"c.ule.dVGH",
  "c.df.dVGH",	"c.ngle.dVGH",	"c.deq.dVGH",	"c.ngl.dVGH",
  "c.lt.dVGH",	"c.nge.dVGH",	"c.le.dVGH",	"c.ngt.dVGH",
}

local map_cop1ps = {
  shift = 0, mask = 63,
  [0] = "add.psFGH",	"sub.psFGH",	"mul.psFGH",	false,
  false,		"abs.psFG",	"mov.psFG",	"neg.psFG",
  false,		false,		false,		false,
  false,		false,		false,		false,
  false,
  { shift = 16, mask = 1, [0] = "movf.psFGC", "movt.psFGC" },
  "movz.psFGT",	"movn.psFGT",
  false,	false,		false,		false,
  false,	false,		false,		false,
  false,	false,		false,		false,
  "cvt.s.puFG",	false,		false,		false,
  false,	false,		false,		false,
  "cvt.s.plFG",	false,		false,		false,
  "pll.psFGH",	"plu.psFGH",	"pul.psFGH",	"puu.psFGH",
  "c.f.psVGH",	"c.un.psVGH",	"c.eq.psVGH",	"c.ueq.psVGH",
  "c.olt.psVGH", "c.ult.psVGH",	"c.ole.psVGH",	"c.ule.psVGH",
  "c.psf.psVGH", "c.ngle.psVGH", "c.pseq.psVGH", "c.ngl.psVGH",
  "c.lt.psVGH",	"c.nge.psVGH",	"c.le.psVGH",	"c.ngt.psVGH",
}

local map_cop1w = {
  shift = 0, mask = 63,
  [32] = "cvt.s.wFG", [33] = "cvt.d.wFG",
}

local map_cop1l = {
  shift = 0, mask = 63,
  [32] = "cvt.s.lFG", [33] = "cvt.d.lFG",
}

local map_cop1bc = {
  shift = 16, mask = 3,
  [0] = "bc1fCB", "bc1tCB",	"bc1flCB",	"bc1tlCB",
}

local map_cop1 = {
  shift = 21, mask = 31,
  [0] = "mfc1TG", false,	"cfc1TG",	"mfhc1TG",
  "mtc1TG",	false,		"ctc1TG",	"mthc1TG",
  map_cop1bc,	false,		false,		false,
  false,	false,		false,		false,
  map_cop1s,	map_cop1d,	false,		false,
  map_cop1w,	map_cop1l,	map_cop1ps,
}

local map_cop1x = {
  shift = 0, mask = 63,
  [0] = "lwxc1FSX",	"ldxc1FSX",	false,		false,
  false,	"luxc1FSX",	false,		false,
  "swxc1FSX",	"sdxc1FSX",	false,		false,
  false,	"suxc1FSX",	false,		"prefxMSX",
  false,	false,		false,		false,
  false,	false,		false,		false,
  false,	false,		false,		false,
  false,	false,		"alnv.psFGHS",	false,
  "madd.sFRGH",	"madd.dFRGH",	false,		false,
  false,	false,		"madd.psFRGH",	false,
  "msub.sFRGH",	"msub.dFRGH",	false,		false,
  false,	false,		"msub.psFRGH",	false,
  "nmadd.sFRGH", "nmadd.dFRGH",	false,		false,
  false,	false,		"nmadd.psFRGH",	false,
  "nmsub.sFRGH", "nmsub.dFRGH",	false,		false,
  false,	false,		"nmsub.psFRGH",	false,
}

local map_pri = {
  [0] = map_special,	map_regimm,	"jJ",	"jalJ",
  "beq|beqz|bST00B",	"bne|bnezST0B",		"blezSB",	"bgtzSB",
  "addiTSI",	"addiu|liTS0I",	"sltiTSI",	"sltiuTSI",
  "andiTSU",	"ori|liTS0U",	"xoriTSU",	"luiTU",
  map_cop0,	map_cop1,	false,		map_cop1x,
  "beql|beqzlST0B",	"bnel|bnezlST0B",	"blezlSB",	"bgtzlSB",
  false,	false,		false,		false,
  map_special2,	false,		false,		map_special3,
  "lbTSO",	"lhTSO",	"lwlTSO",	"lwTSO",
  "lbuTSO",	"lhuTSO",	"lwrTSO",	false,
  "sbTSO",	"shTSO",	"swlTSO",	"swTSO",
  false,	false,		"swrTSO",	"cacheNSO",
  "llTSO",	"lwc1HSO",	"lwc2TSO",	"prefNSO",
  false,	"ldc1HSO",	"ldc2TSO",	false,
  "scTSO",	"swc1HSO",	"swc2TSO",	false,
  false,	"sdc1HSO",	"sdc2TSO",	false,
}

------------------------------------------------------------------------------

local map_gpr = {
  [0] = "r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
  "r24", "r25", "r26", "r27", "r28", "sp", "r30", "ra",
}

------------------------------------------------------------------------------

-- Output a nicely formatted line with an opcode and operands.
local function putop(ctx, text, operands)
  local pos = ctx.pos
  local extra = ""
  if ctx.rel then
    local sym = ctx.symtab[ctx.rel]
    if sym then extra = "\t->"..sym end
  end
  if ctx.hexdump > 0 then
    ctx.out(format("%08x  %s  %-7s %s%s\n",
	    ctx.addr+pos, tohex(ctx.op), text, concat(operands, ", "), extra))
  else
    ctx.out(format("%08x  %-7s %s%s\n",
	    ctx.addr+pos, text, concat(operands, ", "), extra))
  end
  ctx.pos = pos + 4
end

-- Fallback for unknown opcodes.
local function unknown(ctx)
  return putop(ctx, ".long", { "0x"..tohex(ctx.op) })
end

local function get_be(ctx)
  local pos = ctx.pos
  local b0, b1, b2, b3 = byte(ctx.code, pos+1, pos+4)
  return bor(lshift(b0, 24), lshift(b1, 16), lshift(b2, 8), b3)
end

local function get_le(ctx)
  local pos = ctx.pos
  local b0, b1, b2, b3 = byte(ctx.code, pos+1, pos+4)
  return bor(lshift(b3, 24), lshift(b2, 16), lshift(b1, 8), b0)
end

-- Disassemble a single instruction.
local function disass_ins(ctx)
  local op = ctx:get()
  local operands = {}
  local last = nil
  ctx.op = op
  ctx.rel = nil

  local opat = map_pri[rshift(op, 26)]
  while type(opat) ~= "string" do
    if not opat then return unknown(ctx) end
    opat = opat[band(rshift(op, opat.shift), opat.mask)] or opat._
  end
  local name, pat = match(opat, "^([a-z0-9_.]*)(.*)")
  local altname, pat2 = match(pat, "|([a-z0-9_.|]*)(.*)")
  if altname then pat = pat2 end

  for p in gmatch(pat, ".") do
    local x = nil
    if p == "S" then
      x = map_gpr[band(rshift(op, 21), 31)]
    elseif p == "T" then
      x = map_gpr[band(rshift(op, 16), 31)]
    elseif p == "D" then
      x = map_gpr[band(rshift(op, 11), 31)]
    elseif p == "F" then
      x = "f"..band(rshift(op, 6), 31)
    elseif p == "G" then
      x = "f"..band(rshift(op, 11), 31)
    elseif p == "H" then
      x = "f"..band(rshift(op, 16), 31)
    elseif p == "R" then
      x = "f"..band(rshift(op, 21), 31)
    elseif p == "A" then
      x = band(rshift(op, 6), 31)
    elseif p == "M" then
      x = band(rshift(op, 11), 31)
    elseif p == "N" then
      x = band(rshift(op, 16), 31)
    elseif p == "C" then
      x = band(rshift(op, 18), 7)
      if x == 0 then x = nil end
    elseif p == "K" then
      x = band(rshift(op, 11), 31) + 1
    elseif p == "L" then
      x = band(rshift(op, 11), 31) - last + 1
    elseif p == "I" then
      x = arshift(lshift(op, 16), 16)
    elseif p == "U" then
      x = band(op, 0xffff)
    elseif p == "O" then
      local disp = arshift(lshift(op, 16), 16)
      operands[#operands] = format("%d(%s)", disp, last)
    elseif p == "X" then
      local index = map_gpr[band(rshift(op, 16), 31)]
      operands[#operands] = format("%s(%s)", index, last)
    elseif p == "B" then
      x = ctx.addr + ctx.pos + arshift(lshift(op, 16), 16)*4 + 4
      ctx.rel = x
      x = "0x"..tohex(x)
    elseif p == "J" then
      x = band(ctx.addr + ctx.pos, 0xf0000000) + band(op, 0x03ffffff)*4
      ctx.rel = x
      x = "0x"..tohex(x)
    elseif p == "V" then
      x = band(rshift(op, 8), 7)
      if x == 0 then x = nil end
    elseif p == "W" then
      x = band(op, 7)
      if x == 0 then x = nil end
    elseif p == "Y" then
      x = band(rshift(op, 6), 0x000fffff)
      if x == 0 then x = nil end
    elseif p == "Z" then
      x = band(rshift(op, 6), 1023)
      if x == 0 then x = nil end
    elseif p == "0" then
      if last == "r0" or last == 0 then
	local n = #operands
	operands[n] = nil
	last = operands[n-1]
	if altname then
	  local a1, a2 = match(altname, "([^|]*)|(.*)")
	  if a1 then name, altname = a1, a2
	  else name = altname end
	end
      end
    elseif p == "1" then
      if last == "ra" then
	operands[#operands] = nil
      end
    else
      assert(false)
    end
    if x then operands[#operands+1] = x; last = x end
  end

  return putop(ctx, name, operands)
end

------------------------------------------------------------------------------

-- Disassemble a block of code.
local function disass_block(ctx, ofs, len)
  if not ofs then ofs = 0 end
  local stop = len and ofs+len or #ctx.code
  stop = stop - stop % 4
  ctx.pos = ofs - ofs % 4
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
  ctx.get = get_be
  return ctx
end

local function create_el_(code, addr, out)
  local ctx = create_(code, addr, out)
  ctx.get = get_le
  return ctx
end

-- Simple API: disassemble code (a string) at address and output via out.
local function disass_(code, addr, out)
  create_(code, addr, out):disass()
end

local function disass_el_(code, addr, out)
  create_el_(code, addr, out):disass()
end

-- Return register name for RID.
local function regname_(r)
  if r < 32 then return map_gpr[r] end
  return "f"..(r-32)
end

-- Public module functions.
module(...)

create = create_
create_el = create_el_
disass = disass_
disass_el = disass_el_
regname = regname_

