------------------------------------------------------------------------------
-- DynASM PPC module.
--
-- Copyright (C) 2005-2013 Mike Pall. All rights reserved.
-- See dynasm.lua for full copyright notice.
------------------------------------------------------------------------------

-- Module information:
local _info = {
  arch =	"ppc",
  description =	"DynASM PPC module",
  version =	"1.3.0",
  vernum =	 10300,
  release =	"2011-05-05",
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
local band, shl, shr, sar = bit.band, bit.lshift, bit.rshift, bit.arshift
local tohex = bit.tohex

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
  wputxw(w * 0x10000 + (val or 0))
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
  if n <= 0xffffff then waction("ESC") end
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
local map_archdef = { sp = "r1" } -- Ext. register name -> int. name.

local map_type = {}		-- Type name -> { ctype, reg }
local ctypenum = 0		-- Type number (for Dt... macros).

-- Reverse defines for registers.
function _M.revdef(s)
  if s == "r1" then return "sp" end
  return s
end

local map_cond = {
  lt = 0, gt = 1, eq = 2, so = 3,
  ge = 4, le = 5, ne = 6, ns = 7,
}

------------------------------------------------------------------------------

-- Template strings for PPC instructions.
local map_op = {
  tdi_3 =	"08000000ARI",
  twi_3 =	"0c000000ARI",
  mulli_3 =	"1c000000RRI",
  subfic_3 =	"20000000RRI",
  cmplwi_3 =	"28000000XRU",
  cmplwi_2 =	"28000000-RU",
  cmpldi_3 =	"28200000XRU",
  cmpldi_2 =	"28200000-RU",
  cmpwi_3 =	"2c000000XRI",
  cmpwi_2 =	"2c000000-RI",
  cmpdi_3 =	"2c200000XRI",
  cmpdi_2 =	"2c200000-RI",
  addic_3 =	"30000000RRI",
  ["addic._3"] = "34000000RRI",
  addi_3 =	"38000000RR0I",
  li_2 =	"38000000RI",
  la_2 =	"38000000RD",
  addis_3 =	"3c000000RR0I",
  lis_2 =	"3c000000RI",
  lus_2 =	"3c000000RU",
  bc_3 =	"40000000AAK",
  bcl_3 =	"40000001AAK",
  bdnz_1 =	"42000000K",
  bdz_1 =	"42400000K",
  sc_0 =	"44000000",
  b_1 =		"48000000J",
  bl_1 =	"48000001J",
  rlwimi_5 =	"50000000RR~AAA.",
  rlwinm_5 =	"54000000RR~AAA.",
  rlwnm_5 =	"5c000000RR~RAA.",
  ori_3 =	"60000000RR~U",
  nop_0 =	"60000000",
  oris_3 =	"64000000RR~U",
  xori_3 =	"68000000RR~U",
  xoris_3 =	"6c000000RR~U",
  ["andi._3"] =	"70000000RR~U",
  ["andis._3"] = "74000000RR~U",
  lwz_2 =	"80000000RD",
  lwzu_2 =	"84000000RD",
  lbz_2 =	"88000000RD",
  lbzu_2 =	"8c000000RD",
  stw_2 =	"90000000RD",
  stwu_2 =	"94000000RD",
  stb_2 =	"98000000RD",
  stbu_2 =	"9c000000RD",
  lhz_2 =	"a0000000RD",
  lhzu_2 =	"a4000000RD",
  lha_2 =	"a8000000RD",
  lhau_2 =	"ac000000RD",
  sth_2 =	"b0000000RD",
  sthu_2 =	"b4000000RD",
  lmw_2 =	"b8000000RD",
  stmw_2 =	"bc000000RD",
  lfs_2 =	"c0000000FD",
  lfsu_2 =	"c4000000FD",
  lfd_2 =	"c8000000FD",
  lfdu_2 =	"cc000000FD",
  stfs_2 =	"d0000000FD",
  stfsu_2 =	"d4000000FD",
  stfd_2 =	"d8000000FD",
  stfdu_2 =	"dc000000FD",
  ld_2 =	"e8000000RD", -- NYI: displacement must be divisible by 4.
  ldu_2 =	"e8000001RD",
  lwa_2 =	"e8000002RD",
  std_2 =	"f8000000RD",
  stdu_2 =	"f8000001RD",

  -- Primary opcode 19:
  mcrf_2 =	"4c000000XX",
  isync_0 =	"4c00012c",
  crnor_3 =	"4c000042CCC",
  crnot_2 =	"4c000042CC=",
  crandc_3 =	"4c000102CCC",
  crxor_3 =	"4c000182CCC",
  crclr_1 =	"4c000182C==",
  crnand_3 =	"4c0001c2CCC",
  crand_3 =	"4c000202CCC",
  creqv_3 =	"4c000242CCC",
  crset_1 =	"4c000242C==",
  crorc_3 =	"4c000342CCC",
  cror_3 =	"4c000382CCC",
  crmove_2 =	"4c000382CC=",
  bclr_2 =	"4c000020AA",
  bclrl_2 =	"4c000021AA",
  bcctr_2 =	"4c000420AA",
  bcctrl_2 =	"4c000421AA",
  blr_0 =	"4e800020",
  blrl_0 =	"4e800021",
  bctr_0 =	"4e800420",
  bctrl_0 =	"4e800421",

  -- Primary opcode 31:
  cmpw_3 =	"7c000000XRR",
  cmpw_2 =	"7c000000-RR",
  cmpd_3 =	"7c200000XRR",
  cmpd_2 =	"7c200000-RR",
  tw_3 =	"7c000008ARR",
  subfc_3 =	"7c000010RRR.",
  subc_3 =	"7c000010RRR~.",
  mulhdu_3 =	"7c000012RRR.",
  addc_3 =	"7c000014RRR.",
  mulhwu_3 =	"7c000016RRR.",
  isel_4 =	"7c00001eRRRC",
  isellt_3 =	"7c00001eRRR",
  iselgt_3 =	"7c00005eRRR",
  iseleq_3 =	"7c00009eRRR",
  mfcr_1 =	"7c000026R",
  mfocrf_2 =	"7c100026RG",
  mtcrf_2 =	"7c000120GR",
  mtocrf_2 =	"7c100120GR",
  lwarx_3 =	"7c000028RR0R",
  ldx_3 =	"7c00002aRR0R",
  lwzx_3 =	"7c00002eRR0R",
  slw_3 =	"7c000030RR~R.",
  cntlzw_2 =	"7c000034RR~",
  sld_3 =	"7c000036RR~R.",
  and_3 =	"7c000038RR~R.",
  cmplw_3 =	"7c000040XRR",
  cmplw_2 =	"7c000040-RR",
  cmpld_3 =	"7c200040XRR",
  cmpld_2 =	"7c200040-RR",
  subf_3 =	"7c000050RRR.",
  sub_3 =	"7c000050RRR~.",
  ldux_3 =	"7c00006aRR0R",
  dcbst_2 =	"7c00006c-RR",
  lwzux_3 =	"7c00006eRR0R",
  cntlzd_2 =	"7c000074RR~",
  andc_3 =	"7c000078RR~R.",
  td_3 =	"7c000088ARR",
  mulhd_3 =	"7c000092RRR.",
  mulhw_3 =	"7c000096RRR.",
  ldarx_3 =	"7c0000a8RR0R",
  dcbf_2 =	"7c0000ac-RR",
  lbzx_3 =	"7c0000aeRR0R",
  neg_2 =	"7c0000d0RR.",
  lbzux_3 =	"7c0000eeRR0R",
  popcntb_2 =	"7c0000f4RR~",
  not_2 =	"7c0000f8RR~%.",
  nor_3 =	"7c0000f8RR~R.",
  subfe_3 =	"7c000110RRR.",
  sube_3 =	"7c000110RRR~.",
  adde_3 =	"7c000114RRR.",
  stdx_3 =	"7c00012aRR0R",
  stwcx_3 =	"7c00012cRR0R.",
  stwx_3 =	"7c00012eRR0R",
  prtyw_2 =	"7c000134RR~",
  stdux_3 =	"7c00016aRR0R",
  stwux_3 =	"7c00016eRR0R",
  prtyd_2 =	"7c000174RR~",
  subfze_2 =	"7c000190RR.",
  addze_2 =	"7c000194RR.",
  stdcx_3 =	"7c0001acRR0R.",
  stbx_3 =	"7c0001aeRR0R",
  subfme_2 =	"7c0001d0RR.",
  mulld_3 =	"7c0001d2RRR.",
  addme_2 =	"7c0001d4RR.",
  mullw_3 =	"7c0001d6RRR.",
  dcbtst_2 =	"7c0001ec-RR",
  stbux_3 =	"7c0001eeRR0R",
  add_3 =	"7c000214RRR.",
  dcbt_2 =	"7c00022c-RR",
  lhzx_3 =	"7c00022eRR0R",
  eqv_3 =	"7c000238RR~R.",
  eciwx_3 =	"7c00026cRR0R",
  lhzux_3 =	"7c00026eRR0R",
  xor_3 =	"7c000278RR~R.",
  mfspefscr_1 =	"7c0082a6R",
  mfxer_1 =	"7c0102a6R",
  mflr_1 =	"7c0802a6R",
  mfctr_1 =	"7c0902a6R",
  lwax_3 =	"7c0002aaRR0R",
  lhax_3 =	"7c0002aeRR0R",
  mftb_1 =	"7c0c42e6R",
  mftbu_1 =	"7c0d42e6R",
  lwaux_3 =	"7c0002eaRR0R",
  lhaux_3 =	"7c0002eeRR0R",
  sthx_3 =	"7c00032eRR0R",
  orc_3 =	"7c000338RR~R.",
  ecowx_3 =	"7c00036cRR0R",
  sthux_3 =	"7c00036eRR0R",
  or_3 =	"7c000378RR~R.",
  mr_2 =	"7c000378RR~%.",
  divdu_3 =	"7c000392RRR.",
  divwu_3 =	"7c000396RRR.",
  mtspefscr_1 =	"7c0083a6R",
  mtxer_1 =	"7c0103a6R",
  mtlr_1 =	"7c0803a6R",
  mtctr_1 =	"7c0903a6R",
  dcbi_2 =	"7c0003ac-RR",
  nand_3 =	"7c0003b8RR~R.",
  divd_3 =	"7c0003d2RRR.",
  divw_3 =	"7c0003d6RRR.",
  cmpb_3 =	"7c0003f8RR~R.",
  mcrxr_1 =	"7c000400X",
  subfco_3 =	"7c000410RRR.",
  subco_3 =	"7c000410RRR~.",
  addco_3 =	"7c000414RRR.",
  ldbrx_3 =	"7c000428RR0R",
  lswx_3 =	"7c00042aRR0R",
  lwbrx_3 =	"7c00042cRR0R",
  lfsx_3 =	"7c00042eFR0R",
  srw_3 =	"7c000430RR~R.",
  srd_3 =	"7c000436RR~R.",
  subfo_3 =	"7c000450RRR.",
  subo_3 =	"7c000450RRR~.",
  lfsux_3 =	"7c00046eFR0R",
  lswi_3 =	"7c0004aaRR0A",
  sync_0 =	"7c0004ac",
  lwsync_0 =	"7c2004ac",
  ptesync_0 =	"7c4004ac",
  lfdx_3 =	"7c0004aeFR0R",
  nego_2 =	"7c0004d0RR.",
  lfdux_3 =	"7c0004eeFR0R",
  subfeo_3 =	"7c000510RRR.",
  subeo_3 =	"7c000510RRR~.",
  addeo_3 =	"7c000514RRR.",
  stdbrx_3 =	"7c000528RR0R",
  stswx_3 =	"7c00052aRR0R",
  stwbrx_3 =	"7c00052cRR0R",
  stfsx_3 =	"7c00052eFR0R",
  stfsux_3 =	"7c00056eFR0R",
  subfzeo_2 =	"7c000590RR.",
  addzeo_2 =	"7c000594RR.",
  stswi_3 =	"7c0005aaRR0A",
  stfdx_3 =	"7c0005aeFR0R",
  subfmeo_2 =	"7c0005d0RR.",
  mulldo_3 =	"7c0005d2RRR.",
  addmeo_2 =	"7c0005d4RR.",
  mullwo_3 =	"7c0005d6RRR.",
  dcba_2 =	"7c0005ec-RR",
  stfdux_3 =	"7c0005eeFR0R",
  addo_3 =	"7c000614RRR.",
  lhbrx_3 =	"7c00062cRR0R",
  sraw_3 =	"7c000630RR~R.",
  srad_3 =	"7c000634RR~R.",
  srawi_3 =	"7c000670RR~A.",
  sradi_3 =	"7c000674RR~H.",
  eieio_0 =	"7c0006ac",
  lfiwax_3 =	"7c0006aeFR0R",
  sthbrx_3 =	"7c00072cRR0R",
  extsh_2 =	"7c000734RR~.",
  extsb_2 =	"7c000774RR~.",
  divduo_3 =	"7c000792RRR.",
  divwou_3 =	"7c000796RRR.",
  icbi_2 =	"7c0007ac-RR",
  stfiwx_3 =	"7c0007aeFR0R",
  extsw_2 =	"7c0007b4RR~.",
  divdo_3 =	"7c0007d2RRR.",
  divwo_3 =	"7c0007d6RRR.",
  dcbz_2 =	"7c0007ec-RR",

  -- Primary opcode 30:
  rldicl_4 =	"78000000RR~HM.",
  rldicr_4 =	"78000004RR~HM.",
  rldic_4 =	"78000008RR~HM.",
  rldimi_4 =	"7800000cRR~HM.",
  rldcl_4 =	"78000010RR~RM.",
  rldcr_4 =	"78000012RR~RM.",

  -- Primary opcode 59:
  fdivs_3 =	"ec000024FFF.",
  fsubs_3 =	"ec000028FFF.",
  fadds_3 =	"ec00002aFFF.",
  fsqrts_2 =	"ec00002cF-F.",
  fres_2 =	"ec000030F-F.",
  fmuls_3 =	"ec000032FF-F.",
  frsqrtes_2 =	"ec000034F-F.",
  fmsubs_4 =	"ec000038FFFF~.",
  fmadds_4 =	"ec00003aFFFF~.",
  fnmsubs_4 =	"ec00003cFFFF~.",
  fnmadds_4 =	"ec00003eFFFF~.",

  -- Primary opcode 63:
  fdiv_3 =	"fc000024FFF.",
  fsub_3 =	"fc000028FFF.",
  fadd_3 =	"fc00002aFFF.",
  fsqrt_2 =	"fc00002cF-F.",
  fsel_4 =	"fc00002eFFFF~.",
  fre_2 =	"fc000030F-F.",
  fmul_3 =	"fc000032FF-F.",
  frsqrte_2 =	"fc000034F-F.",
  fmsub_4 =	"fc000038FFFF~.",
  fmadd_4 =	"fc00003aFFFF~.",
  fnmsub_4 =	"fc00003cFFFF~.",
  fnmadd_4 =	"fc00003eFFFF~.",
  fcmpu_3 =	"fc000000XFF",
  fcpsgn_3 =	"fc000010FFF.",
  fcmpo_3 =	"fc000040XFF",
  mtfsb1_1 =	"fc00004cA",
  fneg_2 =	"fc000050F-F.",
  mcrfs_2 =	"fc000080XX",
  mtfsb0_1 =	"fc00008cA",
  fmr_2 =	"fc000090F-F.",
  frsp_2 =	"fc000018F-F.",
  fctiw_2 =	"fc00001cF-F.",
  fctiwz_2 =	"fc00001eF-F.",
  mtfsfi_2 =	"fc00010cAA", -- NYI: upshift.
  fnabs_2 =	"fc000110F-F.",
  fabs_2 =	"fc000210F-F.",
  frin_2 =	"fc000310F-F.",
  friz_2 =	"fc000350F-F.",
  frip_2 =	"fc000390F-F.",
  frim_2 =	"fc0003d0F-F.",
  mffs_1 =	"fc00048eF.",
  -- NYI: mtfsf, mtfsb0, mtfsb1.
  fctid_2 =	"fc00065cF-F.",
  fctidz_2 =	"fc00065eF-F.",
  fcfid_2 =	"fc00069cF-F.",

  -- Primary opcode 4, SPE APU extension:
  evaddw_3 =		"10000200RRR",
  evaddiw_3 =		"10000202RAR~",
  evsubw_3 =		"10000204RRR~",
  evsubiw_3 =		"10000206RAR~",
  evabs_2 =		"10000208RR",
  evneg_2 =		"10000209RR",
  evextsb_2 =		"1000020aRR",
  evextsh_2 =		"1000020bRR",
  evrndw_2 =		"1000020cRR",
  evcntlzw_2 =		"1000020dRR",
  evcntlsw_2 =		"1000020eRR",
  brinc_3 =		"1000020fRRR",
  evand_3 =		"10000211RRR",
  evandc_3 =		"10000212RRR",
  evxor_3 =		"10000216RRR",
  evor_3 =		"10000217RRR",
  evmr_2 =		"10000217RR=",
  evnor_3 =		"10000218RRR",
  evnot_2 =		"10000218RR=",
  eveqv_3 =		"10000219RRR",
  evorc_3 =		"1000021bRRR",
  evnand_3 =		"1000021eRRR",
  evsrwu_3 =		"10000220RRR",
  evsrws_3 =		"10000221RRR",
  evsrwiu_3 =		"10000222RRA",
  evsrwis_3 =		"10000223RRA",
  evslw_3 =		"10000224RRR",
  evslwi_3 =		"10000226RRA",
  evrlw_3 =		"10000228RRR",
  evsplati_2 =		"10000229RS",
  evrlwi_3 =		"1000022aRRA",
  evsplatfi_2 =		"1000022bRS",
  evmergehi_3 =		"1000022cRRR",
  evmergelo_3 =		"1000022dRRR",
  evcmpgtu_3 =		"10000230XRR",
  evcmpgtu_2 =		"10000230-RR",
  evcmpgts_3 =		"10000231XRR",
  evcmpgts_2 =		"10000231-RR",
  evcmpltu_3 =		"10000232XRR",
  evcmpltu_2 =		"10000232-RR",
  evcmplts_3 =		"10000233XRR",
  evcmplts_2 =		"10000233-RR",
  evcmpeq_3 =		"10000234XRR",
  evcmpeq_2 =		"10000234-RR",
  evsel_4 =		"10000278RRRW",
  evsel_3 =		"10000278RRR",
  evfsadd_3 =		"10000280RRR",
  evfssub_3 =		"10000281RRR",
  evfsabs_2 =		"10000284RR",
  evfsnabs_2 =		"10000285RR",
  evfsneg_2 =		"10000286RR",
  evfsmul_3 =		"10000288RRR",
  evfsdiv_3 =		"10000289RRR",
  evfscmpgt_3 =		"1000028cXRR",
  evfscmpgt_2 =		"1000028c-RR",
  evfscmplt_3 =		"1000028dXRR",
  evfscmplt_2 =		"1000028d-RR",
  evfscmpeq_3 =		"1000028eXRR",
  evfscmpeq_2 =		"1000028e-RR",
  evfscfui_2 =		"10000290R-R",
  evfscfsi_2 =		"10000291R-R",
  evfscfuf_2 =		"10000292R-R",
  evfscfsf_2 =		"10000293R-R",
  evfsctui_2 =		"10000294R-R",
  evfsctsi_2 =		"10000295R-R",
  evfsctuf_2 =		"10000296R-R",
  evfsctsf_2 =		"10000297R-R",
  evfsctuiz_2 =		"10000298R-R",
  evfsctsiz_2 =		"1000029aR-R",
  evfststgt_3 =		"1000029cXRR",
  evfststgt_2 =		"1000029c-RR",
  evfststlt_3 =		"1000029dXRR",
  evfststlt_2 =		"1000029d-RR",
  evfststeq_3 =		"1000029eXRR",
  evfststeq_2 =		"1000029e-RR",
  efsadd_3 =		"100002c0RRR",
  efssub_3 =		"100002c1RRR",
  efsabs_2 =		"100002c4RR",
  efsnabs_2 =		"100002c5RR",
  efsneg_2 =		"100002c6RR",
  efsmul_3 =		"100002c8RRR",
  efsdiv_3 =		"100002c9RRR",
  efscmpgt_3 =		"100002ccXRR",
  efscmpgt_2 =		"100002cc-RR",
  efscmplt_3 =		"100002cdXRR",
  efscmplt_2 =		"100002cd-RR",
  efscmpeq_3 =		"100002ceXRR",
  efscmpeq_2 =		"100002ce-RR",
  efscfd_2 =		"100002cfR-R",
  efscfui_2 =		"100002d0R-R",
  efscfsi_2 =		"100002d1R-R",
  efscfuf_2 =		"100002d2R-R",
  efscfsf_2 =		"100002d3R-R",
  efsctui_2 =		"100002d4R-R",
  efsctsi_2 =		"100002d5R-R",
  efsctuf_2 =		"100002d6R-R",
  efsctsf_2 =		"100002d7R-R",
  efsctuiz_2 =		"100002d8R-R",
  efsctsiz_2 =		"100002daR-R",
  efststgt_3 =		"100002dcXRR",
  efststgt_2 =		"100002dc-RR",
  efststlt_3 =		"100002ddXRR",
  efststlt_2 =		"100002dd-RR",
  efststeq_3 =		"100002deXRR",
  efststeq_2 =		"100002de-RR",
  efdadd_3 =		"100002e0RRR",
  efdsub_3 =		"100002e1RRR",
  efdcfuid_2 =		"100002e2R-R",
  efdcfsid_2 =		"100002e3R-R",
  efdabs_2 =		"100002e4RR",
  efdnabs_2 =		"100002e5RR",
  efdneg_2 =		"100002e6RR",
  efdmul_3 =		"100002e8RRR",
  efddiv_3 =		"100002e9RRR",
  efdctuidz_2 =		"100002eaR-R",
  efdctsidz_2 =		"100002ebR-R",
  efdcmpgt_3 =		"100002ecXRR",
  efdcmpgt_2 =		"100002ec-RR",
  efdcmplt_3 =		"100002edXRR",
  efdcmplt_2 =		"100002ed-RR",
  efdcmpeq_3 =		"100002eeXRR",
  efdcmpeq_2 =		"100002ee-RR",
  efdcfs_2 =		"100002efR-R",
  efdcfui_2 =		"100002f0R-R",
  efdcfsi_2 =		"100002f1R-R",
  efdcfuf_2 =		"100002f2R-R",
  efdcfsf_2 =		"100002f3R-R",
  efdctui_2 =		"100002f4R-R",
  efdctsi_2 =		"100002f5R-R",
  efdctuf_2 =		"100002f6R-R",
  efdctsf_2 =		"100002f7R-R",
  efdctuiz_2 =		"100002f8R-R",
  efdctsiz_2 =		"100002faR-R",
  efdtstgt_3 =		"100002fcXRR",
  efdtstgt_2 =		"100002fc-RR",
  efdtstlt_3 =		"100002fdXRR",
  efdtstlt_2 =		"100002fd-RR",
  efdtsteq_3 =		"100002feXRR",
  efdtsteq_2 =		"100002fe-RR",
  evlddx_3 =		"10000300RR0R",
  evldd_2 =		"10000301R8",
  evldwx_3 =		"10000302RR0R",
  evldw_2 =		"10000303R8",
  evldhx_3 =		"10000304RR0R",
  evldh_2 =		"10000305R8",
  evlwhex_3 =		"10000310RR0R",
  evlwhe_2 =		"10000311R4",
  evlwhoux_3 =		"10000314RR0R",
  evlwhou_2 =		"10000315R4",
  evlwhosx_3 =		"10000316RR0R",
  evlwhos_2 =		"10000317R4",
  evstddx_3 =		"10000320RR0R",
  evstdd_2 =		"10000321R8",
  evstdwx_3 =		"10000322RR0R",
  evstdw_2 =		"10000323R8",
  evstdhx_3 =		"10000324RR0R",
  evstdh_2 =		"10000325R8",
  evstwhex_3 =		"10000330RR0R",
  evstwhe_2 =		"10000331R4",
  evstwhox_3 =		"10000334RR0R",
  evstwho_2 =		"10000335R4",
  evstwwex_3 =		"10000338RR0R",
  evstwwe_2 =		"10000339R4",
  evstwwox_3 =		"1000033cRR0R",
  evstwwo_2 =		"1000033dR4",
  evmhessf_3 =		"10000403RRR",
  evmhossf_3 =		"10000407RRR",
  evmheumi_3 =		"10000408RRR",
  evmhesmi_3 =		"10000409RRR",
  evmhesmf_3 =		"1000040bRRR",
  evmhoumi_3 =		"1000040cRRR",
  evmhosmi_3 =		"1000040dRRR",
  evmhosmf_3 =		"1000040fRRR",
  evmhessfa_3 =		"10000423RRR",
  evmhossfa_3 =		"10000427RRR",
  evmheumia_3 =		"10000428RRR",
  evmhesmia_3 =		"10000429RRR",
  evmhesmfa_3 =		"1000042bRRR",
  evmhoumia_3 =		"1000042cRRR",
  evmhosmia_3 =		"1000042dRRR",
  evmhosmfa_3 =		"1000042fRRR",
  evmwhssf_3 =		"10000447RRR",
  evmwlumi_3 =		"10000448RRR",
  evmwhumi_3 =		"1000044cRRR",
  evmwhsmi_3 =		"1000044dRRR",
  evmwhsmf_3 =		"1000044fRRR",
  evmwssf_3 =		"10000453RRR",
  evmwumi_3 =		"10000458RRR",
  evmwsmi_3 =		"10000459RRR",
  evmwsmf_3 =		"1000045bRRR",
  evmwhssfa_3 =		"10000467RRR",
  evmwlumia_3 =		"10000468RRR",
  evmwhumia_3 =		"1000046cRRR",
  evmwhsmia_3 =		"1000046dRRR",
  evmwhsmfa_3 =		"1000046fRRR",
  evmwssfa_3 =		"10000473RRR",
  evmwumia_3 =		"10000478RRR",
  evmwsmia_3 =		"10000479RRR",
  evmwsmfa_3 =		"1000047bRRR",
  evmra_2 =		"100004c4RR",
  evdivws_3 =		"100004c6RRR",
  evdivwu_3 =		"100004c7RRR",
  evmwssfaa_3 =		"10000553RRR",
  evmwumiaa_3 =		"10000558RRR",
  evmwsmiaa_3 =		"10000559RRR",
  evmwsmfaa_3 =		"1000055bRRR",
  evmwssfan_3 =		"100005d3RRR",
  evmwumian_3 =		"100005d8RRR",
  evmwsmian_3 =		"100005d9RRR",
  evmwsmfan_3 =		"100005dbRRR",
  evmergehilo_3 =	"1000022eRRR",
  evmergelohi_3 =	"1000022fRRR",
  evlhhesplatx_3 =	"10000308RR0R",
  evlhhesplat_2 =	"10000309R2",
  evlhhousplatx_3 =	"1000030cRR0R",
  evlhhousplat_2 =	"1000030dR2",
  evlhhossplatx_3 =	"1000030eRR0R",
  evlhhossplat_2 =	"1000030fR2",
  evlwwsplatx_3 =	"10000318RR0R",
  evlwwsplat_2 =	"10000319R4",
  evlwhsplatx_3 =	"1000031cRR0R",
  evlwhsplat_2 =	"1000031dR4",
  evaddusiaaw_2 =	"100004c0RR",
  evaddssiaaw_2 =	"100004c1RR",
  evsubfusiaaw_2 =	"100004c2RR",
  evsubfssiaaw_2 =	"100004c3RR",
  evaddumiaaw_2 =	"100004c8RR",
  evaddsmiaaw_2 =	"100004c9RR",
  evsubfumiaaw_2 =	"100004caRR",
  evsubfsmiaaw_2 =	"100004cbRR",
  evmheusiaaw_3 =	"10000500RRR",
  evmhessiaaw_3 =	"10000501RRR",
  evmhessfaaw_3 =	"10000503RRR",
  evmhousiaaw_3 =	"10000504RRR",
  evmhossiaaw_3 =	"10000505RRR",
  evmhossfaaw_3 =	"10000507RRR",
  evmheumiaaw_3 =	"10000508RRR",
  evmhesmiaaw_3 =	"10000509RRR",
  evmhesmfaaw_3 =	"1000050bRRR",
  evmhoumiaaw_3 =	"1000050cRRR",
  evmhosmiaaw_3 =	"1000050dRRR",
  evmhosmfaaw_3 =	"1000050fRRR",
  evmhegumiaa_3 =	"10000528RRR",
  evmhegsmiaa_3 =	"10000529RRR",
  evmhegsmfaa_3 =	"1000052bRRR",
  evmhogumiaa_3 =	"1000052cRRR",
  evmhogsmiaa_3 =	"1000052dRRR",
  evmhogsmfaa_3 =	"1000052fRRR",
  evmwlusiaaw_3 =	"10000540RRR",
  evmwlssiaaw_3 =	"10000541RRR",
  evmwlumiaaw_3 =	"10000548RRR",
  evmwlsmiaaw_3 =	"10000549RRR",
  evmheusianw_3 =	"10000580RRR",
  evmhessianw_3 =	"10000581RRR",
  evmhessfanw_3 =	"10000583RRR",
  evmhousianw_3 =	"10000584RRR",
  evmhossianw_3 =	"10000585RRR",
  evmhossfanw_3 =	"10000587RRR",
  evmheumianw_3 =	"10000588RRR",
  evmhesmianw_3 =	"10000589RRR",
  evmhesmfanw_3 =	"1000058bRRR",
  evmhoumianw_3 =	"1000058cRRR",
  evmhosmianw_3 =	"1000058dRRR",
  evmhosmfanw_3 =	"1000058fRRR",
  evmhegumian_3 =	"100005a8RRR",
  evmhegsmian_3 =	"100005a9RRR",
  evmhegsmfan_3 =	"100005abRRR",
  evmhogumian_3 =	"100005acRRR",
  evmhogsmian_3 =	"100005adRRR",
  evmhogsmfan_3 =	"100005afRRR",
  evmwlusianw_3 =	"100005c0RRR",
  evmwlssianw_3 =	"100005c1RRR",
  evmwlumianw_3 =	"100005c8RRR",
  evmwlsmianw_3 =	"100005c9RRR",

  -- NYI: Book E instructions.
}

-- Add mnemonics for "." variants.
do
  local t = {}
  for k,v in pairs(map_op) do
    if sub(v, -1) == "." then
      local v2 = sub(v, 1, 7)..char(byte(v, 8)+1)..sub(v, 9, -2)
      t[sub(k, 1, -3).."."..sub(k, -2)] = v2
    end
  end
  for k,v in pairs(t) do
    map_op[k] = v
  end
end

-- Add more branch mnemonics.
for cond,c in pairs(map_cond) do
  local b1 = "b"..cond
  local c1 = shl(band(c, 3), 16) + (c < 4 and 0x01000000 or 0)
  -- bX[l]
  map_op[b1.."_1"] = tohex(0x40800000 + c1).."K"
  map_op[b1.."y_1"] = tohex(0x40a00000 + c1).."K"
  map_op[b1.."l_1"] = tohex(0x40800001 + c1).."K"
  map_op[b1.."_2"] = tohex(0x40800000 + c1).."-XK"
  map_op[b1.."y_2"] = tohex(0x40a00000 + c1).."-XK"
  map_op[b1.."l_2"] = tohex(0x40800001 + c1).."-XK"
  -- bXlr[l]
  map_op[b1.."lr_0"] = tohex(0x4c800020 + c1)
  map_op[b1.."lrl_0"] = tohex(0x4c800021 + c1)
  map_op[b1.."ctr_0"] = tohex(0x4c800420 + c1)
  map_op[b1.."ctrl_0"] = tohex(0x4c800421 + c1)
  -- bXctr[l]
  map_op[b1.."lr_1"] = tohex(0x4c800020 + c1).."-X"
  map_op[b1.."lrl_1"] = tohex(0x4c800021 + c1).."-X"
  map_op[b1.."ctr_1"] = tohex(0x4c800420 + c1).."-X"
  map_op[b1.."ctrl_1"] = tohex(0x4c800421 + c1).."-X"
end

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

local function parse_cr(expr)
  local r = match(expr, "^cr([0-7])$")
  if r then return tonumber(r) end
  werror("bad condition register name `"..expr.."'")
end

local function parse_cond(expr)
  local r, cond = match(expr, "^4%*cr([0-7])%+(%w%w)$")
  if r then
    r = tonumber(r)
    local c = map_cond[cond]
    if c and c < 4 then return r*4+c end
  end
  werror("bad condition bit name `"..expr.."'")
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
  elseif match(imm, "^r([1-3]?[0-9])$") or
	 match(imm, "^([%w_]+):(r[1-3]?[0-9])$") then
    werror("expected immediate operand, got register")
  else
    waction("IMM", (signed and 32768 or 0)+scale*1024+bits*32+shift, imm)
    return 0
  end
end

local function parse_shiftmask(imm, isshift)
  local n = tonumber(imm)
  if n then
    if shr(n, 6) == 0 then
      local lsb = band(imm, 31)
      local msb = imm - lsb
      return isshift and (shl(lsb, 11)+shr(msb, 4)) or (shl(lsb, 6)+msb)
    end
    werror("out of range immediate `"..imm.."'")
  elseif match(imm, "^r([1-3]?[0-9])$") or
	 match(imm, "^([%w_]+):(r[1-3]?[0-9])$") then
    werror("expected immediate operand, got register")
  else
    werror("NYI: parameterized 64 bit shift/mask")
  end
end

local function parse_disp(disp)
  local imm, reg = match(disp, "^(.*)%(([%w_:]+)%)$")
  if imm then
    local r = parse_gpr(reg)
    if r == 0 then werror("cannot use r0 in displacement") end
    return shl(r, 16) + parse_imm(imm, 16, 0, 0, true)
  end
  local reg, tailr = match(disp, "^([%w_:]+)%s*(.*)$")
  if reg and tailr ~= "" then
    local r, tp = parse_gpr(reg)
    if r == 0 then werror("cannot use r0 in displacement") end
    if tp then
      waction("IMM", 32768+16*32, format(tp.ctypefmt, tailr))
      return shl(r, 16)
    end
  end
  werror("bad displacement `"..disp.."'")
end

local function parse_u5disp(disp, scale)
  local imm, reg = match(disp, "^(.*)%(([%w_:]+)%)$")
  if imm then
    local r = parse_gpr(reg)
    if r == 0 then werror("cannot use r0 in displacement") end
    return shl(r, 16) + parse_imm(imm, 5, 11, scale, false)
  end
  local reg, tailr = match(disp, "^([%w_:]+)%s*(.*)$")
  if reg and tailr ~= "" then
    local r, tp = parse_gpr(reg)
    if r == 0 then werror("cannot use r0 in displacement") end
    if tp then
      waction("IMM", scale*1024+5*32+11, format(tp.ctypefmt, tailr))
      return shl(r, 16)
    end
  end
  werror("bad displacement `"..disp.."'")
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
  local n, rs = 1, 26

  -- Limit number of section buffer positions used by a single dasm_put().
  -- A single opcode needs a maximum of 3 positions (rlwinm).
  if secpos+3 > maxsecpos then wflush() end
  local pos = wpos()

  -- Process each character.
  for p in gmatch(sub(template, 9), ".") do
    if p == "R" then
      rs = rs - 5; op = op + shl(parse_gpr(params[n]), rs); n = n + 1
    elseif p == "F" then
      rs = rs - 5; op = op + shl(parse_fpr(params[n]), rs); n = n + 1
    elseif p == "A" then
      rs = rs - 5; op = op + parse_imm(params[n], 5, rs, 0, false); n = n + 1
    elseif p == "S" then
      rs = rs - 5; op = op + parse_imm(params[n], 5, rs, 0, true); n = n + 1
    elseif p == "I" then
      op = op + parse_imm(params[n], 16, 0, 0, true); n = n + 1
    elseif p == "U" then
      op = op + parse_imm(params[n], 16, 0, 0, false); n = n + 1
    elseif p == "D" then
      op = op + parse_disp(params[n]); n = n + 1
    elseif p == "2" then
      op = op + parse_u5disp(params[n], 1); n = n + 1
    elseif p == "4" then
      op = op + parse_u5disp(params[n], 2); n = n + 1
    elseif p == "8" then
      op = op + parse_u5disp(params[n], 3); n = n + 1
    elseif p == "C" then
      rs = rs - 5; op = op + shl(parse_cond(params[n]), rs); n = n + 1
    elseif p == "X" then
      rs = rs - 5; op = op + shl(parse_cr(params[n]), rs+2); n = n + 1
    elseif p == "W" then
      op = op + parse_cr(params[n]); n = n + 1
    elseif p == "G" then
      op = op + parse_imm(params[n], 8, 12, 0, false); n = n + 1
    elseif p == "H" then
      op = op + parse_shiftmask(params[n], true); n = n + 1
    elseif p == "M" then
      op = op + parse_shiftmask(params[n], false); n = n + 1
    elseif p == "J" or p == "K" then
      local mode, n, s = parse_label(params[n], false)
      if p == "K" then n = n + 2048 end
      waction("REL_"..mode, n, s, 1)
      n = n + 1
    elseif p == "0" then
      if band(shr(op, rs), 31) == 0 then werror("cannot use r0") end
    elseif p == "=" or p == "%" then
      local t = band(shr(op, p == "%" and rs+5 or rs), 31)
      rs = rs - 5
      op = op + shl(t, rs)
    elseif p == "~" then
      local mm = shl(31, rs)
      local lo = band(op, mm)
      local hi = band(op, shl(mm, 5))
      op = op - lo - hi + shl(lo, 5) + shr(hi, 5)
    elseif p == "-" then
      rs = rs - 5
    elseif p == "." then
      -- Ignored.
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

