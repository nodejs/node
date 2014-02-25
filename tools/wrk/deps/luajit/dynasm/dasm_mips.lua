------------------------------------------------------------------------------
-- DynASM MIPS module.
--
-- Copyright (C) 2005-2013 Mike Pall. All rights reserved.
-- See dynasm.lua for full copyright notice.
------------------------------------------------------------------------------

-- Module information:
local _info = {
  arch =	"mips",
  description =	"DynASM MIPS module",
  version =	"1.3.0",
  vernum =	 10300,
  release =	"2012-01-23",
  author =	"Mike Pall",
  license =	"MIT",
}

-- Exported glue functions for the arch-specific module.
local _M = { _info = _info }

-- Cache library functions.
local type, tonumber, pairs, ipairs = type, tonumber, pairs, ipairs
local assert, setmetatable = assert, setmetatable
local _s = string
local sub, format, byte, char = _s.sub, _s.format, _s.byte, _s.char
local match, gmatch = _s.match, _s.gmatch
local concat, sort = table.concat, table.sort
local bit = bit or require("bit")
local band, shl, sar, tohex = bit.band, bit.lshift, bit.arshift, bit.tohex

-- Inherited tables and callbacks.
local g_opt, g_arch
local wline, werror, wfatal, wwarn

-- Action name list.
-- CHECK: Keep this in sync with the C code!
local action_names = {
  "STOP", "SECTION", "ESC", "REL_EXT",
  "ALIGN", "REL_LG", "LABEL_LG",
  "REL_PC", "LABEL_PC", "IMM",
}

-- Maximum number of section buffer positions for dasm_put().
-- CHECK: Keep this in sync with the C code!
local maxsecpos = 25 -- Keep this low, to avoid excessively long C lines.

-- Action name -> action number.
local map_action = {}
for n,name in ipairs(action_names) do
  map_action[name] = n-1
end

-- Action list buffer.
local actlist = {}

-- Argument list for next dasm_put(). Start with offset 0 into action list.
local actargs = { 0 }

-- Current number of section buffer positions for dasm_put().
local secpos = 1

------------------------------------------------------------------------------

-- Dump action names and numbers.
local function dumpactions(out)
  out:write("DynASM encoding engine action codes:\n")
  for n,name in ipairs(action_names) do
    local num = map_action[name]
    out:write(format("  %-10s %02X  %d\n", name, num, num))
  end
  out:write("\n")
end

-- Write action list buffer as a huge static C array.
local function writeactions(out, name)
  local nn = #actlist
  if nn == 0 then nn = 1; actlist[0] = map_action.STOP end
  out:write("static const unsigned int ", name, "[", nn, "] = {\n")
  for i = 1,nn-1 do
    assert(out:write("0x", tohex(actlist[i]), ",\n"))
  end
  assert(out:write("0x", tohex(actlist[nn]), "\n};\n\n"))
end

------------------------------------------------------------------------------

-- Add word to action list.
local function wputxw(n)
  assert(n >= 0 and n <= 0xffffffff and n % 1 == 0, "word out of range")
  actlist[#actlist+1] = n
end

-- Add action to list with optional arg. Advance buffer pos, too.
local function waction(action, val, a, num)
  local w = assert(map_action[action], "bad action name `"..action.."'")
  wputxw(0xff000000 + w * 0x10000 + (val or 0))
  if a then actargs[#actargs+1] = a end
  if a or num then secpos = secpos + (num or 1) end
end

-- Flush action list (intervening C code or buffer pos overflow).
local function wflush(term)
  if #actlist == actargs[1] then return end -- Nothing to flush.
  if not term then waction("STOP") end -- Terminate action list.
  wline(format("dasm_put(Dst, %s);", concat(actargs, ", ")), true)
  actargs = { #actlist } -- Actionlist offset is 1st arg to next dasm_put().
  secpos = 1 -- The actionlist offset occupies a buffer position, too.
end

-- Put escaped word.
local function wputw(n)
  if n >= 0xff000000 then waction("ESC") end
  wputxw(n)
end

-- Reserve position for word.
local function wpos()
  local pos = #actlist+1
  actlist[pos] = ""
  return pos
end

-- Store word to reserved position.
local function wputpos(pos, n)
  assert(n >= 0 and n <= 0xffffffff and n % 1 == 0, "word out of range")
  actlist[pos] = n
end

------------------------------------------------------------------------------

-- Global label name -> global label number. With auto assignment on 1st use.
local next_global = 20
local map_global = setmetatable({}, { __index = function(t, name)
  if not match(name, "^[%a_][%w_]*$") then werror("bad global label") end
  local n = next_global
  if n > 2047 then werror("too many global labels") end
  next_global = n + 1
  t[name] = n
  return n
end})

-- Dump global labels.
local function dumpglobals(out, lvl)
  local t = {}
  for name, n in pairs(map_global) do t[n] = name end
  out:write("Global labels:\n")
  for i=20,next_global-1 do
    out:write(format("  %s\n", t[i]))
  end
  out:write("\n")
end

-- Write global label enum.
local function writeglobals(out, prefix)
  local t = {}
  for name, n in pairs(map_global) do t[n] = name end
  out:write("enum {\n")
  for i=20,next_global-1 do
    out:write("  ", prefix, t[i], ",\n")
  end
  out:write("  ", prefix, "_MAX\n};\n")
end

-- Write global label names.
local function writeglobalnames(out, name)
  local t = {}
  for name, n in pairs(map_global) do t[n] = name end
  out:write("static const char *const ", name, "[] = {\n")
  for i=20,next_global-1 do
    out:write("  \"", t[i], "\",\n")
  end
  out:write("  (const char *)0\n};\n")
end

------------------------------------------------------------------------------

-- Extern label name -> extern label number. With auto assignment on 1st use.
local next_extern = 0
local map_extern_ = {}
local map_extern = setmetatable({}, { __index = function(t, name)
  -- No restrictions on the name for now.
  local n = next_extern
  if n > 2047 then werror("too many extern labels") end
  next_extern = n + 1
  t[name] = n
  map_extern_[n] = name
  return n
end})

-- Dump extern labels.
local function dumpexterns(out, lvl)
  out:write("Extern labels:\n")
  for i=0,next_extern-1 do
    out:write(format("  %s\n", map_extern_[i]))
  end
  out:write("\n")
end

-- Write extern label names.
local function writeexternnames(out, name)
  out:write("static const char *const ", name, "[] = {\n")
  for i=0,next_extern-1 do
    out:write("  \"", map_extern_[i], "\",\n")
  end
  out:write("  (const char *)0\n};\n")
end

------------------------------------------------------------------------------

-- Arch-specific maps.
local map_archdef = { sp="r29", ra="r31" } -- Ext. register name -> int. name.

local map_type = {}		-- Type name -> { ctype, reg }
local ctypenum = 0		-- Type number (for Dt... macros).

-- Reverse defines for registers.
function _M.revdef(s)
  if s == "r29" then return "sp"
  elseif s == "r31" then return "ra" end
  return s
end

------------------------------------------------------------------------------

-- Template strings for MIPS instructions.
local map_op = {
  -- First-level opcodes.
  j_1 =		"08000000J",
  jal_1 =	"0c000000J",
  b_1 =		"10000000B",
  beqz_2 =	"10000000SB",
  beq_3 =	"10000000STB",
  bnez_2 =	"14000000SB",
  bne_3 =	"14000000STB",
  blez_2 =	"18000000SB",
  bgtz_2 =	"1c000000SB",
  addi_3 =	"20000000TSI",
  li_2 =	"24000000TI",
  addiu_3 =	"24000000TSI",
  slti_3 =	"28000000TSI",
  sltiu_3 =	"2c000000TSI",
  andi_3 =	"30000000TSU",
  lu_2 =	"34000000TU",
  ori_3 =	"34000000TSU",
  xori_3 =	"38000000TSU",
  lui_2 =	"3c000000TU",
  beqzl_2 =	"50000000SB",
  beql_3 =	"50000000STB",
  bnezl_2 =	"54000000SB",
  bnel_3 =	"54000000STB",
  blezl_2 =	"58000000SB",
  bgtzl_2 =	"5c000000SB",
  lb_2 =	"80000000TO",
  lh_2 =	"84000000TO",
  lwl_2 =	"88000000TO",
  lw_2 =	"8c000000TO",
  lbu_2 =	"90000000TO",
  lhu_2 =	"94000000TO",
  lwr_2 =	"98000000TO",
  sb_2 =	"a0000000TO",
  sh_2 =	"a4000000TO",
  swl_2 =	"a8000000TO",
  sw_2 =	"ac000000TO",
  swr_2 =	"b8000000TO",
  cache_2 =	"bc000000NO",
  ll_2 =	"c0000000TO",
  lwc1_2 =	"c4000000HO",
  pref_2 =	"cc000000NO",
  ldc1_2 =	"d4000000HO",
  sc_2 =	"e0000000TO",
  swc1_2 =	"e4000000HO",
  sdc1_2 =	"f4000000HO",

  -- Opcode SPECIAL.
  nop_0 =	"00000000",
  sll_3 =	"00000000DTA",
  movf_2 =	"00000001DS",
  movf_3 =	"00000001DSC",
  movt_2 =	"00010001DS",
  movt_3 =	"00010001DSC",
  srl_3 =	"00000002DTA",
  rotr_3 =	"00200002DTA",
  sra_3 =	"00000003DTA",
  sllv_3 =	"00000004DTS",
  srlv_3 =	"00000006DTS",
  rotrv_3 =	"00000046DTS",
  srav_3 =	"00000007DTS",
  jr_1 =	"00000008S",
  jalr_1 =	"0000f809S",
  jalr_2 =	"00000009DS",
  movz_3 =	"0000000aDST",
  movn_3 =	"0000000bDST",
  syscall_0 =	"0000000c",
  syscall_1 =	"0000000cY",
  break_0 =	"0000000d",
  break_1 =	"0000000dY",
  sync_0 =	"0000000f",
  mfhi_1 =	"00000010D",
  mthi_1 =	"00000011S",
  mflo_1 =	"00000012D",
  mtlo_1 =	"00000013S",
  mult_2 =	"00000018ST",
  multu_2 =	"00000019ST",
  div_2 =	"0000001aST",
  divu_2 =	"0000001bST",
  add_3 =	"00000020DST",
  move_2 =	"00000021DS",
  addu_3 =	"00000021DST",
  sub_3 =	"00000022DST",
  negu_2 =	"00000023DT",
  subu_3 =	"00000023DST",
  and_3 =	"00000024DST",
  or_3 =	"00000025DST",
  xor_3 =	"00000026DST",
  not_2 =	"00000027DS",
  nor_3 =	"00000027DST",
  slt_3 =	"0000002aDST",
  sltu_3 =	"0000002bDST",
  tge_2 =	"00000030ST",
  tge_3 =	"00000030STZ",
  tgeu_2 =	"00000031ST",
  tgeu_3 =	"00000031STZ",
  tlt_2 =	"00000032ST",
  tlt_3 =	"00000032STZ",
  tltu_2 =	"00000033ST",
  tltu_3 =	"00000033STZ",
  teq_2 =	"00000034ST",
  teq_3 =	"00000034STZ",
  tne_2 =	"00000036ST",
  tne_3 =	"00000036STZ",

  -- Opcode REGIMM.
  bltz_2 =	"04000000SB",
  bgez_2 =	"04010000SB",
  bltzl_2 =	"04020000SB",
  bgezl_2 =	"04030000SB",
  tgei_2 =	"04080000SI",
  tgeiu_2 =	"04090000SI",
  tlti_2 =	"040a0000SI",
  tltiu_2 =	"040b0000SI",
  teqi_2 =	"040c0000SI",
  tnei_2 =	"040e0000SI",
  bltzal_2 =	"04100000SB",
  bal_1 =	"04110000B",
  bgezal_2 =	"04110000SB",
  bltzall_2 =	"04120000SB",
  bgezall_2 =	"04130000SB",
  synci_1 =	"041f0000O",

  -- Opcode SPECIAL2.
  madd_2 =	"70000000ST",
  maddu_2 =	"70000001ST",
  mul_3 =	"70000002DST",
  msub_2 =	"70000004ST",
  msubu_2 =	"70000005ST",
  clz_2 =	"70000020DS=",
  clo_2 =	"70000021DS=",
  sdbbp_0 =	"7000003f",
  sdbbp_1 =	"7000003fY",

  -- Opcode SPECIAL3.
  ext_4 =	"7c000000TSAM", -- Note: last arg is msbd = size-1
  ins_4 =	"7c000004TSAM", -- Note: last arg is msb = pos+size-1
  wsbh_2 =	"7c0000a0DT",
  seb_2 =	"7c000420DT",
  seh_2 =	"7c000620DT",
  rdhwr_2 =	"7c00003bTD",

  -- Opcode COP0.
  mfc0_2 =	"40000000TD",
  mfc0_3 =	"40000000TDW",
  mtc0_2 =	"40800000TD",
  mtc0_3 =	"40800000TDW",
  rdpgpr_2 =	"41400000DT",
  di_0 =	"41606000",
  di_1 =	"41606000T",
  ei_0 =	"41606020",
  ei_1 =	"41606020T",
  wrpgpr_2 =	"41c00000DT",
  tlbr_0 =	"42000001",
  tlbwi_0 =	"42000002",
  tlbwr_0 =	"42000006",
  tlbp_0 =	"42000008",
  eret_0 =	"42000018",
  deret_0 =	"4200001f",
  wait_0 =	"42000020",

  -- Opcode COP1.
  mfc1_2 =	"44000000TG",
  cfc1_2 =	"44400000TG",
  mfhc1_2 =	"44600000TG",
  mtc1_2 =	"44800000TG",
  ctc1_2 =	"44c00000TG",
  mthc1_2 =	"44e00000TG",

  bc1f_1 =	"45000000B",
  bc1f_2 =	"45000000CB",
  bc1t_1 =	"45010000B",
  bc1t_2 =	"45010000CB",
  bc1fl_1 =	"45020000B",
  bc1fl_2 =	"45020000CB",
  bc1tl_1 =	"45030000B",
  bc1tl_2 =	"45030000CB",

  ["add.s_3"] =		"46000000FGH",
  ["sub.s_3"] =		"46000001FGH",
  ["mul.s_3"] =		"46000002FGH",
  ["div.s_3"] =		"46000003FGH",
  ["sqrt.s_2"] =	"46000004FG",
  ["abs.s_2"] =		"46000005FG",
  ["mov.s_2"] =		"46000006FG",
  ["neg.s_2"] =		"46000007FG",
  ["round.l.s_2"] =	"46000008FG",
  ["trunc.l.s_2"] =	"46000009FG",
  ["ceil.l.s_2"] =	"4600000aFG",
  ["floor.l.s_2"] =	"4600000bFG",
  ["round.w.s_2"] =	"4600000cFG",
  ["trunc.w.s_2"] =	"4600000dFG",
  ["ceil.w.s_2"] =	"4600000eFG",
  ["floor.w.s_2"] =	"4600000fFG",
  ["movf.s_2"] =	"46000011FG",
  ["movf.s_3"] =	"46000011FGC",
  ["movt.s_2"] =	"46010011FG",
  ["movt.s_3"] =	"46010011FGC",
  ["movz.s_3"] =	"46000012FGT",
  ["movn.s_3"] =	"46000013FGT",
  ["recip.s_2"] =	"46000015FG",
  ["rsqrt.s_2"] =	"46000016FG",
  ["cvt.d.s_2"] =	"46000021FG",
  ["cvt.w.s_2"] =	"46000024FG",
  ["cvt.l.s_2"] =	"46000025FG",
  ["cvt.ps.s_3"] =	"46000026FGH",
  ["c.f.s_2"] =		"46000030GH",
  ["c.f.s_3"] =		"46000030VGH",
  ["c.un.s_2"] =	"46000031GH",
  ["c.un.s_3"] =	"46000031VGH",
  ["c.eq.s_2"] =	"46000032GH",
  ["c.eq.s_3"] =	"46000032VGH",
  ["c.ueq.s_2"] =	"46000033GH",
  ["c.ueq.s_3"] =	"46000033VGH",
  ["c.olt.s_2"] =	"46000034GH",
  ["c.olt.s_3"] =	"46000034VGH",
  ["c.ult.s_2"] =	"46000035GH",
  ["c.ult.s_3"] =	"46000035VGH",
  ["c.ole.s_2"] =	"46000036GH",
  ["c.ole.s_3"] =	"46000036VGH",
  ["c.ule.s_2"] =	"46000037GH",
  ["c.ule.s_3"] =	"46000037VGH",
  ["c.sf.s_2"] =	"46000038GH",
  ["c.sf.s_3"] =	"46000038VGH",
  ["c.ngle.s_2"] =	"46000039GH",
  ["c.ngle.s_3"] =	"46000039VGH",
  ["c.seq.s_2"] =	"4600003aGH",
  ["c.seq.s_3"] =	"4600003aVGH",
  ["c.ngl.s_2"] =	"4600003bGH",
  ["c.ngl.s_3"] =	"4600003bVGH",
  ["c.lt.s_2"] =	"4600003cGH",
  ["c.lt.s_3"] =	"4600003cVGH",
  ["c.nge.s_2"] =	"4600003dGH",
  ["c.nge.s_3"] =	"4600003dVGH",
  ["c.le.s_2"] =	"4600003eGH",
  ["c.le.s_3"] =	"4600003eVGH",
  ["c.ngt.s_2"] =	"4600003fGH",
  ["c.ngt.s_3"] =	"4600003fVGH",

  ["add.d_3"] =		"46200000FGH",
  ["sub.d_3"] =		"46200001FGH",
  ["mul.d_3"] =		"46200002FGH",
  ["div.d_3"] =		"46200003FGH",
  ["sqrt.d_2"] =	"46200004FG",
  ["abs.d_2"] =		"46200005FG",
  ["mov.d_2"] =		"46200006FG",
  ["neg.d_2"] =		"46200007FG",
  ["round.l.d_2"] =	"46200008FG",
  ["trunc.l.d_2"] =	"46200009FG",
  ["ceil.l.d_2"] =	"4620000aFG",
  ["floor.l.d_2"] =	"4620000bFG",
  ["round.w.d_2"] =	"4620000cFG",
  ["trunc.w.d_2"] =	"4620000dFG",
  ["ceil.w.d_2"] =	"4620000eFG",
  ["floor.w.d_2"] =	"4620000fFG",
  ["movf.d_2"] =	"46200011FG",
  ["movf.d_3"] =	"46200011FGC",
  ["movt.d_2"] =	"46210011FG",
  ["movt.d_3"] =	"46210011FGC",
  ["movz.d_3"] =	"46200012FGT",
  ["movn.d_3"] =	"46200013FGT",
  ["recip.d_2"] =	"46200015FG",
  ["rsqrt.d_2"] =	"46200016FG",
  ["cvt.s.d_2"] =	"46200020FG",
  ["cvt.w.d_2"] =	"46200024FG",
  ["cvt.l.d_2"] =	"46200025FG",
  ["c.f.d_2"] =		"46200030GH",
  ["c.f.d_3"] =		"46200030VGH",
  ["c.un.d_2"] =	"46200031GH",
  ["c.un.d_3"] =	"46200031VGH",
  ["c.eq.d_2"] =	"46200032GH",
  ["c.eq.d_3"] =	"46200032VGH",
  ["c.ueq.d_2"] =	"46200033GH",
  ["c.ueq.d_3"] =	"46200033VGH",
  ["c.olt.d_2"] =	"46200034GH",
  ["c.olt.d_3"] =	"46200034VGH",
  ["c.ult.d_2"] =	"46200035GH",
  ["c.ult.d_3"] =	"46200035VGH",
  ["c.ole.d_2"] =	"46200036GH",
  ["c.ole.d_3"] =	"46200036VGH",
  ["c.ule.d_2"] =	"46200037GH",
  ["c.ule.d_3"] =	"46200037VGH",
  ["c.sf.d_2"] =	"46200038GH",
  ["c.sf.d_3"] =	"46200038VGH",
  ["c.ngle.d_2"] =	"46200039GH",
  ["c.ngle.d_3"] =	"46200039VGH",
  ["c.seq.d_2"] =	"4620003aGH",
  ["c.seq.d_3"] =	"4620003aVGH",
  ["c.ngl.d_2"] =	"4620003bGH",
  ["c.ngl.d_3"] =	"4620003bVGH",
  ["c.lt.d_2"] =	"4620003cGH",
  ["c.lt.d_3"] =	"4620003cVGH",
  ["c.nge.d_2"] =	"4620003dGH",
  ["c.nge.d_3"] =	"4620003dVGH",
  ["c.le.d_2"] =	"4620003eGH",
  ["c.le.d_3"] =	"4620003eVGH",
  ["c.ngt.d_2"] =	"4620003fGH",
  ["c.ngt.d_3"] =	"4620003fVGH",

  ["add.ps_3"] =	"46c00000FGH",
  ["sub.ps_3"] =	"46c00001FGH",
  ["mul.ps_3"] =	"46c00002FGH",
  ["abs.ps_2"] =	"46c00005FG",
  ["mov.ps_2"] =	"46c00006FG",
  ["neg.ps_2"] =	"46c00007FG",
  ["movf.ps_2"] =	"46c00011FG",
  ["movf.ps_3"] =	"46c00011FGC",
  ["movt.ps_2"] =	"46c10011FG",
  ["movt.ps_3"] =	"46c10011FGC",
  ["movz.ps_3"] =	"46c00012FGT",
  ["movn.ps_3"] =	"46c00013FGT",
  ["cvt.s.pu_2"] =	"46c00020FG",
  ["cvt.s.pl_2"] =	"46c00028FG",
  ["pll.ps_3"] =	"46c0002cFGH",
  ["plu.ps_3"] =	"46c0002dFGH",
  ["pul.ps_3"] =	"46c0002eFGH",
  ["puu.ps_3"] =	"46c0002fFGH",
  ["c.f.ps_2"] =	"46c00030GH",
  ["c.f.ps_3"] =	"46c00030VGH",
  ["c.un.ps_2"] =	"46c00031GH",
  ["c.un.ps_3"] =	"46c00031VGH",
  ["c.eq.ps_2"] =	"46c00032GH",
  ["c.eq.ps_3"] =	"46c00032VGH",
  ["c.ueq.ps_2"] =	"46c00033GH",
  ["c.ueq.ps_3"] =	"46c00033VGH",
  ["c.olt.ps_2"] =	"46c00034GH",
  ["c.olt.ps_3"] =	"46c00034VGH",
  ["c.ult.ps_2"] =	"46c00035GH",
  ["c.ult.ps_3"] =	"46c00035VGH",
  ["c.ole.ps_2"] =	"46c00036GH",
  ["c.ole.ps_3"] =	"46c00036VGH",
  ["c.ule.ps_2"] =	"46c00037GH",
  ["c.ule.ps_3"] =	"46c00037VGH",
  ["c.sf.ps_2"] =	"46c00038GH",
  ["c.sf.ps_3"] =	"46c00038VGH",
  ["c.ngle.ps_2"] =	"46c00039GH",
  ["c.ngle.ps_3"] =	"46c00039VGH",
  ["c.seq.ps_2"] =	"46c0003aGH",
  ["c.seq.ps_3"] =	"46c0003aVGH",
  ["c.ngl.ps_2"] =	"46c0003bGH",
  ["c.ngl.ps_3"] =	"46c0003bVGH",
  ["c.lt.ps_2"] =	"46c0003cGH",
  ["c.lt.ps_3"] =	"46c0003cVGH",
  ["c.nge.ps_2"] =	"46c0003dGH",
  ["c.nge.ps_3"] =	"46c0003dVGH",
  ["c.le.ps_2"] =	"46c0003eGH",
  ["c.le.ps_3"] =	"46c0003eVGH",
  ["c.ngt.ps_2"] =	"46c0003fGH",
  ["c.ngt.ps_3"] =	"46c0003fVGH",

  ["cvt.s.w_2"] =	"46800020FG",
  ["cvt.d.w_2"] =	"46800021FG",

  ["cvt.s.l_2"] =	"46a00020FG",
  ["cvt.d.l_2"] =	"46a00021FG",

  -- Opcode COP1X.
  lwxc1_2 =		"4c000000FX",
  ldxc1_2 =		"4c000001FX",
  luxc1_2 =		"4c000005FX",
  swxc1_2 =		"4c000008FX",
  sdxc1_2 =		"4c000009FX",
  suxc1_2 =		"4c00000dFX",
  prefx_2 =		"4c00000fMX",
  ["alnv.ps_4"] =	"4c00001eFGHS",
  ["madd.s_4"] =	"4c000020FRGH",
  ["madd.d_4"] =	"4c000021FRGH",
  ["madd.ps_4"] =	"4c000026FRGH",
  ["msub.s_4"] =	"4c000028FRGH",
  ["msub.d_4"] =	"4c000029FRGH",
  ["msub.ps_4"] =	"4c00002eFRGH",
  ["nmadd.s_4"] =	"4c000030FRGH",
  ["nmadd.d_4"] =	"4c000031FRGH",
  ["nmadd.ps_4"] =	"4c000036FRGH",
  ["nmsub.s_4"] =	"4c000038FRGH",
  ["nmsub.d_4"] =	"4c000039FRGH",
  ["nmsub.ps_4"] =	"4c00003eFRGH",
}

------------------------------------------------------------------------------

local function parse_gpr(expr)
  local tname, ovreg = match(expr, "^([%w_]+):(r[1-3]?[0-9])$")
  local tp = map_type[tname or expr]
  if tp then
    local reg = ovreg or tp.reg
    if not reg then
      werror("type `"..(tname or expr).."' needs a register override")
    end
    expr = reg
  end
  local r = match(expr, "^r([1-3]?[0-9])$")
  if r then
    r = tonumber(r)
    if r <= 31 then return r, tp end
  end
  werror("bad register name `"..expr.."'")
end

local function parse_fpr(expr)
  local r = match(expr, "^f([1-3]?[0-9])$")
  if r then
    r = tonumber(r)
    if r <= 31 then return r end
  end
  werror("bad register name `"..expr.."'")
end

local function parse_imm(imm, bits, shift, scale, signed)
  local n = tonumber(imm)
  if n then
    local m = sar(n, scale)
    if shl(m, scale) == n then
      if signed then
	local s = sar(m, bits-1)
	if s == 0 then return shl(m, shift)
	elseif s == -1 then return shl(m + shl(1, bits), shift) end
      else
	if sar(m, bits) == 0 then return shl(m, shift) end
      end
    end
    werror("out of range immediate `"..imm.."'")
  elseif match(imm, "^[rf]([1-3]?[0-9])$") or
	 match(imm, "^([%w_]+):([rf][1-3]?[0-9])$") then
    werror("expected immediate operand, got register")
  else
    waction("IMM", (signed and 32768 or 0)+scale*1024+bits*32+shift, imm)
    return 0
  end
end

local function parse_disp(disp)
  local imm, reg = match(disp, "^(.*)%(([%w_:]+)%)$")
  if imm then
    local r = shl(parse_gpr(reg), 21)
    local extname = match(imm, "^extern%s+(%S+)$")
    if extname then
      waction("REL_EXT", map_extern[extname], nil, 1)
      return r
    else
      return r + parse_imm(imm, 16, 0, 0, true)
    end
  end
  local reg, tailr = match(disp, "^([%w_:]+)%s*(.*)$")
  if reg and tailr ~= "" then
    local r, tp = parse_gpr(reg)
    if tp then
      waction("IMM", 32768+16*32, format(tp.ctypefmt, tailr))
      return shl(r, 21)
    end
  end
  werror("bad displacement `"..disp.."'")
end

local function parse_index(idx)
  local rt, rs = match(idx, "^(.*)%(([%w_:]+)%)$")
  if rt then
    rt = parse_gpr(rt)
    rs = parse_gpr(rs)
    return shl(rt, 16) + shl(rs, 21)
  end
  werror("bad index `"..idx.."'")
end

local function parse_label(label, def)
  local prefix = sub(label, 1, 2)
  -- =>label (pc label reference)
  if prefix == "=>" then
    return "PC", 0, sub(label, 3)
  end
  -- ->name (global label reference)
  if prefix == "->" then
    return "LG", map_global[sub(label, 3)]
  end
  if def then
    -- [1-9] (local label definition)
    if match(label, "^[1-9]$") then
      return "LG", 10+tonumber(label)
    end
  else
    -- [<>][1-9] (local label reference)
    local dir, lnum = match(label, "^([<>])([1-9])$")
    if dir then -- Fwd: 1-9, Bkwd: 11-19.
      return "LG", lnum + (dir == ">" and 0 or 10)
    end
    -- extern label (extern label reference)
    local extname = match(label, "^extern%s+(%S+)$")
    if extname then
      return "EXT", map_extern[extname]
    end
  end
  werror("bad label `"..label.."'")
end

------------------------------------------------------------------------------

-- Handle opcodes defined with template strings.
map_op[".template__"] = function(params, template, nparams)
  if not params then return sub(template, 9) end
  local op = tonumber(sub(template, 1, 8), 16)
  local n = 1

  -- Limit number of section buffer positions used by a single dasm_put().
  -- A single opcode needs a maximum of 2 positions (ins/ext).
  if secpos+2 > maxsecpos then wflush() end
  local pos = wpos()

  -- Process each character.
  for p in gmatch(sub(template, 9), ".") do
    if p == "D" then
      op = op + shl(parse_gpr(params[n]), 11); n = n + 1
    elseif p == "T" then
      op = op + shl(parse_gpr(params[n]), 16); n = n + 1
    elseif p == "S" then
      op = op + shl(parse_gpr(params[n]), 21); n = n + 1
    elseif p == "F" then
      op = op + shl(parse_fpr(params[n]), 6); n = n + 1
    elseif p == "G" then
      op = op + shl(parse_fpr(params[n]), 11); n = n + 1
    elseif p == "H" then
      op = op + shl(parse_fpr(params[n]), 16); n = n + 1
    elseif p == "R" then
      op = op + shl(parse_fpr(params[n]), 21); n = n + 1
    elseif p == "I" then
      op = op + parse_imm(params[n], 16, 0, 0, true); n = n + 1
    elseif p == "U" then
      op = op + parse_imm(params[n], 16, 0, 0, false); n = n + 1
    elseif p == "O" then
      op = op + parse_disp(params[n]); n = n + 1
    elseif p == "X" then
      op = op + parse_index(params[n]); n = n + 1
    elseif p == "B" or p == "J" then
      local mode, n, s = parse_label(params[n], false)
      if p == "B" then n = n + 2048 end
      waction("REL_"..mode, n, s, 1)
      n = n + 1
    elseif p == "A" then
      op = op + parse_imm(params[n], 5, 6, 0, false); n = n + 1
    elseif p == "M" then
      op = op + parse_imm(params[n], 5, 11, 0, false); n = n + 1
    elseif p == "N" then
      op = op + parse_imm(params[n], 5, 16, 0, false); n = n + 1
    elseif p == "C" then
      op = op + parse_imm(params[n], 3, 18, 0, false); n = n + 1
    elseif p == "V" then
      op = op + parse_imm(params[n], 3, 8, 0, false); n = n + 1
    elseif p == "W" then
      op = op + parse_imm(params[n], 3, 0, 0, false); n = n + 1
    elseif p == "Y" then
      op = op + parse_imm(params[n], 20, 6, 0, false); n = n + 1
    elseif p == "Z" then
      op = op + parse_imm(params[n], 10, 6, 0, false); n = n + 1
    elseif p == "=" then
      op = op + shl(band(op, 0xf800), 5) -- Copy D to T for clz, clo.
    else
      assert(false)
    end
  end
  wputpos(pos, op)
end

------------------------------------------------------------------------------

-- Pseudo-opcode to mark the position where the action list is to be emitted.
map_op[".actionlist_1"] = function(params)
  if not params then return "cvar" end
  local name = params[1] -- No syntax check. You get to keep the pieces.
  wline(function(out) writeactions(out, name) end)
end

-- Pseudo-opcode to mark the position where the global enum is to be emitted.
map_op[".globals_1"] = function(params)
  if not params then return "prefix" end
  local prefix = params[1] -- No syntax check. You get to keep the pieces.
  wline(function(out) writeglobals(out, prefix) end)
end

-- Pseudo-opcode to mark the position where the global names are to be emitted.
map_op[".globalnames_1"] = function(params)
  if not params then return "cvar" end
  local name = params[1] -- No syntax check. You get to keep the pieces.
  wline(function(out) writeglobalnames(out, name) end)
end

-- Pseudo-opcode to mark the position where the extern names are to be emitted.
map_op[".externnames_1"] = function(params)
  if not params then return "cvar" end
  local name = params[1] -- No syntax check. You get to keep the pieces.
  wline(function(out) writeexternnames(out, name) end)
end

------------------------------------------------------------------------------

-- Label pseudo-opcode (converted from trailing colon form).
map_op[".label_1"] = function(params)
  if not params then return "[1-9] | ->global | =>pcexpr" end
  if secpos+1 > maxsecpos then wflush() end
  local mode, n, s = parse_label(params[1], true)
  if mode == "EXT" then werror("bad label definition") end
  waction("LABEL_"..mode, n, s, 1)
end

------------------------------------------------------------------------------

-- Pseudo-opcodes for data storage.
map_op[".long_*"] = function(params)
  if not params then return "imm..." end
  for _,p in ipairs(params) do
    local n = tonumber(p)
    if not n then werror("bad immediate `"..p.."'") end
    if n < 0 then n = n + 2^32 end
    wputw(n)
    if secpos+2 > maxsecpos then wflush() end
  end
end

-- Alignment pseudo-opcode.
map_op[".align_1"] = function(params)
  if not params then return "numpow2" end
  if secpos+1 > maxsecpos then wflush() end
  local align = tonumber(params[1])
  if align then
    local x = align
    -- Must be a power of 2 in the range (2 ... 256).
    for i=1,8 do
      x = x / 2
      if x == 1 then
	waction("ALIGN", align-1, nil, 1) -- Action byte is 2**n-1.
	return
      end
    end
  end
  werror("bad alignment")
end

------------------------------------------------------------------------------

-- Pseudo-opcode for (primitive) type definitions (map to C types).
map_op[".type_3"] = function(params, nparams)
  if not params then
    return nparams == 2 and "name, ctype" or "name, ctype, reg"
  end
  local name, ctype, reg = params[1], params[2], params[3]
  if not match(name, "^[%a_][%w_]*$") then
    werror("bad type name `"..name.."'")
  end
  local tp = map_type[name]
  if tp then
    werror("duplicate type `"..name.."'")
  end
  -- Add #type to defines. A bit unclean to put it in map_archdef.
  map_archdef["#"..name] = "sizeof("..ctype..")"
  -- Add new type and emit shortcut define.
  local num = ctypenum + 1
  map_type[name] = {
    ctype = ctype,
    ctypefmt = format("Dt%X(%%s)", num),
    reg = reg,
  }
  wline(format("#define Dt%X(_V) (int)(ptrdiff_t)&(((%s *)0)_V)", num, ctype))
  ctypenum = num
end
map_op[".type_2"] = map_op[".type_3"]

-- Dump type definitions.
local function dumptypes(out, lvl)
  local t = {}
  for name in pairs(map_type) do t[#t+1] = name end
  sort(t)
  out:write("Type definitions:\n")
  for _,name in ipairs(t) do
    local tp = map_type[name]
    local reg = tp.reg or ""
    out:write(format("  %-20s %-20s %s\n", name, tp.ctype, reg))
  end
  out:write("\n")
end

------------------------------------------------------------------------------

-- Set the current section.
function _M.section(num)
  waction("SECTION", num)
  wflush(true) -- SECTION is a terminal action.
end

------------------------------------------------------------------------------

-- Dump architecture description.
function _M.dumparch(out)
  out:write(format("DynASM %s version %s, released %s\n\n",
    _info.arch, _info.version, _info.release))
  dumpactions(out)
end

-- Dump all user defined elements.
function _M.dumpdef(out, lvl)
  dumptypes(out, lvl)
  dumpglobals(out, lvl)
  dumpexterns(out, lvl)
end

------------------------------------------------------------------------------

-- Pass callbacks from/to the DynASM core.
function _M.passcb(wl, we, wf, ww)
  wline, werror, wfatal, wwarn = wl, we, wf, ww
  return wflush
end

-- Setup the arch-specific module.
function _M.setup(arch, opt)
  g_arch, g_opt = arch, opt
end

-- Merge the core maps and the arch-specific maps.
function _M.mergemaps(map_coreop, map_def)
  setmetatable(map_op, { __index = map_coreop })
  setmetatable(map_def, { __index = map_archdef })
  return map_op, map_def
end

return _M

------------------------------------------------------------------------------

