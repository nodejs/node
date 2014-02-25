----------------------------------------------------------------------------
-- LuaJIT PPC disassembler module.
--
-- Copyright (C) 2005-2013 Mike Pall. All rights reserved.
-- Released under the MIT/X license. See Copyright Notice in luajit.h
----------------------------------------------------------------------------
-- This is a helper module used by the LuaJIT machine code dumper module.
--
-- It disassembles all common, non-privileged 32/64 bit PowerPC instructions
-- plus the e500 SPE instructions and some Cell/Xenon extensions.
--
-- NYI: VMX, VMX128
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

local map_crops = {
  shift = 1, mask = 1023,
  [0] = "mcrfXX",
  [33] = "crnor|crnotCCC=", [129] = "crandcCCC",
  [193] = "crxor|crclrCCC%", [225] = "crnandCCC",
  [257] = "crandCCC", [289] = "creqv|crsetCCC%",
  [417] = "crorcCCC", [449] = "cror|crmoveCCC=",
  [16] = "b_lrKB", [528] = "b_ctrKB",
  [150] = "isync",
}

local map_rlwinm = setmetatable({
  shift = 0, mask = -1,
},
{ __index = function(t, x)
    local rot = band(rshift(x, 11), 31)
    local mb = band(rshift(x, 6), 31)
    local me = band(rshift(x, 1), 31)
    if mb == 0 and me == 31-rot then
      return "slwiRR~A."
    elseif me == 31 and mb == 32-rot then
      return "srwiRR~-A."
    else
      return "rlwinmRR~AAA."
    end
  end
})

local map_rld = {
  shift = 2, mask = 7,
  [0] = "rldiclRR~HM.", "rldicrRR~HM.", "rldicRR~HM.", "rldimiRR~HM.",
  {
    shift = 1, mask = 1,
    [0] = "rldclRR~RM.", "rldcrRR~RM.",
  },
}

local map_ext = setmetatable({
  shift = 1, mask = 1023,

  [0] = "cmp_YLRR", [32] = "cmpl_YLRR",
  [4] = "twARR", [68] = "tdARR",

  [8] = "subfcRRR.", [40] = "subfRRR.",
  [104] = "negRR.", [136] = "subfeRRR.",
  [200] = "subfzeRR.", [232] = "subfmeRR.",
  [520] = "subfcoRRR.", [552] = "subfoRRR.",
  [616] = "negoRR.", [648] = "subfeoRRR.",
  [712] = "subfzeoRR.", [744] = "subfmeoRR.",

  [9] = "mulhduRRR.", [73] = "mulhdRRR.", [233] = "mulldRRR.",
  [457] = "divduRRR.", [489] = "divdRRR.",
  [745] = "mulldoRRR.",
  [969] = "divduoRRR.", [1001] = "divdoRRR.",

  [10] = "addcRRR.", [138] = "addeRRR.",
  [202] = "addzeRR.", [234] = "addmeRR.", [266] = "addRRR.",
  [522] = "addcoRRR.", [650] = "addeoRRR.",
  [714] = "addzeoRR.", [746] = "addmeoRR.", [778] = "addoRRR.",

  [11] = "mulhwuRRR.", [75] = "mulhwRRR.", [235] = "mullwRRR.",
  [459] = "divwuRRR.", [491] = "divwRRR.",
  [747] = "mullwoRRR.",
  [971] = "divwouRRR.", [1003] = "divwoRRR.",

  [15] = "iselltRRR", [47] = "iselgtRRR", [79] = "iseleqRRR",

  [144] = { shift = 20, mask = 1, [0] = "mtcrfRZ~", "mtocrfRZ~", },
  [19] = { shift = 20, mask = 1, [0] = "mfcrR", "mfocrfRZ", },
  [371] = { shift = 11, mask = 1023, [392] = "mftbR", [424] = "mftbuR", },
  [339] = {
    shift = 11, mask = 1023,
    [32] = "mferR", [256] = "mflrR", [288] = "mfctrR", [16] = "mfspefscrR",
  },
  [467] = {
    shift = 11, mask = 1023,
    [32] = "mtxerR", [256] = "mtlrR", [288] = "mtctrR", [16] = "mtspefscrR",
  },

  [20] = "lwarxRR0R", [84] = "ldarxRR0R",

  [21] = "ldxRR0R", [53] = "lduxRRR",
  [149] = "stdxRR0R", [181] = "stduxRRR",
  [341] = "lwaxRR0R", [373] = "lwauxRRR",

  [23] = "lwzxRR0R", [55] = "lwzuxRRR",
  [87] = "lbzxRR0R", [119] = "lbzuxRRR",
  [151] = "stwxRR0R", [183] = "stwuxRRR",
  [215] = "stbxRR0R", [247] = "stbuxRRR",
  [279] = "lhzxRR0R", [311] = "lhzuxRRR",
  [343] = "lhaxRR0R", [375] = "lhauxRRR",
  [407] = "sthxRR0R", [439] = "sthuxRRR",

  [54] = "dcbst-R0R", [86] = "dcbf-R0R",
  [150] = "stwcxRR0R.", [214] = "stdcxRR0R.",
  [246] = "dcbtst-R0R", [278] = "dcbt-R0R",
  [310] = "eciwxRR0R", [438] = "ecowxRR0R",
  [470] = "dcbi-RR",

  [598] = {
    shift = 21, mask = 3,
    [0] = "sync", "lwsync", "ptesync",
  },
  [758] = "dcba-RR",
  [854] = "eieio", [982] = "icbi-R0R", [1014] = "dcbz-R0R",

  [26] = "cntlzwRR~", [58] = "cntlzdRR~",
  [122] = "popcntbRR~",
  [154] = "prtywRR~", [186] = "prtydRR~",

  [28] = "andRR~R.", [60] = "andcRR~R.", [124] = "nor|notRR~R=.",
  [284] = "eqvRR~R.", [316] = "xorRR~R.",
  [412] = "orcRR~R.", [444] = "or|mrRR~R=.", [476] = "nandRR~R.",
  [508] = "cmpbRR~R",

  [512] = "mcrxrX",

  [532] = "ldbrxRR0R", [660] = "stdbrxRR0R",

  [533] = "lswxRR0R", [597] = "lswiRR0A",
  [661] = "stswxRR0R", [725] = "stswiRR0A",

  [534] = "lwbrxRR0R", [662] = "stwbrxRR0R",
  [790] = "lhbrxRR0R", [918] = "sthbrxRR0R",

  [535] = "lfsxFR0R", [567] = "lfsuxFRR",
  [599] = "lfdxFR0R", [631] = "lfduxFRR",
  [663] = "stfsxFR0R", [695] = "stfsuxFRR",
  [727] = "stfdxFR0R", [759] = "stfduxFR0R",
  [855] = "lfiwaxFR0R",
  [983] = "stfiwxFR0R",

  [24] = "slwRR~R.",

  [27] = "sldRR~R.", [536] = "srwRR~R.",
  [792] = "srawRR~R.", [824] = "srawiRR~A.",

  [794] = "sradRR~R.", [826] = "sradiRR~H.", [827] = "sradiRR~H.",
  [922] = "extshRR~.", [954] = "extsbRR~.", [986] = "extswRR~.",

  [539] = "srdRR~R.",
},
{ __index = function(t, x)
    if band(x, 31) == 15 then return "iselRRRC" end
  end
})

local map_ld = {
  shift = 0, mask = 3,
  [0] = "ldRRE", "lduRRE", "lwaRRE",
}

local map_std = {
  shift = 0, mask = 3,
  [0] = "stdRRE", "stduRRE",
}

local map_fps = {
  shift = 5, mask = 1,
  {
    shift = 1, mask = 15,
    [0] = false, false, "fdivsFFF.", false,
    "fsubsFFF.", "faddsFFF.", "fsqrtsF-F.", false,
    "fresF-F.", "fmulsFF-F.", "frsqrtesF-F.", false,
    "fmsubsFFFF~.", "fmaddsFFFF~.", "fnmsubsFFFF~.", "fnmaddsFFFF~.",
  }
}

local map_fpd = {
  shift = 5, mask = 1,
  [0] = {
    shift = 1, mask = 1023,
    [0] = "fcmpuXFF", [32] = "fcmpoXFF", [64] = "mcrfsXX",
    [38] = "mtfsb1A.", [70] = "mtfsb0A.", [134] = "mtfsfiA>>-A>",
    [8] = "fcpsgnFFF.", [40] = "fnegF-F.", [72] = "fmrF-F.",
    [136] = "fnabsF-F.", [264] = "fabsF-F.",
    [12] = "frspF-F.",
    [14] = "fctiwF-F.", [15] = "fctiwzF-F.",
    [583] = "mffsF.", [711] = "mtfsfZF.",
    [392] = "frinF-F.", [424] = "frizF-F.",
    [456] = "fripF-F.", [488] = "frimF-F.",
    [814] = "fctidF-F.", [815] = "fctidzF-F.", [846] = "fcfidF-F.",
  },
  {
    shift = 1, mask = 15,
    [0] = false, false, "fdivFFF.", false,
    "fsubFFF.", "faddFFF.", "fsqrtF-F.", "fselFFFF~.",
    "freF-F.", "fmulFF-F.", "frsqrteF-F.", false,
    "fmsubFFFF~.", "fmaddFFFF~.", "fnmsubFFFF~.", "fnmaddFFFF~.",
  }
}

local map_spe = {
  shift = 0, mask = 2047,

  [512] = "evaddwRRR", [514] = "evaddiwRAR~",
  [516] = "evsubwRRR~", [518] = "evsubiwRAR~",
  [520] = "evabsRR", [521] = "evnegRR",
  [522] = "evextsbRR", [523] = "evextshRR", [524] = "evrndwRR",
  [525] = "evcntlzwRR", [526] = "evcntlswRR",

  [527] = "brincRRR",

  [529] = "evandRRR", [530] = "evandcRRR", [534] = "evxorRRR",
  [535] = "evor|evmrRRR=", [536] = "evnor|evnotRRR=",
  [537] = "eveqvRRR", [539] = "evorcRRR", [542] = "evnandRRR",

  [544] = "evsrwuRRR", [545] = "evsrwsRRR",
  [546] = "evsrwiuRRA", [547] = "evsrwisRRA",
  [548] = "evslwRRR", [550] = "evslwiRRA",
  [552] = "evrlwRRR", [553] = "evsplatiRS",
  [554] = "evrlwiRRA", [555] = "evsplatfiRS",
  [556] = "evmergehiRRR", [557] = "evmergeloRRR",
  [558] = "evmergehiloRRR", [559] = "evmergelohiRRR",

  [560] = "evcmpgtuYRR", [561] = "evcmpgtsYRR",
  [562] = "evcmpltuYRR", [563] = "evcmpltsYRR",
  [564] = "evcmpeqYRR",

  [632] = "evselRRR", [633] = "evselRRRW",
  [634] = "evselRRRW", [635] = "evselRRRW",
  [636] = "evselRRRW", [637] = "evselRRRW",
  [638] = "evselRRRW", [639] = "evselRRRW",

  [640] = "evfsaddRRR", [641] = "evfssubRRR",
  [644] = "evfsabsRR", [645] = "evfsnabsRR", [646] = "evfsnegRR",
  [648] = "evfsmulRRR", [649] = "evfsdivRRR",
  [652] = "evfscmpgtYRR", [653] = "evfscmpltYRR", [654] = "evfscmpeqYRR",
  [656] = "evfscfuiR-R", [657] = "evfscfsiR-R",
  [658] = "evfscfufR-R", [659] = "evfscfsfR-R",
  [660] = "evfsctuiR-R", [661] = "evfsctsiR-R",
  [662] = "evfsctufR-R", [663] = "evfsctsfR-R",
  [664] = "evfsctuizR-R", [666] = "evfsctsizR-R",
  [668] = "evfststgtYRR", [669] = "evfststltYRR", [670] = "evfststeqYRR",

  [704] = "efsaddRRR", [705] = "efssubRRR",
  [708] = "efsabsRR", [709] = "efsnabsRR", [710] = "efsnegRR",
  [712] = "efsmulRRR", [713] = "efsdivRRR",
  [716] = "efscmpgtYRR", [717] = "efscmpltYRR", [718] = "efscmpeqYRR",
  [719] = "efscfdR-R",
  [720] = "efscfuiR-R", [721] = "efscfsiR-R",
  [722] = "efscfufR-R", [723] = "efscfsfR-R",
  [724] = "efsctuiR-R", [725] = "efsctsiR-R",
  [726] = "efsctufR-R", [727] = "efsctsfR-R",
  [728] = "efsctuizR-R", [730] = "efsctsizR-R",
  [732] = "efststgtYRR", [733] = "efststltYRR", [734] = "efststeqYRR",

  [736] = "efdaddRRR", [737] = "efdsubRRR",
  [738] = "efdcfuidR-R", [739] = "efdcfsidR-R",
  [740] = "efdabsRR", [741] = "efdnabsRR", [742] = "efdnegRR",
  [744] = "efdmulRRR", [745] = "efddivRRR",
  [746] = "efdctuidzR-R", [747] = "efdctsidzR-R",
  [748] = "efdcmpgtYRR", [749] = "efdcmpltYRR", [750] = "efdcmpeqYRR",
  [751] = "efdcfsR-R",
  [752] = "efdcfuiR-R", [753] = "efdcfsiR-R",
  [754] = "efdcfufR-R", [755] = "efdcfsfR-R",
  [756] = "efdctuiR-R", [757] = "efdctsiR-R",
  [758] = "efdctufR-R", [759] = "efdctsfR-R",
  [760] = "efdctuizR-R", [762] = "efdctsizR-R",
  [764] = "efdtstgtYRR", [765] = "efdtstltYRR", [766] = "efdtsteqYRR",

  [768] = "evlddxRR0R", [769] = "evlddRR8",
  [770] = "evldwxRR0R", [771] = "evldwRR8",
  [772] = "evldhxRR0R", [773] = "evldhRR8",
  [776] = "evlhhesplatxRR0R", [777] = "evlhhesplatRR2",
  [780] = "evlhhousplatxRR0R", [781] = "evlhhousplatRR2",
  [782] = "evlhhossplatxRR0R", [783] = "evlhhossplatRR2",
  [784] = "evlwhexRR0R", [785] = "evlwheRR4",
  [788] = "evlwhouxRR0R", [789] = "evlwhouRR4",
  [790] = "evlwhosxRR0R", [791] = "evlwhosRR4",
  [792] = "evlwwsplatxRR0R", [793] = "evlwwsplatRR4",
  [796] = "evlwhsplatxRR0R", [797] = "evlwhsplatRR4",

  [800] = "evstddxRR0R", [801] = "evstddRR8",
  [802] = "evstdwxRR0R", [803] = "evstdwRR8",
  [804] = "evstdhxRR0R", [805] = "evstdhRR8",
  [816] = "evstwhexRR0R", [817] = "evstwheRR4",
  [820] = "evstwhoxRR0R", [821] = "evstwhoRR4",
  [824] = "evstwwexRR0R", [825] = "evstwweRR4",
  [828] = "evstwwoxRR0R", [829] = "evstwwoRR4",

  [1027] = "evmhessfRRR", [1031] = "evmhossfRRR", [1032] = "evmheumiRRR",
  [1033] = "evmhesmiRRR", [1035] = "evmhesmfRRR", [1036] = "evmhoumiRRR",
  [1037] = "evmhosmiRRR", [1039] = "evmhosmfRRR", [1059] = "evmhessfaRRR",
  [1063] = "evmhossfaRRR", [1064] = "evmheumiaRRR", [1065] = "evmhesmiaRRR",
  [1067] = "evmhesmfaRRR", [1068] = "evmhoumiaRRR", [1069] = "evmhosmiaRRR",
  [1071] = "evmhosmfaRRR", [1095] = "evmwhssfRRR", [1096] = "evmwlumiRRR",
  [1100] = "evmwhumiRRR", [1101] = "evmwhsmiRRR", [1103] = "evmwhsmfRRR",
  [1107] = "evmwssfRRR", [1112] = "evmwumiRRR", [1113] = "evmwsmiRRR",
  [1115] = "evmwsmfRRR", [1127] = "evmwhssfaRRR", [1128] = "evmwlumiaRRR",
  [1132] = "evmwhumiaRRR", [1133] = "evmwhsmiaRRR", [1135] = "evmwhsmfaRRR",
  [1139] = "evmwssfaRRR", [1144] = "evmwumiaRRR", [1145] = "evmwsmiaRRR",
  [1147] = "evmwsmfaRRR",

  [1216] = "evaddusiaawRR", [1217] = "evaddssiaawRR",
  [1218] = "evsubfusiaawRR", [1219] = "evsubfssiaawRR",
  [1220] = "evmraRR",
  [1222] = "evdivwsRRR", [1223] = "evdivwuRRR",
  [1224] = "evaddumiaawRR", [1225] = "evaddsmiaawRR",
  [1226] = "evsubfumiaawRR", [1227] = "evsubfsmiaawRR",

  [1280] = "evmheusiaawRRR", [1281] = "evmhessiaawRRR",
  [1283] = "evmhessfaawRRR", [1284] = "evmhousiaawRRR",
  [1285] = "evmhossiaawRRR", [1287] = "evmhossfaawRRR",
  [1288] = "evmheumiaawRRR", [1289] = "evmhesmiaawRRR",
  [1291] = "evmhesmfaawRRR", [1292] = "evmhoumiaawRRR",
  [1293] = "evmhosmiaawRRR", [1295] = "evmhosmfaawRRR",
  [1320] = "evmhegumiaaRRR", [1321] = "evmhegsmiaaRRR",
  [1323] = "evmhegsmfaaRRR", [1324] = "evmhogumiaaRRR",
  [1325] = "evmhogsmiaaRRR", [1327] = "evmhogsmfaaRRR",
  [1344] = "evmwlusiaawRRR", [1345] = "evmwlssiaawRRR",
  [1352] = "evmwlumiaawRRR", [1353] = "evmwlsmiaawRRR",
  [1363] = "evmwssfaaRRR", [1368] = "evmwumiaaRRR",
  [1369] = "evmwsmiaaRRR", [1371] = "evmwsmfaaRRR",
  [1408] = "evmheusianwRRR", [1409] = "evmhessianwRRR",
  [1411] = "evmhessfanwRRR", [1412] = "evmhousianwRRR",
  [1413] = "evmhossianwRRR", [1415] = "evmhossfanwRRR",
  [1416] = "evmheumianwRRR", [1417] = "evmhesmianwRRR",
  [1419] = "evmhesmfanwRRR", [1420] = "evmhoumianwRRR",
  [1421] = "evmhosmianwRRR", [1423] = "evmhosmfanwRRR",
  [1448] = "evmhegumianRRR", [1449] = "evmhegsmianRRR",
  [1451] = "evmhegsmfanRRR", [1452] = "evmhogumianRRR",
  [1453] = "evmhogsmianRRR", [1455] = "evmhogsmfanRRR",
  [1472] = "evmwlusianwRRR", [1473] = "evmwlssianwRRR",
  [1480] = "evmwlumianwRRR", [1481] = "evmwlsmianwRRR",
  [1491] = "evmwssfanRRR", [1496] = "evmwumianRRR",
  [1497] = "evmwsmianRRR", [1499] = "evmwsmfanRRR",
}

local map_pri = {
  [0] = false,	false,		"tdiARI",	"twiARI",
  map_spe,	false,		false,		"mulliRRI",
  "subficRRI",	false,		"cmpl_iYLRU",	"cmp_iYLRI",
  "addicRRI",	"addic.RRI",	"addi|liRR0I",	"addis|lisRR0I",
  "b_KBJ",	"sc",		 "bKJ",		map_crops,
  "rlwimiRR~AAA.", map_rlwinm,	false,		"rlwnmRR~RAA.",
  "oriNRR~U",	"orisRR~U",	"xoriRR~U",	"xorisRR~U",
  "andi.RR~U",	"andis.RR~U",	map_rld,	map_ext,
  "lwzRRD",	"lwzuRRD",	"lbzRRD",	"lbzuRRD",
  "stwRRD",	"stwuRRD",	"stbRRD",	"stbuRRD",
  "lhzRRD",	"lhzuRRD",	"lhaRRD",	"lhauRRD",
  "sthRRD",	"sthuRRD",	"lmwRRD",	"stmwRRD",
  "lfsFRD",	"lfsuFRD",	"lfdFRD",	"lfduFRD",
  "stfsFRD",	"stfsuFRD",	"stfdFRD",	"stfduFRD",
  false,	false,		map_ld,		map_fps,
  false,	false,		map_std,	map_fpd,
}

------------------------------------------------------------------------------

local map_gpr = {
  [0] = "r0", "sp", "r2", "r3", "r4", "r5", "r6", "r7",
  "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
  "r16", "r17", "r18", "r19", "r20", "r21", "r22", "r23",
  "r24", "r25", "r26", "r27", "r28", "r29", "r30", "r31",
}

local map_cond = { [0] = "lt", "gt", "eq", "so", "ge", "le", "ne", "ns", }

-- Format a condition bit.
local function condfmt(cond)
  if cond <= 3 then
    return map_cond[band(cond, 3)]
  else
    return format("4*cr%d+%s", rshift(cond, 2), map_cond[band(cond, 3)])
  end
end

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

-- Disassemble a single instruction.
local function disass_ins(ctx)
  local pos = ctx.pos
  local b0, b1, b2, b3 = byte(ctx.code, pos+1, pos+4)
  local op = bor(lshift(b0, 24), lshift(b1, 16), lshift(b2, 8), b3)
  local operands = {}
  local last = nil
  local rs = 21
  ctx.op = op
  ctx.rel = nil

  local opat = map_pri[rshift(b0, 2)]
  while type(opat) ~= "string" do
    if not opat then return unknown(ctx) end
    opat = opat[band(rshift(op, opat.shift), opat.mask)]
  end
  local name, pat = match(opat, "^([a-z0-9_.]*)(.*)")
  local altname, pat2 = match(pat, "|([a-z0-9_.]*)(.*)")
  if altname then pat = pat2 end

  for p in gmatch(pat, ".") do
    local x = nil
    if p == "R" then
      x = map_gpr[band(rshift(op, rs), 31)]
      rs = rs - 5
    elseif p == "F" then
      x = "f"..band(rshift(op, rs), 31)
      rs = rs - 5
    elseif p == "A" then
      x = band(rshift(op, rs), 31)
      rs = rs - 5
    elseif p == "S" then
      x = arshift(lshift(op, 27-rs), 27)
      rs = rs - 5
    elseif p == "I" then
      x = arshift(lshift(op, 16), 16)
    elseif p == "U" then
      x = band(op, 0xffff)
    elseif p == "D" or p == "E" then
      local disp = arshift(lshift(op, 16), 16)
      if p == "E" then disp = band(disp, -4) end
      if last == "r0" then last = "0" end
      operands[#operands] = format("%d(%s)", disp, last)
    elseif p >= "2" and p <= "8" then
      local disp = band(rshift(op, rs), 31) * p
      if last == "r0" then last = "0" end
      operands[#operands] = format("%d(%s)", disp, last)
    elseif p == "H" then
      x = band(rshift(op, rs), 31) + lshift(band(op, 2), 4)
      rs = rs - 5
    elseif p == "M" then
      x = band(rshift(op, rs), 31) + band(op, 0x20)
    elseif p == "C" then
      x = condfmt(band(rshift(op, rs), 31))
      rs = rs - 5
    elseif p == "B" then
      local bo = rshift(op, 21)
      local cond = band(rshift(op, 16), 31)
      local cn = ""
      rs = rs - 10
      if band(bo, 4) == 0 then
	cn = band(bo, 2) == 0 and "dnz" or "dz"
	if band(bo, 0x10) == 0 then
	  cn = cn..(band(bo, 8) == 0 and "f" or "t")
	end
	if band(bo, 0x10) == 0 then x = condfmt(cond) end
	name = name..(band(bo, 1) == band(rshift(op, 15), 1) and "-" or "+")
      elseif band(bo, 0x10) == 0 then
	cn = map_cond[band(cond, 3) + (band(bo, 8) == 0 and 4 or 0)]
	if cond > 3 then x = "cr"..rshift(cond, 2) end
	name = name..(band(bo, 1) == band(rshift(op, 15), 1) and "-" or "+")
      end
      name = gsub(name, "_", cn)
    elseif p == "J" then
      x = arshift(lshift(op, 27-rs), 29-rs)*4
      if band(op, 2) == 0 then x = ctx.addr + pos + x end
      ctx.rel = x
      x = "0x"..tohex(x)
    elseif p == "K" then
      if band(op, 1) ~= 0 then name = name.."l" end
      if band(op, 2) ~= 0 then name = name.."a" end
    elseif p == "X" or p == "Y" then
      x = band(rshift(op, rs+2), 7)
      if x == 0 and p == "Y" then x = nil else x = "cr"..x end
      rs = rs - 5
    elseif p == "W" then
      x = "cr"..band(op, 7)
    elseif p == "Z" then
      x = band(rshift(op, rs-4), 255)
      rs = rs - 10
    elseif p == ">" then
      operands[#operands] = rshift(operands[#operands], 1)
    elseif p == "0" then
      if last == "r0" then
	operands[#operands] = nil
	if altname then name = altname end
      end
    elseif p == "L" then
      name = gsub(name, "_", band(op, 0x00200000) ~= 0 and "d" or "w")
    elseif p == "." then
      if band(op, 1) == 1 then name = name.."." end
    elseif p == "N" then
      if op == 0x60000000 then name = "nop"; break end
    elseif p == "~" then
      local n = #operands
      operands[n-1],  operands[n] = operands[n], operands[n-1]
    elseif p == "=" then
      local n = #operands
      if last == operands[n-1] then
	operands[n] = nil
	name = altname
      end
    elseif p == "%" then
      local n = #operands
      if last == operands[n-1] and last == operands[n-2] then
	operands[n] = nil
	operands[n-1] = nil
	name = altname
      end
    elseif p == "-" then
      rs = rs - 5
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
  return ctx
end

-- Simple API: disassemble code (a string) at address and output via out.
local function disass_(code, addr, out)
  create_(code, addr, out):disass()
end

-- Return register name for RID.
local function regname_(r)
  if r < 32 then return map_gpr[r] end
  return "f"..(r-32)
end

-- Public module functions.
module(...)

create = create_
disass = disass_
regname = regname_

