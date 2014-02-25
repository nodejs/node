----------------------------------------------------------------------------
-- LuaJIT x86/x64 disassembler module.
--
-- Copyright (C) 2005-2013 Mike Pall. All rights reserved.
-- Released under the MIT license. See Copyright Notice in luajit.h
----------------------------------------------------------------------------
-- This is a helper module used by the LuaJIT machine code dumper module.
--
-- Sending small code snippets to an external disassembler and mixing the
-- output with our own stuff was too fragile. So I had to bite the bullet
-- and write yet another x86 disassembler. Oh well ...
--
-- The output format is very similar to what ndisasm generates. But it has
-- been developed independently by looking at the opcode tables from the
-- Intel and AMD manuals. The supported instruction set is quite extensive
-- and reflects what a current generation Intel or AMD CPU implements in
-- 32 bit and 64 bit mode. Yes, this includes MMX, SSE, SSE2, SSE3, SSSE3,
-- SSE4.1, SSE4.2, SSE4a and even privileged and hypervisor (VMX/SVM)
-- instructions.
--
-- Notes:
-- * The (useless) a16 prefix, 3DNow and pre-586 opcodes are unsupported.
-- * No attempt at optimization has been made -- it's fast enough for my needs.
-- * The public API may change when more architectures are added.
------------------------------------------------------------------------------

local type = type
local sub, byte, format = string.sub, string.byte, string.format
local match, gmatch, gsub = string.match, string.gmatch, string.gsub
local lower, rep = string.lower, string.rep

-- Map for 1st opcode byte in 32 bit mode. Ugly? Well ... read on.
local map_opc1_32 = {
--0x
[0]="addBmr","addVmr","addBrm","addVrm","addBai","addVai","push es","pop es",
"orBmr","orVmr","orBrm","orVrm","orBai","orVai","push cs","opc2*",
--1x
"adcBmr","adcVmr","adcBrm","adcVrm","adcBai","adcVai","push ss","pop ss",
"sbbBmr","sbbVmr","sbbBrm","sbbVrm","sbbBai","sbbVai","push ds","pop ds",
--2x
"andBmr","andVmr","andBrm","andVrm","andBai","andVai","es:seg","daa",
"subBmr","subVmr","subBrm","subVrm","subBai","subVai","cs:seg","das",
--3x
"xorBmr","xorVmr","xorBrm","xorVrm","xorBai","xorVai","ss:seg","aaa",
"cmpBmr","cmpVmr","cmpBrm","cmpVrm","cmpBai","cmpVai","ds:seg","aas",
--4x
"incVR","incVR","incVR","incVR","incVR","incVR","incVR","incVR",
"decVR","decVR","decVR","decVR","decVR","decVR","decVR","decVR",
--5x
"pushUR","pushUR","pushUR","pushUR","pushUR","pushUR","pushUR","pushUR",
"popUR","popUR","popUR","popUR","popUR","popUR","popUR","popUR",
--6x
"sz*pushaw,pusha","sz*popaw,popa","boundVrm","arplWmr",
"fs:seg","gs:seg","o16:","a16",
"pushUi","imulVrmi","pushBs","imulVrms",
"insb","insVS","outsb","outsVS",
--7x
"joBj","jnoBj","jbBj","jnbBj","jzBj","jnzBj","jbeBj","jaBj",
"jsBj","jnsBj","jpeBj","jpoBj","jlBj","jgeBj","jleBj","jgBj",
--8x
"arith!Bmi","arith!Vmi","arith!Bmi","arith!Vms",
"testBmr","testVmr","xchgBrm","xchgVrm",
"movBmr","movVmr","movBrm","movVrm",
"movVmg","leaVrm","movWgm","popUm",
--9x
"nop*xchgVaR|pause|xchgWaR|repne nop","xchgVaR","xchgVaR","xchgVaR",
"xchgVaR","xchgVaR","xchgVaR","xchgVaR",
"sz*cbw,cwde,cdqe","sz*cwd,cdq,cqo","call farViw","wait",
"sz*pushfw,pushf","sz*popfw,popf","sahf","lahf",
--Ax
"movBao","movVao","movBoa","movVoa",
"movsb","movsVS","cmpsb","cmpsVS",
"testBai","testVai","stosb","stosVS",
"lodsb","lodsVS","scasb","scasVS",
--Bx
"movBRi","movBRi","movBRi","movBRi","movBRi","movBRi","movBRi","movBRi",
"movVRI","movVRI","movVRI","movVRI","movVRI","movVRI","movVRI","movVRI",
--Cx
"shift!Bmu","shift!Vmu","retBw","ret","$lesVrm","$ldsVrm","movBmi","movVmi",
"enterBwu","leave","retfBw","retf","int3","intBu","into","iretVS",
--Dx
"shift!Bm1","shift!Vm1","shift!Bmc","shift!Vmc","aamBu","aadBu","salc","xlatb",
"fp*0","fp*1","fp*2","fp*3","fp*4","fp*5","fp*6","fp*7",
--Ex
"loopneBj","loopeBj","loopBj","sz*jcxzBj,jecxzBj,jrcxzBj",
"inBau","inVau","outBua","outVua",
"callVj","jmpVj","jmp farViw","jmpBj","inBad","inVad","outBda","outVda",
--Fx
"lock:","int1","repne:rep","rep:","hlt","cmc","testb!Bm","testv!Vm",
"clc","stc","cli","sti","cld","std","incb!Bm","incd!Vm",
}
assert(#map_opc1_32 == 255)

-- Map for 1st opcode byte in 64 bit mode (overrides only).
local map_opc1_64 = setmetatable({
  [0x06]=false, [0x07]=false, [0x0e]=false,
  [0x16]=false, [0x17]=false, [0x1e]=false, [0x1f]=false,
  [0x27]=false, [0x2f]=false, [0x37]=false, [0x3f]=false,
  [0x60]=false, [0x61]=false, [0x62]=false, [0x63]="movsxdVrDmt", [0x67]="a32:",
  [0x40]="rex*",   [0x41]="rex*b",   [0x42]="rex*x",   [0x43]="rex*xb",
  [0x44]="rex*r",  [0x45]="rex*rb",  [0x46]="rex*rx",  [0x47]="rex*rxb",
  [0x48]="rex*w",  [0x49]="rex*wb",  [0x4a]="rex*wx",  [0x4b]="rex*wxb",
  [0x4c]="rex*wr", [0x4d]="rex*wrb", [0x4e]="rex*wrx", [0x4f]="rex*wrxb",
  [0x82]=false, [0x9a]=false, [0xc4]=false, [0xc5]=false, [0xce]=false,
  [0xd4]=false, [0xd5]=false, [0xd6]=false, [0xea]=false,
}, { __index = map_opc1_32 })

-- Map for 2nd opcode byte (0F xx). True CISC hell. Hey, I told you.
-- Prefix dependent MMX/SSE opcodes: (none)|rep|o16|repne, -|F3|66|F2
local map_opc2 = {
--0x
[0]="sldt!Dmp","sgdt!Ump","larVrm","lslVrm",nil,"syscall","clts","sysret",
"invd","wbinvd",nil,"ud1",nil,"$prefetch!Bm","femms","3dnowMrmu",
--1x
"movupsXrm|movssXrm|movupdXrm|movsdXrm",
"movupsXmr|movssXmr|movupdXmr|movsdXmr",
"movhlpsXrm$movlpsXrm|movsldupXrm|movlpdXrm|movddupXrm",
"movlpsXmr||movlpdXmr",
"unpcklpsXrm||unpcklpdXrm",
"unpckhpsXrm||unpckhpdXrm",
"movlhpsXrm$movhpsXrm|movshdupXrm|movhpdXrm",
"movhpsXmr||movhpdXmr",
"$prefetcht!Bm","hintnopVm","hintnopVm","hintnopVm",
"hintnopVm","hintnopVm","hintnopVm","hintnopVm",
--2x
"movUmx$","movUmy$","movUxm$","movUym$","movUmz$",nil,"movUzm$",nil,
"movapsXrm||movapdXrm",
"movapsXmr||movapdXmr",
"cvtpi2psXrMm|cvtsi2ssXrVmt|cvtpi2pdXrMm|cvtsi2sdXrVmt",
"movntpsXmr|movntssXmr|movntpdXmr|movntsdXmr",
"cvttps2piMrXm|cvttss2siVrXm|cvttpd2piMrXm|cvttsd2siVrXm",
"cvtps2piMrXm|cvtss2siVrXm|cvtpd2piMrXm|cvtsd2siVrXm",
"ucomissXrm||ucomisdXrm",
"comissXrm||comisdXrm",
--3x
"wrmsr","rdtsc","rdmsr","rdpmc","sysenter","sysexit",nil,"getsec",
"opc3*38",nil,"opc3*3a",nil,nil,nil,nil,nil,
--4x
"cmovoVrm","cmovnoVrm","cmovbVrm","cmovnbVrm",
"cmovzVrm","cmovnzVrm","cmovbeVrm","cmovaVrm",
"cmovsVrm","cmovnsVrm","cmovpeVrm","cmovpoVrm",
"cmovlVrm","cmovgeVrm","cmovleVrm","cmovgVrm",
--5x
"movmskpsVrXm$||movmskpdVrXm$","sqrtpsXrm|sqrtssXrm|sqrtpdXrm|sqrtsdXrm",
"rsqrtpsXrm|rsqrtssXrm","rcppsXrm|rcpssXrm",
"andpsXrm||andpdXrm","andnpsXrm||andnpdXrm",
"orpsXrm||orpdXrm","xorpsXrm||xorpdXrm",
"addpsXrm|addssXrm|addpdXrm|addsdXrm","mulpsXrm|mulssXrm|mulpdXrm|mulsdXrm",
"cvtps2pdXrm|cvtss2sdXrm|cvtpd2psXrm|cvtsd2ssXrm",
"cvtdq2psXrm|cvttps2dqXrm|cvtps2dqXrm",
"subpsXrm|subssXrm|subpdXrm|subsdXrm","minpsXrm|minssXrm|minpdXrm|minsdXrm",
"divpsXrm|divssXrm|divpdXrm|divsdXrm","maxpsXrm|maxssXrm|maxpdXrm|maxsdXrm",
--6x
"punpcklbwPrm","punpcklwdPrm","punpckldqPrm","packsswbPrm",
"pcmpgtbPrm","pcmpgtwPrm","pcmpgtdPrm","packuswbPrm",
"punpckhbwPrm","punpckhwdPrm","punpckhdqPrm","packssdwPrm",
"||punpcklqdqXrm","||punpckhqdqXrm",
"movPrVSm","movqMrm|movdquXrm|movdqaXrm",
--7x
"pshufwMrmu|pshufhwXrmu|pshufdXrmu|pshuflwXrmu","pshiftw!Pmu",
"pshiftd!Pmu","pshiftq!Mmu||pshiftdq!Xmu",
"pcmpeqbPrm","pcmpeqwPrm","pcmpeqdPrm","emms|",
"vmreadUmr||extrqXmuu$|insertqXrmuu$","vmwriteUrm||extrqXrm$|insertqXrm$",
nil,nil,
"||haddpdXrm|haddpsXrm","||hsubpdXrm|hsubpsXrm",
"movVSmMr|movqXrm|movVSmXr","movqMmr|movdquXmr|movdqaXmr",
--8x
"joVj","jnoVj","jbVj","jnbVj","jzVj","jnzVj","jbeVj","jaVj",
"jsVj","jnsVj","jpeVj","jpoVj","jlVj","jgeVj","jleVj","jgVj",
--9x
"setoBm","setnoBm","setbBm","setnbBm","setzBm","setnzBm","setbeBm","setaBm",
"setsBm","setnsBm","setpeBm","setpoBm","setlBm","setgeBm","setleBm","setgBm",
--Ax
"push fs","pop fs","cpuid","btVmr","shldVmru","shldVmrc",nil,nil,
"push gs","pop gs","rsm","btsVmr","shrdVmru","shrdVmrc","fxsave!Dmp","imulVrm",
--Bx
"cmpxchgBmr","cmpxchgVmr","$lssVrm","btrVmr",
"$lfsVrm","$lgsVrm","movzxVrBmt","movzxVrWmt",
"|popcntVrm","ud2Dp","bt!Vmu","btcVmr",
"bsfVrm","bsrVrm|lzcntVrm|bsrWrm","movsxVrBmt","movsxVrWmt",
--Cx
"xaddBmr","xaddVmr",
"cmppsXrmu|cmpssXrmu|cmppdXrmu|cmpsdXrmu","$movntiVmr|",
"pinsrwPrWmu","pextrwDrPmu",
"shufpsXrmu||shufpdXrmu","$cmpxchg!Qmp",
"bswapVR","bswapVR","bswapVR","bswapVR","bswapVR","bswapVR","bswapVR","bswapVR",
--Dx
"||addsubpdXrm|addsubpsXrm","psrlwPrm","psrldPrm","psrlqPrm",
"paddqPrm","pmullwPrm",
"|movq2dqXrMm|movqXmr|movdq2qMrXm$","pmovmskbVrMm||pmovmskbVrXm",
"psubusbPrm","psubuswPrm","pminubPrm","pandPrm",
"paddusbPrm","padduswPrm","pmaxubPrm","pandnPrm",
--Ex
"pavgbPrm","psrawPrm","psradPrm","pavgwPrm",
"pmulhuwPrm","pmulhwPrm",
"|cvtdq2pdXrm|cvttpd2dqXrm|cvtpd2dqXrm","$movntqMmr||$movntdqXmr",
"psubsbPrm","psubswPrm","pminswPrm","porPrm",
"paddsbPrm","paddswPrm","pmaxswPrm","pxorPrm",
--Fx
"|||lddquXrm","psllwPrm","pslldPrm","psllqPrm",
"pmuludqPrm","pmaddwdPrm","psadbwPrm","maskmovqMrm||maskmovdquXrm$",
"psubbPrm","psubwPrm","psubdPrm","psubqPrm",
"paddbPrm","paddwPrm","padddPrm","ud",
}
assert(map_opc2[255] == "ud")

-- Map for three-byte opcodes. Can't wait for their next invention.
local map_opc3 = {
["38"] = { -- [66] 0f 38 xx
--0x
[0]="pshufbPrm","phaddwPrm","phadddPrm","phaddswPrm",
"pmaddubswPrm","phsubwPrm","phsubdPrm","phsubswPrm",
"psignbPrm","psignwPrm","psigndPrm","pmulhrswPrm",
nil,nil,nil,nil,
--1x
"||pblendvbXrma",nil,nil,nil,
"||blendvpsXrma","||blendvpdXrma",nil,"||ptestXrm",
nil,nil,nil,nil,
"pabsbPrm","pabswPrm","pabsdPrm",nil,
--2x
"||pmovsxbwXrm","||pmovsxbdXrm","||pmovsxbqXrm","||pmovsxwdXrm",
"||pmovsxwqXrm","||pmovsxdqXrm",nil,nil,
"||pmuldqXrm","||pcmpeqqXrm","||$movntdqaXrm","||packusdwXrm",
nil,nil,nil,nil,
--3x
"||pmovzxbwXrm","||pmovzxbdXrm","||pmovzxbqXrm","||pmovzxwdXrm",
"||pmovzxwqXrm","||pmovzxdqXrm",nil,"||pcmpgtqXrm",
"||pminsbXrm","||pminsdXrm","||pminuwXrm","||pminudXrm",
"||pmaxsbXrm","||pmaxsdXrm","||pmaxuwXrm","||pmaxudXrm",
--4x
"||pmulddXrm","||phminposuwXrm",
--Fx
[0xf0] = "|||crc32TrBmt",[0xf1] = "|||crc32TrVmt",
},

["3a"] = { -- [66] 0f 3a xx
--0x
[0x00]=nil,nil,nil,nil,nil,nil,nil,nil,
"||roundpsXrmu","||roundpdXrmu","||roundssXrmu","||roundsdXrmu",
"||blendpsXrmu","||blendpdXrmu","||pblendwXrmu","palignrPrmu",
--1x
nil,nil,nil,nil,
"||pextrbVmXru","||pextrwVmXru","||pextrVmSXru","||extractpsVmXru",
nil,nil,nil,nil,nil,nil,nil,nil,
--2x
"||pinsrbXrVmu","||insertpsXrmu","||pinsrXrVmuS",nil,
--4x
[0x40] = "||dppsXrmu",
[0x41] = "||dppdXrmu",
[0x42] = "||mpsadbwXrmu",
--6x
[0x60] = "||pcmpestrmXrmu",[0x61] = "||pcmpestriXrmu",
[0x62] = "||pcmpistrmXrmu",[0x63] = "||pcmpistriXrmu",
},
}

-- Map for VMX/SVM opcodes 0F 01 C0-FF (sgdt group with register operands).
local map_opcvm = {
[0xc1]="vmcall",[0xc2]="vmlaunch",[0xc3]="vmresume",[0xc4]="vmxoff",
[0xc8]="monitor",[0xc9]="mwait",
[0xd8]="vmrun",[0xd9]="vmmcall",[0xda]="vmload",[0xdb]="vmsave",
[0xdc]="stgi",[0xdd]="clgi",[0xde]="skinit",[0xdf]="invlpga",
[0xf8]="swapgs",[0xf9]="rdtscp",
}

-- Map for FP opcodes. And you thought stack machines are simple?
local map_opcfp = {
-- D8-DF 00-BF: opcodes with a memory operand.
-- D8
[0]="faddFm","fmulFm","fcomFm","fcompFm","fsubFm","fsubrFm","fdivFm","fdivrFm",
"fldFm",nil,"fstFm","fstpFm","fldenvVm","fldcwWm","fnstenvVm","fnstcwWm",
-- DA
"fiaddDm","fimulDm","ficomDm","ficompDm",
"fisubDm","fisubrDm","fidivDm","fidivrDm",
-- DB
"fildDm","fisttpDm","fistDm","fistpDm",nil,"fld twordFmp",nil,"fstp twordFmp",
-- DC
"faddGm","fmulGm","fcomGm","fcompGm","fsubGm","fsubrGm","fdivGm","fdivrGm",
-- DD
"fldGm","fisttpQm","fstGm","fstpGm","frstorDmp",nil,"fnsaveDmp","fnstswWm",
-- DE
"fiaddWm","fimulWm","ficomWm","ficompWm",
"fisubWm","fisubrWm","fidivWm","fidivrWm",
-- DF
"fildWm","fisttpWm","fistWm","fistpWm",
"fbld twordFmp","fildQm","fbstp twordFmp","fistpQm",
-- xx C0-FF: opcodes with a pseudo-register operand.
-- D8
"faddFf","fmulFf","fcomFf","fcompFf","fsubFf","fsubrFf","fdivFf","fdivrFf",
-- D9
"fldFf","fxchFf",{"fnop"},nil,
{"fchs","fabs",nil,nil,"ftst","fxam"},
{"fld1","fldl2t","fldl2e","fldpi","fldlg2","fldln2","fldz"},
{"f2xm1","fyl2x","fptan","fpatan","fxtract","fprem1","fdecstp","fincstp"},
{"fprem","fyl2xp1","fsqrt","fsincos","frndint","fscale","fsin","fcos"},
-- DA
"fcmovbFf","fcmoveFf","fcmovbeFf","fcmovuFf",nil,{nil,"fucompp"},nil,nil,
-- DB
"fcmovnbFf","fcmovneFf","fcmovnbeFf","fcmovnuFf",
{nil,nil,"fnclex","fninit"},"fucomiFf","fcomiFf",nil,
-- DC
"fadd toFf","fmul toFf",nil,nil,
"fsub toFf","fsubr toFf","fdivr toFf","fdiv toFf",
-- DD
"ffreeFf",nil,"fstFf","fstpFf","fucomFf","fucompFf",nil,nil,
-- DE
"faddpFf","fmulpFf",nil,{nil,"fcompp"},
"fsubrpFf","fsubpFf","fdivrpFf","fdivpFf",
-- DF
nil,nil,nil,nil,{"fnstsw ax"},"fucomipFf","fcomipFf",nil,
}
assert(map_opcfp[126] == "fcomipFf")

-- Map for opcode groups. The subkey is sp from the ModRM byte.
local map_opcgroup = {
  arith = { "add", "or", "adc", "sbb", "and", "sub", "xor", "cmp" },
  shift = { "rol", "ror", "rcl", "rcr", "shl", "shr", "sal", "sar" },
  testb = { "testBmi", "testBmi", "not", "neg", "mul", "imul", "div", "idiv" },
  testv = { "testVmi", "testVmi", "not", "neg", "mul", "imul", "div", "idiv" },
  incb = { "inc", "dec" },
  incd = { "inc", "dec", "callUmp", "$call farDmp",
	   "jmpUmp", "$jmp farDmp", "pushUm" },
  sldt = { "sldt", "str", "lldt", "ltr", "verr", "verw" },
  sgdt = { "vm*$sgdt", "vm*$sidt", "$lgdt", "vm*$lidt",
	   "smsw", nil, "lmsw", "vm*$invlpg" },
  bt = { nil, nil, nil, nil, "bt", "bts", "btr", "btc" },
  cmpxchg = { nil, "sz*,cmpxchg8bQmp,cmpxchg16bXmp", nil, nil,
	      nil, nil, "vmptrld|vmxon|vmclear", "vmptrst" },
  pshiftw = { nil, nil, "psrlw", nil, "psraw", nil, "psllw" },
  pshiftd = { nil, nil, "psrld", nil, "psrad", nil, "pslld" },
  pshiftq = { nil, nil, "psrlq", nil, nil, nil, "psllq" },
  pshiftdq = { nil, nil, "psrlq", "psrldq", nil, nil, "psllq", "pslldq" },
  fxsave = { "$fxsave", "$fxrstor", "$ldmxcsr", "$stmxcsr",
	     nil, "lfenceDp$", "mfenceDp$", "sfenceDp$clflush" },
  prefetch = { "prefetch", "prefetchw" },
  prefetcht = { "prefetchnta", "prefetcht0", "prefetcht1", "prefetcht2" },
}

------------------------------------------------------------------------------

-- Maps for register names.
local map_regs = {
  B = { "al", "cl", "dl", "bl", "ah", "ch", "dh", "bh",
	"r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" },
  B64 = { "al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil",
	  "r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b" },
  W = { "ax", "cx", "dx", "bx", "sp", "bp", "si", "di",
	"r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w" },
  D = { "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
	"r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d" },
  Q = { "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
	"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15" },
  M = { "mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7",
	"mm0", "mm1", "mm2", "mm3", "mm4", "mm5", "mm6", "mm7" }, -- No x64 ext!
  X = { "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
	"xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15" },
}
local map_segregs = { "es", "cs", "ss", "ds", "fs", "gs", "segr6", "segr7" }

-- Maps for size names.
local map_sz2n = {
  B = 1, W = 2, D = 4, Q = 8, M = 8, X = 16,
}
local map_sz2prefix = {
  B = "byte", W = "word", D = "dword",
  Q = "qword",
  M = "qword", X = "xword",
  F = "dword", G = "qword", -- No need for sizes/register names for these two.
}

------------------------------------------------------------------------------

-- Output a nicely formatted line with an opcode and operands.
local function putop(ctx, text, operands)
  local code, pos, hex = ctx.code, ctx.pos, ""
  local hmax = ctx.hexdump
  if hmax > 0 then
    for i=ctx.start,pos-1 do
      hex = hex..format("%02X", byte(code, i, i))
    end
    if #hex > hmax then hex = sub(hex, 1, hmax)..". "
    else hex = hex..rep(" ", hmax-#hex+2) end
  end
  if operands then text = text.." "..operands end
  if ctx.o16 then text = "o16 "..text; ctx.o16 = false end
  if ctx.a32 then text = "a32 "..text; ctx.a32 = false end
  if ctx.rep then text = ctx.rep.." "..text; ctx.rep = false end
  if ctx.rex then
    local t = (ctx.rexw and "w" or "")..(ctx.rexr and "r" or "")..
	      (ctx.rexx and "x" or "")..(ctx.rexb and "b" or "")
    if t ~= "" then text = "rex."..t.." "..text end
    ctx.rexw = false; ctx.rexr = false; ctx.rexx = false; ctx.rexb = false
    ctx.rex = false
  end
  if ctx.seg then
    local text2, n = gsub(text, "%[", "["..ctx.seg..":")
    if n == 0 then text = ctx.seg.." "..text else text = text2 end
    ctx.seg = false
  end
  if ctx.lock then text = "lock "..text; ctx.lock = false end
  local imm = ctx.imm
  if imm then
    local sym = ctx.symtab[imm]
    if sym then text = text.."\t->"..sym end
  end
  ctx.out(format("%08x  %s%s\n", ctx.addr+ctx.start, hex, text))
  ctx.mrm = false
  ctx.start = pos
  ctx.imm = nil
end

-- Clear all prefix flags.
local function clearprefixes(ctx)
  ctx.o16 = false; ctx.seg = false; ctx.lock = false; ctx.rep = false
  ctx.rexw = false; ctx.rexr = false; ctx.rexx = false; ctx.rexb = false
  ctx.rex = false; ctx.a32 = false
end

-- Fallback for incomplete opcodes at the end.
local function incomplete(ctx)
  ctx.pos = ctx.stop+1
  clearprefixes(ctx)
  return putop(ctx, "(incomplete)")
end

-- Fallback for unknown opcodes.
local function unknown(ctx)
  clearprefixes(ctx)
  return putop(ctx, "(unknown)")
end

-- Return an immediate of the specified size.
local function getimm(ctx, pos, n)
  if pos+n-1 > ctx.stop then return incomplete(ctx) end
  local code = ctx.code
  if n == 1 then
    local b1 = byte(code, pos, pos)
    return b1
  elseif n == 2 then
    local b1, b2 = byte(code, pos, pos+1)
    return b1+b2*256
  else
    local b1, b2, b3, b4 = byte(code, pos, pos+3)
    local imm = b1+b2*256+b3*65536+b4*16777216
    ctx.imm = imm
    return imm
  end
end

-- Process pattern string and generate the operands.
local function putpat(ctx, name, pat)
  local operands, regs, sz, mode, sp, rm, sc, rx, sdisp
  local code, pos, stop = ctx.code, ctx.pos, ctx.stop

  -- Chars used: 1DFGIMPQRSTUVWXacdfgijmoprstuwxyz
  for p in gmatch(pat, ".") do
    local x = nil
    if p == "V" or p == "U" then
      if ctx.rexw then sz = "Q"; ctx.rexw = false
      elseif ctx.o16 then sz = "W"; ctx.o16 = false
      elseif p == "U" and ctx.x64 then sz = "Q"
      else sz = "D" end
      regs = map_regs[sz]
    elseif p == "T" then
      if ctx.rexw then sz = "Q"; ctx.rexw = false else sz = "D" end
      regs = map_regs[sz]
    elseif p == "B" then
      sz = "B"
      regs = ctx.rex and map_regs.B64 or map_regs.B
    elseif match(p, "[WDQMXFG]") then
      sz = p
      regs = map_regs[sz]
    elseif p == "P" then
      sz = ctx.o16 and "X" or "M"; ctx.o16 = false
      regs = map_regs[sz]
    elseif p == "S" then
      name = name..lower(sz)
    elseif p == "s" then
      local imm = getimm(ctx, pos, 1); if not imm then return end
      x = imm <= 127 and format("+0x%02x", imm)
		     or format("-0x%02x", 256-imm)
      pos = pos+1
    elseif p == "u" then
      local imm = getimm(ctx, pos, 1); if not imm then return end
      x = format("0x%02x", imm)
      pos = pos+1
    elseif p == "w" then
      local imm = getimm(ctx, pos, 2); if not imm then return end
      x = format("0x%x", imm)
      pos = pos+2
    elseif p == "o" then -- [offset]
      if ctx.x64 then
	local imm1 = getimm(ctx, pos, 4); if not imm1 then return end
	local imm2 = getimm(ctx, pos+4, 4); if not imm2 then return end
	x = format("[0x%08x%08x]", imm2, imm1)
	pos = pos+8
      else
	local imm = getimm(ctx, pos, 4); if not imm then return end
	x = format("[0x%08x]", imm)
	pos = pos+4
      end
    elseif p == "i" or p == "I" then
      local n = map_sz2n[sz]
      if n == 8 and ctx.x64 and p == "I" then
	local imm1 = getimm(ctx, pos, 4); if not imm1 then return end
	local imm2 = getimm(ctx, pos+4, 4); if not imm2 then return end
	x = format("0x%08x%08x", imm2, imm1)
      else
	if n == 8 then n = 4 end
	local imm = getimm(ctx, pos, n); if not imm then return end
	if sz == "Q" and (imm < 0 or imm > 0x7fffffff) then
	  imm = (0xffffffff+1)-imm
	  x = format(imm > 65535 and "-0x%08x" or "-0x%x", imm)
	else
	  x = format(imm > 65535 and "0x%08x" or "0x%x", imm)
	end
      end
      pos = pos+n
    elseif p == "j" then
      local n = map_sz2n[sz]
      if n == 8 then n = 4 end
      local imm = getimm(ctx, pos, n); if not imm then return end
      if sz == "B" and imm > 127 then imm = imm-256
      elseif imm > 2147483647 then imm = imm-4294967296 end
      pos = pos+n
      imm = imm + pos + ctx.addr
      if imm > 4294967295 and not ctx.x64 then imm = imm-4294967296 end
      ctx.imm = imm
      if sz == "W" then
	x = format("word 0x%04x", imm%65536)
      elseif ctx.x64 then
	local lo = imm % 0x1000000
	x = format("0x%02x%06x", (imm-lo) / 0x1000000, lo)
      else
	x = format("0x%08x", imm)
      end
    elseif p == "R" then
      local r = byte(code, pos-1, pos-1)%8
      if ctx.rexb then r = r + 8; ctx.rexb = false end
      x = regs[r+1]
    elseif p == "a" then x = regs[1]
    elseif p == "c" then x = "cl"
    elseif p == "d" then x = "dx"
    elseif p == "1" then x = "1"
    else
      if not mode then
	mode = ctx.mrm
	if not mode then
	  if pos > stop then return incomplete(ctx) end
	  mode = byte(code, pos, pos)
	  pos = pos+1
	end
	rm = mode%8; mode = (mode-rm)/8
	sp = mode%8; mode = (mode-sp)/8
	sdisp = ""
	if mode < 3 then
	  if rm == 4 then
	    if pos > stop then return incomplete(ctx) end
	    sc = byte(code, pos, pos)
	    pos = pos+1
	    rm = sc%8; sc = (sc-rm)/8
	    rx = sc%8; sc = (sc-rx)/8
	    if ctx.rexx then rx = rx + 8; ctx.rexx = false end
	    if rx == 4 then rx = nil end
	  end
	  if mode > 0 or rm == 5 then
	    local dsz = mode
	    if dsz ~= 1 then dsz = 4 end
	    local disp = getimm(ctx, pos, dsz); if not disp then return end
	    if mode == 0 then rm = nil end
	    if rm or rx or (not sc and ctx.x64 and not ctx.a32) then
	      if dsz == 1 and disp > 127 then
		sdisp = format("-0x%x", 256-disp)
	      elseif disp >= 0 and disp <= 0x7fffffff then
		sdisp = format("+0x%x", disp)
	      else
		sdisp = format("-0x%x", (0xffffffff+1)-disp)
	      end
	    else
	      sdisp = format(ctx.x64 and not ctx.a32 and
		not (disp >= 0 and disp <= 0x7fffffff)
		and "0xffffffff%08x" or "0x%08x", disp)
	    end
	    pos = pos+dsz
	  end
	end
	if rm and ctx.rexb then rm = rm + 8; ctx.rexb = false end
	if ctx.rexr then sp = sp + 8; ctx.rexr = false end
      end
      if p == "m" then
	if mode == 3 then x = regs[rm+1]
	else
	  local aregs = ctx.a32 and map_regs.D or ctx.aregs
	  local srm, srx = "", ""
	  if rm then srm = aregs[rm+1]
	  elseif not sc and ctx.x64 and not ctx.a32 then srm = "rip" end
	  ctx.a32 = false
	  if rx then
	    if rm then srm = srm.."+" end
	    srx = aregs[rx+1]
	    if sc > 0 then srx = srx.."*"..(2^sc) end
	  end
	  x = format("[%s%s%s]", srm, srx, sdisp)
	end
	if mode < 3 and
	   (not match(pat, "[aRrgp]") or match(pat, "t")) then -- Yuck.
	  x = map_sz2prefix[sz].." "..x
	end
      elseif p == "r" then x = regs[sp+1]
      elseif p == "g" then x = map_segregs[sp+1]
      elseif p == "p" then -- Suppress prefix.
      elseif p == "f" then x = "st"..rm
      elseif p == "x" then
	if sp == 0 and ctx.lock and not ctx.x64 then
	  x = "CR8"; ctx.lock = false
	else
	  x = "CR"..sp
	end
      elseif p == "y" then x = "DR"..sp
      elseif p == "z" then x = "TR"..sp
      elseif p == "t" then
      else
	error("bad pattern `"..pat.."'")
      end
    end
    if x then operands = operands and operands..", "..x or x end
  end
  ctx.pos = pos
  return putop(ctx, name, operands)
end

-- Forward declaration.
local map_act

-- Fetch and cache MRM byte.
local function getmrm(ctx)
  local mrm = ctx.mrm
  if not mrm then
    local pos = ctx.pos
    if pos > ctx.stop then return nil end
    mrm = byte(ctx.code, pos, pos)
    ctx.pos = pos+1
    ctx.mrm = mrm
  end
  return mrm
end

-- Dispatch to handler depending on pattern.
local function dispatch(ctx, opat, patgrp)
  if not opat then return unknown(ctx) end
  if match(opat, "%|") then -- MMX/SSE variants depending on prefix.
    local p
    if ctx.rep then
      p = ctx.rep=="rep" and "%|([^%|]*)" or "%|[^%|]*%|[^%|]*%|([^%|]*)"
      ctx.rep = false
    elseif ctx.o16 then p = "%|[^%|]*%|([^%|]*)"; ctx.o16 = false
    else p = "^[^%|]*" end
    opat = match(opat, p)
    if not opat then return unknown(ctx) end
--    ctx.rep = false; ctx.o16 = false
    --XXX fails for 66 f2 0f 38 f1 06  crc32 eax,WORD PTR [esi]
    --XXX remove in branches?
  end
  if match(opat, "%$") then -- reg$mem variants.
    local mrm = getmrm(ctx); if not mrm then return incomplete(ctx) end
    opat = match(opat, mrm >= 192 and "^[^%$]*" or "%$(.*)")
    if opat == "" then return unknown(ctx) end
  end
  if opat == "" then return unknown(ctx) end
  local name, pat = match(opat, "^([a-z0-9 ]*)(.*)")
  if pat == "" and patgrp then pat = patgrp end
  return map_act[sub(pat, 1, 1)](ctx, name, pat)
end

-- Get a pattern from an opcode map and dispatch to handler.
local function dispatchmap(ctx, opcmap)
  local pos = ctx.pos
  local opat = opcmap[byte(ctx.code, pos, pos)]
  pos = pos + 1
  ctx.pos = pos
  return dispatch(ctx, opat)
end

-- Map for action codes. The key is the first char after the name.
map_act = {
  -- Simple opcodes without operands.
  [""] = function(ctx, name, pat)
    return putop(ctx, name)
  end,

  -- Operand size chars fall right through.
  B = putpat, W = putpat, D = putpat, Q = putpat,
  V = putpat, U = putpat, T = putpat,
  M = putpat, X = putpat, P = putpat,
  F = putpat, G = putpat,

  -- Collect prefixes.
  [":"] = function(ctx, name, pat)
    ctx[pat == ":" and name or sub(pat, 2)] = name
    if ctx.pos - ctx.start > 5 then return unknown(ctx) end -- Limit #prefixes.
  end,

  -- Chain to special handler specified by name.
  ["*"] = function(ctx, name, pat)
    return map_act[name](ctx, name, sub(pat, 2))
  end,

  -- Use named subtable for opcode group.
  ["!"] = function(ctx, name, pat)
    local mrm = getmrm(ctx); if not mrm then return incomplete(ctx) end
    return dispatch(ctx, map_opcgroup[name][((mrm-(mrm%8))/8)%8+1], sub(pat, 2))
  end,

  -- o16,o32[,o64] variants.
  sz = function(ctx, name, pat)
    if ctx.o16 then ctx.o16 = false
    else
      pat = match(pat, ",(.*)")
      if ctx.rexw then
	local p = match(pat, ",(.*)")
	if p then pat = p; ctx.rexw = false end
      end
    end
    pat = match(pat, "^[^,]*")
    return dispatch(ctx, pat)
  end,

  -- Two-byte opcode dispatch.
  opc2 = function(ctx, name, pat)
    return dispatchmap(ctx, map_opc2)
  end,

  -- Three-byte opcode dispatch.
  opc3 = function(ctx, name, pat)
    return dispatchmap(ctx, map_opc3[pat])
  end,

  -- VMX/SVM dispatch.
  vm = function(ctx, name, pat)
    return dispatch(ctx, map_opcvm[ctx.mrm])
  end,

  -- Floating point opcode dispatch.
  fp = function(ctx, name, pat)
    local mrm = getmrm(ctx); if not mrm then return incomplete(ctx) end
    local rm = mrm%8
    local idx = pat*8 + ((mrm-rm)/8)%8
    if mrm >= 192 then idx = idx + 64 end
    local opat = map_opcfp[idx]
    if type(opat) == "table" then opat = opat[rm+1] end
    return dispatch(ctx, opat)
  end,

  -- REX prefix.
  rex = function(ctx, name, pat)
    if ctx.rex then return unknown(ctx) end -- Only 1 REX prefix allowed.
    for p in gmatch(pat, ".") do ctx["rex"..p] = true end
    ctx.rex = true
  end,

  -- Special case for nop with REX prefix.
  nop = function(ctx, name, pat)
    return dispatch(ctx, ctx.rex and pat or "nop")
  end,
}

------------------------------------------------------------------------------

-- Disassemble a block of code.
local function disass_block(ctx, ofs, len)
  if not ofs then ofs = 0 end
  local stop = len and ofs+len or #ctx.code
  ofs = ofs + 1
  ctx.start = ofs
  ctx.pos = ofs
  ctx.stop = stop
  ctx.imm = nil
  ctx.mrm = false
  clearprefixes(ctx)
  while ctx.pos <= stop do dispatchmap(ctx, ctx.map1) end
  if ctx.pos ~= ctx.start then incomplete(ctx) end
end

-- Extended API: create a disassembler context. Then call ctx:disass(ofs, len).
local function create_(code, addr, out)
  local ctx = {}
  ctx.code = code
  ctx.addr = (addr or 0) - 1
  ctx.out = out or io.write
  ctx.symtab = {}
  ctx.disass = disass_block
  ctx.hexdump = 16
  ctx.x64 = false
  ctx.map1 = map_opc1_32
  ctx.aregs = map_regs.D
  return ctx
end

local function create64_(code, addr, out)
  local ctx = create_(code, addr, out)
  ctx.x64 = true
  ctx.map1 = map_opc1_64
  ctx.aregs = map_regs.Q
  return ctx
end

-- Simple API: disassemble code (a string) at address and output via out.
local function disass_(code, addr, out)
  create_(code, addr, out):disass()
end

local function disass64_(code, addr, out)
  create64_(code, addr, out):disass()
end

-- Return register name for RID.
local function regname_(r)
  if r < 8 then return map_regs.D[r+1] end
  return map_regs.X[r-7]
end

local function regname64_(r)
  if r < 16 then return map_regs.Q[r+1] end
  return map_regs.X[r-15]
end

-- Public module functions.
module(...)

create = create_
create64 = create64_
disass = disass_
disass64 = disass64_
regname = regname_
regname64 = regname64_

