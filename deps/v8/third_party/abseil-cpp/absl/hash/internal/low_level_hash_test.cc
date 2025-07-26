// Copyright 2020 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/hash/internal/low_level_hash.h"

#include <cinttypes>
#include <cstdint>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/escaping.h"

#define UPDATE_GOLDEN 0

namespace {

static const uint64_t kSalt[5] = {0xa0761d6478bd642f, 0xe7037ed1a0b428dbl,
                                  0x8ebc6af09c88c6e3, 0x589965cc75374cc3l,
                                  0x1d8e4e27c47d124f};

TEST(LowLevelHashTest, VerifyGolden) {
  constexpr size_t kNumGoldenOutputs = 94;
  static struct {
    absl::string_view base64_data;
    uint64_t seed;
  } cases[] = {
      {"VprUGNH+5NnNRaORxgH/ySrZFQFDL+4VAodhfBNinmn8cg==",
       uint64_t{0x531858a40bfa7ea1}},
      {"gc1xZaY+q0nPcUvOOnWnT3bqfmT/geth/f7Dm2e/DemMfk4=",
       uint64_t{0x86689478a7a7e8fa}},
      {"Mr35fIxqx1ukPAL0su1yFuzzAU3wABCLZ8+ZUFsXn47UmAph",
       uint64_t{0x4ec948b8e7f27288}},
      {"A9G8pw2+m7+rDtWYAdbl8tb2fT7FFo4hLi2vAsa5Y8mKH3CX3g==",
       uint64_t{0xce46c7213c10032}},
      {"DFaJGishGwEHDdj9ixbCoaTjz9KS0phLNWHVVdFsM93CvPft3hM=",
       uint64_t{0xf63e96ee6f32a8b6}},
      {"7+Ugx+Kr3aRNgYgcUxru62YkTDt5Hqis+2po81hGBkcrJg4N0uuy",
       uint64_t{0x1cfe85e65fc5225}},
      {"H2w6O8BUKqu6Tvj2xxaecxEI2wRgIgqnTTG1WwOgDSINR13Nm4d4Vg==",
       uint64_t{0x45c474f1cee1d2e8}},
      {"1XBMnIbqD5jy65xTDaf6WtiwtdtQwv1dCVoqpeKj+7cTR1SaMWMyI04=",
       uint64_t{0x6e024e14015f329c}},
      {"znZbdXG2TSFrKHEuJc83gPncYpzXGbAebUpP0XxzH0rpe8BaMQ17nDbt",
       uint64_t{0x760c40502103ae1c}},
      {"ylu8Atu13j1StlcC1MRMJJXIl7USgDDS22HgVv0WQ8hx/8pNtaiKB17hCQ==",
       uint64_t{0x17fd05c3c560c320}},
      {"M6ZVVzsd7vAvbiACSYHioH/440dp4xG2mLlBnxgiqEvI/aIEGpD0Sf4VS0g=",
       uint64_t{0x8b34200a6f8e90d9}},
      {"li3oFSXLXI+ubUVGJ4blP6mNinGKLHWkvGruun85AhVn6iuMtocbZPVhqxzn",
       uint64_t{0x6be89e50818bdf69}},
      {"kFuQHuUCqBF3Tc3hO4dgdIp223ShaCoog48d5Do5zMqUXOh5XpGK1t5XtxnfGA==",
       uint64_t{0xfb389773315b47d8}},
      {"jWmOad0v0QhXVJd1OdGuBZtDYYS8wBVHlvOeTQx9ZZnm8wLEItPMeihj72E0nWY=",
       uint64_t{0x4f2512a23f61efee}},
      {"z+DHU52HaOQdW4JrZwDQAebEA6rm13Zg/9lPYA3txt3NjTBqFZlOMvTRnVzRbl23",
       uint64_t{0x59ccd92fc16c6fda}},
      {"MmBiGDfYeTayyJa/tVycg+rN7f9mPDFaDc+23j0TlW9094er0ADigsl4QX7V3gG/qw==",
       uint64_t{0x25c5a7f5bd330919}},
      {"774RK+9rOL4iFvs1q2qpo/JVc/I39buvNjqEFDtDvyoB0FXxPI2vXqOrk08VPfIHkmU=",
       uint64_t{0x51df4174d34c97d7}},
      {"+slatXiQ7/2lK0BkVUI1qzNxOOLP3I1iK6OfHaoxgqT63FpzbElwEXSwdsryq3UlHK0I",
       uint64_t{0x80ce6d76f89cb57}},
      {"64mVTbQ47dHjHlOHGS/hjJwr/"
       "K2frCNpn87exOqMzNUVYiPKmhCbfS7vBUce5tO6Ec9osQ==",
       uint64_t{0x20961c911965f684}},
      {"fIsaG1r530SFrBqaDj1kqE0AJnvvK8MNEZbII2Yw1OK77v0V59xabIh0B5axaz/"
       "+a2V5WpA=",
       uint64_t{0x4e5b926ec83868e7}},
      {"PGih0zDEOWCYGxuHGDFu9Ivbff/"
       "iE7BNUq65tycTR2R76TerrXALRosnzaNYO5fjFhTi+CiS",
       uint64_t{0x3927b30b922eecef}},
      {"RnpA/"
       "zJnEnnLjmICORByRVb9bCOgxF44p3VMiW10G7PvW7IhwsWajlP9kIwNA9FjAD2GoQHk2Q="
       "=",
       uint64_t{0xbd0291284a49b61c}},
      {"qFklMceaTHqJpy2qavJE+EVBiNFOi6OxjOA3LeIcBop1K7w8xQi3TrDk+"
       "BrWPRIbfprszSaPfrI=",
       uint64_t{0x73a77c575bcc956}},
      {"cLbfUtLl3EcQmITWoTskUR8da/VafRDYF/ylPYwk7/"
       "zazk6ssyrzxMN3mmSyvrXR2yDGNZ3WDrTT",
       uint64_t{0x766a0e2ade6d09a6}},
      {"s/"
       "Jf1+"
       "FbsbCpXWPTUSeWyMH6e4CvTFvPE5Fs6Z8hvFITGyr0dtukHzkI84oviVLxhM1xMxrMAy1db"
       "w==",
       uint64_t{0x2599f4f905115869}},
      {"FvyQ00+j7nmYZVQ8hI1Edxd0AWplhTfWuFGiu34AK5X8u2hLX1bE97sZM0CmeLe+"
       "7LgoUT1fJ/axybE=",
       uint64_t{0xd8256e5444d21e53}},
      {"L8ncxMaYLBH3g9buPu8hfpWZNlOF7nvWLNv9IozH07uQsIBWSKxoPy8+"
       "LW4tTuzC6CIWbRGRRD1sQV/4",
       uint64_t{0xf664a91333fb8dfd}},
      {"CDK0meI07yrgV2kQlZZ+"
       "wuVqhc2NmzqeLH7bmcA6kchsRWFPeVF5Wqjjaj556ABeUoUr3yBmfU3kWOakkg==",
       uint64_t{0x9625b859be372cd1}},
      {"d23/vc5ONh/"
       "HkMiq+gYk4gaCNYyuFKwUkvn46t+dfVcKfBTYykr4kdvAPNXGYLjM4u1YkAEFpJP+"
       "nX7eOvs=",
       uint64_t{0x7b99940782e29898}},
      {"NUR3SRxBkxTSbtQORJpu/GdR6b/h6sSGfsMj/KFd99ahbh+9r7LSgSGmkGVB/"
       "mGoT0pnMTQst7Lv2q6QN6Vm",
       uint64_t{0x4fe12fa5383b51a8}},
      {"2BOFlcI3Z0RYDtS9T9Ie9yJoXlOdigpPeeT+CRujb/"
       "O39Ih5LPC9hP6RQk1kYESGyaLZZi3jtabHs7DiVx/VDg==",
       uint64_t{0xe2ccb09ac0f5b4b6}},
      {"FF2HQE1FxEvWBpg6Z9zAMH+Zlqx8S1JD/"
       "wIlViL6ZDZY63alMDrxB0GJQahmAtjlm26RGLnjW7jmgQ4Ie3I+014=",
       uint64_t{0x7d0a37adbd7b753b}},
      {"tHmO7mqVL/PX11nZrz50Hc+M17Poj5lpnqHkEN+4bpMx/"
       "YGbkrGOaYjoQjgmt1X2QyypK7xClFrjeWrCMdlVYtbW",
       uint64_t{0xd3ae96ef9f7185f2}},
      {"/WiHi9IQcxRImsudkA/KOTqGe8/"
       "gXkhKIHkjddv5S9hi02M049dIK3EUyAEjkjpdGLUs+BN0QzPtZqjIYPOgwsYE9g==",
       uint64_t{0x4fb88ea63f79a0d8}},
      {"qds+1ExSnU11L4fTSDz/QE90g4Jh6ioqSh3KDOTOAo2pQGL1k/"
       "9CCC7J23YF27dUTzrWsCQA2m4epXoCc3yPHb3xElA=",
       uint64_t{0xed564e259bb5ebe9}},
      {"8FVYHx40lSQPTHheh08Oq0/"
       "pGm2OlG8BEf8ezvAxHuGGdgCkqpXIueJBF2mQJhTfDy5NncO8ntS7vaKs7sCNdDaNGOEi",
       uint64_t{0x3e3256b60c428000}},
      {"4ZoEIrJtstiCkeew3oRzmyJHVt/pAs2pj0HgHFrBPztbQ10NsQ/"
       "lM6DM439QVxpznnBSiHMgMQJhER+70l72LqFTO1JiIQ==",
       uint64_t{0xfb05bad59ec8705}},
      {"hQPtaYI+wJyxXgwD5n8jGIKFKaFA/"
       "P83KqCKZfPthnjwdOFysqEOYwAaZuaaiv4cDyi9TyS8hk5cEbNP/jrI7q6pYGBLbsM=",
       uint64_t{0xafdc251dbf97b5f8}},
      {"S4gpMSKzMD7CWPsSfLeYyhSpfWOntyuVZdX1xSBjiGvsspwOZcxNKCRIOqAA0moUfOh3I5+"
       "juQV4rsqYElMD/gWfDGpsWZKQ",
       uint64_t{0x10ec9c92ddb5dcbc}},
      {"oswxop+"
       "bthuDLT4j0PcoSKby4LhF47ZKg8K17xxHf74UsGCzTBbOz0MM8hQEGlyqDT1iUiAYnaPaUp"
       "L2mRK0rcIUYA4qLt5uOw==",
       uint64_t{0x9a767d5822c7dac4}},
      {"0II/"
       "697p+"
       "BtLSjxj5989OXI004TogEb94VUnDzOVSgMXie72cuYRvTFNIBgtXlKfkiUjeqVpd4a+"
       "n5bxNOD1TGrjQtzKU5r7obo=",
       uint64_t{0xee46254080d6e2db}},
      {"E84YZW2qipAlMPmctrg7TKlwLZ68l4L+c0xRDUfyyFrA4MAti0q9sHq3TDFviH0Y+"
       "Kq3tEE5srWFA8LM9oomtmvm5PYxoaarWPLc",
       uint64_t{0xbbb669588d8bf398}},
      {"x3pa4HIElyZG0Nj7Vdy9IdJIR4izLmypXw5PCmZB5y68QQ4uRaVVi3UthsoJROvbjDJkP2D"
       "Q6L/eN8pFeLFzNPKBYzcmuMOb5Ull7w==",
       uint64_t{0xdc2afaa529beef44}},
      {"jVDKGYIuWOP/"
       "QKLdd2wi8B2VJA8Wh0c8PwrXJVM8FOGM3voPDVPyDJOU6QsBDPseoR8uuKd19OZ/"
       "zAvSCB+zlf6upAsBlheUKgCfKww=",
       uint64_t{0xf1f67391d45013a8}},
      {"mkquunhmYe1aR2wmUz4vcvLEcKBoe6H+kjUok9VUn2+eTSkWs4oDDtJvNCWtY5efJwg/"
       "j4PgjRYWtqnrCkhaqJaEvkkOwVfgMIwF3e+d",
       uint64_t{0x16fce2b8c65a3429}},
      {"fRelvKYonTQ+s+rnnvQw+JzGfFoPixtna0vzcSjiDqX5s2Kg2//"
       "UGrK+AVCyMUhO98WoB1DDbrsOYSw2QzrcPe0+3ck9sePvb+Q/IRaHbw==",
       uint64_t{0xf4b096699f49fe67}},
      {"DUwXFJzagljo44QeJ7/"
       "6ZKw4QXV18lhkYT2jglMr8WB3CHUU4vdsytvw6AKv42ZcG6fRkZkq9fpnmXy6xG0aO3WPT1"
       "eHuyFirAlkW+zKtwg=",
       uint64_t{0xca584c4bc8198682}},
      {"cYmZCrOOBBongNTr7e4nYn52uQUy2mfe48s50JXx2AZ6cRAt/"
       "xRHJ5QbEoEJOeOHsJyM4nbzwFm++SlT6gFZZHJpkXJ92JkR86uS/eV1hJUR",
       uint64_t{0xed269fc3818b6aad}},
      {"EXeHBDfhwzAKFhsMcH9+2RHwV+mJaN01+9oacF6vgm8mCXRd6jeN9U2oAb0of5c5cO4i+"
       "Vb/LlHZSMI490SnHU0bejhSCC2gsC5d2K30ER3iNA==",
       uint64_t{0x33f253cbb8fe66a8}},
      {"FzkzRYoNjkxFhZDso94IHRZaJUP61nFYrh5MwDwv9FNoJ5jyNCY/"
       "eazPZk+tbmzDyJIGw2h3GxaWZ9bSlsol/vK98SbkMKCQ/wbfrXRLcDzdd/8=",
       uint64_t{0xd0b76b2c1523d99c}},
      {"Re4aXISCMlYY/XsX7zkIFR04ta03u4zkL9dVbLXMa/q6hlY/CImVIIYRN3VKP4pnd0AUr/"
       "ugkyt36JcstAInb4h9rpAGQ7GMVOgBniiMBZ/MGU7H",
       uint64_t{0xfd28f0811a2a237f}},
      {"ueLyMcqJXX+MhO4UApylCN9WlTQ+"
       "ltJmItgG7vFUtqs2qNwBMjmAvr5u0sAKd8jpzV0dDPTwchbIeAW5zbtkA2NABJV6hFM48ib"
       "4/J3A5mseA3cS8w==",
       uint64_t{0x6261fb136482e84}},
      {"6Si7Yi11L+jZMkwaN+GUuzXMrlvEqviEkGOilNq0h8TdQyYKuFXzkYc/"
       "q74gP3pVCyiwz9KpVGMM9vfnq36riMHRknkmhQutxLZs5fbmOgEO69HglCU=",
       uint64_t{0x458efc750bca7c3a}},
      {"Q6AbOofGuTJOegPh9Clm/"
       "9crtUMQqylKrTc1fhfJo1tqvpXxhU4k08kntL1RG7woRnFrVh2UoMrL1kjin+s9CanT+"
       "y4hHwLqRranl9FjvxfVKm3yvg68",
       uint64_t{0xa7e69ff84e5e7c27}},
      {"ieQEbIPvqY2YfIjHnqfJiO1/MIVRk0RoaG/WWi3kFrfIGiNLCczYoklgaecHMm/"
       "1sZ96AjO+a5stQfZbJQwS7Sc1ODABEdJKcTsxeW2hbh9A6CFzpowP1A==",
       uint64_t{0x3c59bfd0c29efe9e}},
      {"zQUv8hFB3zh2GGl3KTvCmnfzE+"
       "SUgQPVaSVIELFX5H9cE3FuVFGmymkPQZJLAyzC90Cmi8GqYCvPqTuAAB//"
       "XTJxy4bCcVArgZG9zJXpjowpNBfr3ngWrSE=",
       uint64_t{0x10befacc6afd298d}},
      {"US4hcC1+op5JKGC7eIs8CUgInjKWKlvKQkapulxW262E/"
       "B2ye79QxOexf188u2mFwwe3WTISJHRZzS61IwljqAWAWoBAqkUnW8SHmIDwHUP31J0p5sGd"
       "P47L",
       uint64_t{0x41d5320b0a38efa7}},
      {"9bHUWFna2LNaGF6fQLlkx1Hkt24nrkLE2CmFdWgTQV3FFbUe747SSqYw6ebpTa07MWSpWRP"
       "sHesVo2B9tqHbe7eQmqYebPDFnNqrhSdZwFm9arLQVs+7a3Ic6A==",
       uint64_t{0x58db1c7450fe17f3}},
      {"Kb3DpHRUPhtyqgs3RuXjzA08jGb59hjKTOeFt1qhoINfYyfTt2buKhD6YVffRCPsgK9SeqZ"
       "qRPJSyaqsa0ovyq1WnWW8jI/NhvAkZTVHUrX2pC+cD3OPYT05Dag=",
       uint64_t{0x6098c055a335b7a6}},
      {"gzxyMJIPlU+bJBwhFUCHSofZ/"
       "319LxqMoqnt3+L6h2U2+ZXJCSsYpE80xmR0Ta77Jq54o92SMH87HV8dGOaCTuAYF+"
       "lDL42SY1P316Cl0sZTS2ow3ZqwGbcPNs/1",
       uint64_t{0x1bbacec67845a801}},
      {"uR7V0TW+FGVMpsifnaBAQ3IGlr1wx5sKd7TChuqRe6OvUXTlD4hKWy8S+"
       "8yyOw8lQabism19vOQxfmocEOW/"
       "vzY0pEa87qHrAZy4s9fH2Bltu8vaOIe+agYohhYORQ==",
       uint64_t{0xc419cfc7442190}},
      {"1UR5eoo2aCwhacjZHaCh9bkOsITp6QunUxHQ2SfeHv0imHetzt/"
       "Z70mhyWZBalv6eAx+YfWKCUib2SHDtz/"
       "A2dc3hqUWX5VfAV7FQsghPUAtu6IiRatq4YSLpDvKZBQ=",
       uint64_t{0xc95e510d94ba270c}},
      {"opubR7H63BH7OtY+Avd7QyQ25UZ8kLBdFDsBTwZlY6gA/"
       "u+x+"
       "czC9AaZMgmQrUy15DH7YMGsvdXnviTtI4eVI4aF1H9Rl3NXMKZgwFOsdTfdcZeeHVRzBBKX"
       "8jUfh1il",
       uint64_t{0xff1ae05c98089c3f}},
      {"DC0kXcSXtfQ9FbSRwirIn5tgPri0sbzHSa78aDZVDUKCMaBGyFU6BmrulywYX8yzvwprdLs"
       "oOwTWN2wMjHlPDqrvVHNEjnmufRDblW+nSS+xtKNs3N5xsxXdv6JXDrAB/Q==",
       uint64_t{0x90c02b8dceced493}},
      {"BXRBk+3wEP3Lpm1y75wjoz+PgB0AMzLe8tQ1AYU2/"
       "oqrQB2YMC6W+9QDbcOfkGbeH+b7IBkt/"
       "gwCMw2HaQsRFEsurXtcQ3YwRuPz5XNaw5NAvrNa67Fm7eRzdE1+hWLKtA8=",
       uint64_t{0x9f8a76697ab1aa36}},
      {"RRBSvEGYnzR9E45Aps/+WSnpCo/X7gJLO4DRnUqFrJCV/kzWlusLE/"
       "6ZU6RoUf2ROwcgEvUiXTGjLs7ts3t9SXnJHxC1KiOzxHdYLMhVvgNd3hVSAXODpKFSkVXND"
       "55G2L1W",
       uint64_t{0x6ba1bf3d811a531d}},
      {"jeh6Qazxmdi57pa9S3XSnnZFIRrnc6s8QLrah5OX3SB/V2ErSPoEAumavzQPkdKF1/"
       "SfvmdL+qgF1C+Yawy562QaFqwVGq7+tW0yxP8FStb56ZRgNI4IOmI30s1Ei7iops9Uuw==",
       uint64_t{0x6a418974109c67b4}},
      {"6QO5nnDrY2/"
       "wrUXpltlKy2dSBcmK15fOY092CR7KxAjNfaY+"
       "aAmtWbbzQk3MjBg03x39afSUN1fkrWACdyQKRaGxgwq6MGNxI6W+8DLWJBHzIXrntrE/"
       "ml6fnNXEpxplWJ1vEs4=",
       uint64_t{0x8472f1c2b3d230a3}},
      {"0oPxeEHhqhcFuwonNfLd5jF3RNATGZS6NPoS0WklnzyokbTqcl4BeBkMn07+fDQv83j/"
       "BpGUwcWO05f3+DYzocfnizpFjLJemFGsls3gxcBYxcbqWYev51tG3lN9EvRE+X9+Pwww",
       uint64_t{0x5e06068f884e73a7}},
      {"naSBSjtOKgAOg8XVbR5cHAW3Y+QL4Pb/JO9/"
       "oy6L08wvVRZqo0BrssMwhzBP401Um7A4ppAupbQeJFdMrysY34AuSSNvtNUy5VxjNECwiNt"
       "gwYHw7yakDUv8WvonctmnoSPKENegQg==",
       uint64_t{0x55290b1a8f170f59}},
      {"vPyl8DxVeRe1OpilKb9KNwpGkQRtA94UpAHetNh+"
       "95V7nIW38v7PpzhnTWIml5kw3So1Si0TXtIUPIbsu32BNhoH7QwFvLM+"
       "JACgSpc5e3RjsL6Qwxxi11npwxRmRUqATDeMUfRAjxg=",
       uint64_t{0x5501cfd83dfe706a}},
      {"QC9i2GjdTMuNC1xQJ74ngKfrlA4w3o58FhvNCltdIpuMhHP1YsDA78scQPLbZ3OCUgeQguY"
       "f/vw6zAaVKSgwtaykqg5ka/4vhz4hYqWU5ficdXqClHl+zkWEY26slCNYOM5nnDlly8Cj",
       uint64_t{0xe43ed13d13a66990}},
      {"7CNIgQhAHX27nxI0HeB5oUTnTdgKpRDYDKwRcXfSFGP1XeT9nQF6WKCMjL1tBV6x7KuJ91G"
       "Zz11F4c+8s+MfqEAEpd4FHzamrMNjGcjCyrVtU6y+7HscMVzr7Q/"
       "ODLcPEFztFnwjvCjmHw==",
       uint64_t{0xdf43bc375cf5283f}},
      {"Qa/hC2RPXhANSospe+gUaPfjdK/yhQvfm4cCV6/pdvCYWPv8p1kMtKOX3h5/"
       "8oZ31fsmx4Axphu5qXJokuhZKkBUJueuMpxRyXpwSWz2wELx5glxF7CM0Fn+"
       "OevnkhUn5jsPlG2r5jYlVn8=",
       uint64_t{0x8112b806d288d7b5}},
      {"kUw/0z4l3a89jTwN5jpG0SHY5km/"
       "IVhTjgM5xCiPRLncg40aqWrJ5vcF891AOq5hEpSq0bUCJUMFXgct7kvnys905HjerV7Vs1G"
       "y84tgVJ70/2+pAZTsB/PzNOE/G6sOj4+GbTzkQu819OLB",
       uint64_t{0xd52a18abb001cb46}},
      {"VDdfSDbO8Tdj3T5W0XM3EI7iHh5xpIutiM6dvcJ/fhe23V/srFEkDy5iZf/"
       "VnA9kfi2C79ENnFnbOReeuZW1b3MUXB9lgC6U4pOTuC+"
       "jHK3Qnpyiqzj7h3ISJSuo2pob7vY6VHZo6Fn7exEqHg==",
       uint64_t{0xe12b76a2433a1236}},
      {"Ldfvy3ORdquM/R2fIkhH/ONi69mcP1AEJ6n/"
       "oropwecAsLJzQSgezSY8bEiEs0VnFTBBsW+RtZY6tDj03fnb3amNUOq1b7jbqyQkL9hpl+"
       "2Z2J8IaVSeownWl+bQcsR5/xRktIMckC5AtF4YHfU=",
       uint64_t{0x175bf7319cf1fa00}},
      {"BrbNpb42+"
       "VzZAjJw6QLirXzhweCVRfwlczzZ0VX2xluskwBqyfnGovz5EuX79JJ31VNXa5hTkAyQat3l"
       "YKRADTdAdwE5PqM1N7YaMqqsqoAAAeuYVXuk5eWCykYmClNdSspegwgCuT+403JigBzi",
       uint64_t{0xd63d57b3f67525ae}},
      {"gB3NGHJJvVcuPyF0ZSvHwnWSIfmaI7La24VMPQVoIIWF7Z74NltPZZpx2f+cocESM+"
       "ILzQW9p+BC8x5IWz7N4Str2WLGKMdgmaBfNkEhSHQDU0IJEOnpUt0HmjhFaBlx0/"
       "LTmhua+rQ6Wup8ezLwfg==",
       uint64_t{0x933faea858832b73}},
      {"hTKHlRxx6Pl4gjG+6ksvvj0CWFicUg3WrPdSJypDpq91LUWRni2KF6+"
       "81ZoHBFhEBrCdogKqeK+hy9bLDnx7g6rAFUjtn1+cWzQ2YjiOpz4+"
       "ROBB7lnwjyTGWzJD1rXtlso1g2qVH8XJVigC5M9AIxM=",
       uint64_t{0x53d061e5f8e7c04f}},
      {"IWQBelSQnhrr0F3BhUpXUIDauhX6f95Qp+A0diFXiUK7irwPG1oqBiqHyK/SH/"
       "9S+"
       "rln9DlFROAmeFdH0OCJi2tFm4afxYzJTFR4HnR4cG4x12JqHaZLQx6iiu6CE3rtWBVz99oA"
       "wCZUOEXIsLU24o2Y",
       uint64_t{0xdb4124556dd515e0}},
      {"TKo+l+"
       "1dOXdLvIrFqeLaHdm0HZnbcdEgOoLVcGRiCbAMR0j5pIFw8D36tefckAS1RCFOH5IgP8yiF"
       "T0Gd0a2hI3+"
       "fTKA7iK96NekxWeoeqzJyctc6QsoiyBlkZerRxs5RplrxoeNg29kKDTM0K94mnhD9g==",
       uint64_t{0x4fb31a0dd681ee71}},
      {"YU4e7G6EfQYvxCFoCrrT0EFgVLHFfOWRTJQJ5gxM3G2b+"
       "1kJf9YPrpsxF6Xr6nYtS8reEEbDoZJYqnlk9lXSkVArm88Cqn6d25VCx3+"
       "49MqC0trIlXtb7SXUUhwpJK16T0hJUfPH7s5cMZXc6YmmbFuBNPE=",
       uint64_t{0x27cc72eefa138e4c}},
      {"/I/"
       "eImMwPo1U6wekNFD1Jxjk9XQVi1D+"
       "FPdqcHifYXQuP5aScNQfxMAmaPR2XhuOQhADV5tTVbBKwCDCX4E3jcDNHzCiPvViZF1W27t"
       "xaf2BbFQdwKrNCmrtzcluBFYu0XZfc7RU1RmxK/RtnF1qHsq/O4pp",
       uint64_t{0x44bc2dfba4bd3ced}},
      {"CJTT9WGcY2XykTdo8KodRIA29qsqY0iHzWZRjKHb9alwyJ7RZAE3V5Juv4MY3MeYEr1EPCC"
       "MxO7yFXqT8XA8YTjaMp3bafRt17Pw8JC4iKJ1zN+WWKOESrj+"
       "3aluGQqn8z1EzqY4PH7rLG575PYeWsP98BugdA==",
       uint64_t{0x242da1e3a439bed8}},
      {"ZlhyQwLhXQyIUEnMH/"
       "AEW27vh9xrbNKJxpWGtrEmKhd+nFqAfbeNBQjW0SfG1YI0xQkQMHXjuTt4P/"
       "EpZRtA47ibZDVS8TtaxwyBjuIDwqcN09eCtpC+Ls+"
       "vWDTLmBeDM3u4hmzz4DQAYsLiZYSJcldg9Q3wszw=",
       uint64_t{0xdc559c746e35c139}},
      {"v2KU8y0sCrBghmnm8lzGJlwo6D6ObccAxCf10heoDtYLosk4ztTpLlpSFEyu23MLA1tJkcg"
       "Rko04h19QMG0mOw/"
       "wc93EXAweriBqXfvdaP85sZABwiKO+6rtS9pacRVpYYhHJeVTQ5NzrvBvi1huxAr+"
       "xswhVMfL",
       uint64_t{0xd0b0350275b9989}},
      {"QhKlnIS6BuVCTQsnoE67E/"
       "yrgogE8EwO7xLaEGei26m0gEU4OksefJgppDh3X0x0Cs78Dr9IHK5b977CmZlrTRmwhlP8p"
       "M+UzXPNRNIZuN3ntOum/QhUWP8SGpirheXENWsXMQ/"
       "nxtxakyEtrNkKk471Oov9juP8oQ==",
       uint64_t{0xb04489e41d17730c}},
      {"/ZRMgnoRt+Uo6fUPr9FqQvKX7syhgVqWu+"
       "WUSsiQ68UlN0efSP6Eced5gJZL6tg9gcYJIkhjuQNITU0Q3TjVAnAcobgbJikCn6qZ6pRxK"
       "BY4MTiAlfGD3T7R7hwJwx554MAy++Zb/YUFlnCaCJiwQMnowF7aQzwYFCo=",
       uint64_t{0x2217285eb4572156}},
      {"NB7tU5fNE8nI+SXGfipc7sRkhnSkUF1krjeo6k+8FITaAtdyz+"
       "o7mONgXmGLulBPH9bEwyYhKNVY0L+njNQrZ9YC2aXsFD3PdZsxAFaBT3VXEzh+"
       "NGBTjDASNL3mXyS8Yv1iThGfHoY7T4aR0NYGJ+k+pR6f+KrPC96M",
       uint64_t{0x12c2e8e68aede73b}},
      {"8T6wrqCtEO6/rwxF6lvMeyuigVOLwPipX/FULvwyu+1wa5sQGav/"
       "2FsLHUVn6cGSi0LlFwLewGHPFJDLR0u4t7ZUyM//"
       "x6da0sWgOa5hzDqjsVGmjxEHXiaXKW3i4iSZNuxoNbMQkIbVML+"
       "DkYu9ND0O2swg4itGeVSzXA==",
       uint64_t{0x4d612125bdc4fd00}},
      {"Ntf1bMRdondtMv1CYr3G80iDJ4WSAlKy5H34XdGruQiCrnRGDBa+"
       "eUi7vKp4gp3BBcVGl8eYSasVQQjn7MLvb3BjtXx6c/"
       "bCL7JtpzQKaDnPr9GWRxpBXVxKREgMM7d8lm35EODv0w+"
       "hQLfVSh8OGs7fsBb68nNWPLeeSOo=",
       uint64_t{0x81826b553954464e}},
      {"VsSAw72Ro6xks02kaiLuiTEIWBC5bgqr4WDnmP8vglXzAhixk7td926rm9jNimL+"
       "kroPSygZ9gl63aF5DCPOACXmsbmhDrAQuUzoh9ZKhWgElLQsrqo1KIjWoZT5b5QfVUXY9lS"
       "IBg3U75SqORoTPq7HalxxoIT5diWOcJQi",
       uint64_t{0xc2e5d345dc0ddd2d}},
      {"j+loZ+C87+"
       "bJxNVebg94gU0mSLeDulcHs84tQT7BZM2rzDSLiCNxUedHr1ZWJ9ejTiBa0dqy2I2ABc++"
       "xzOLcv+//YfibtjKtYggC6/3rv0XCc7xu6d/"
       "O6xO+XOBhOWAQ+IHJVHf7wZnDxIXB8AUHsnjEISKj7823biqXjyP3g==",
       uint64_t{0x3da6830a9e32631e}},
      {"f3LlpcPElMkspNtDq5xXyWU62erEaKn7RWKlo540gR6mZsNpK1czV/"
       "sOmqaq8XAQLEn68LKj6/"
       "cFkJukxRzCa4OF1a7cCAXYFp9+wZDu0bw4y63qbpjhdCl8GO6Z2lkcXy7KOzbPE01ukg7+"
       "gN+7uKpoohgAhIwpAKQXmX5xtd0=",
       uint64_t{0xc9ae5c8759b4877a}},
  };

#if defined(ABSL_IS_BIG_ENDIAN)
  constexpr uint64_t kGolden[kNumGoldenOutputs] = {};
  GTEST_SKIP() << "We only maintain golden data for little endian systems.";
#else
  constexpr uint64_t kGolden[kNumGoldenOutputs] = {
      0x59b1542b0ff6b7b8, 0x3fb979d297096db9, 0xb391802c536343a9,
      0x94e0f7e4331081c4, 0x234d95e49e3ce30e, 0xca6351a3e568ed17,
      0xa62fcf7fa334293d, 0xb03111035f546067, 0x97b8c861e013d558,
      0xb6683803d9387949, 0xce5d907e0b3cb6a1, 0xab7466fae53ed201,
      0x8f13ca3f1cac3edd, 0xa2684a99cd909a2a, 0x03194f86b9440843,
      0xab3a745d96f75a66, 0xef2448606760ec3d, 0xd999e03247d5d5c5,
      0x4a25ab345d53f926, 0xa511b829ce9fc919, 0x4b76517f8e806cbf,
      0x006efd7ee09ff8d4, 0x790a4978bd0170a1, 0xc14f6e4b2dff057e,
      0xe0d2f4ae7c836d09, 0x4e2038a491ed939d, 0x23fd6f408e9598e0,
      0xa91cf8f1d92bcb08, 0x555cdec06df49d58, 0xe7d3e14bd6a8f3bd,
      0x4fdd25c1e75c009a, 0x3dffb8acf1ffbd17, 0x56946f33ed73a705,
      0x154c633d7690f3b0, 0x3e96f8e9a58a04e0, 0xb0279b244d3ccf9c,
      0x8571e87c882b2142, 0x9d9ada45132e7b41, 0xd5667655533f1dec,
      0x70607ace4ec36463, 0x691418d2eb63116c, 0xa70179d8e7142980,
      0xf8388d756bea25a7, 0xe5127c736d9826de, 0x7f1c95f9b6b656b6,
      0x66ab835b7bf4c7b3, 0xc03423b9a6db9728, 0xe88415a2b416b76d,
      0x8afd8c14d0b56c36, 0xe9a252b3ba217dad, 0x710150f5cd87a9ff,
      0xd66b147837fad9ae, 0x1af5f8ffbaa717a7, 0xe01f88d7a9a8ac17,
      0xd67870a7251fde72, 0xf32b837f845a676b, 0x0827717b1ffe59f7,
      0x80307212ca7645fb, 0xf0d22af71ea57c80, 0x459373765f2c114b,
      0x54d26109fab9cbaf, 0xc603da4e257b93db, 0x57fa334b5689d7d5,
      0x41cd1b2a8a91f620, 0xe1d6e7cd0fb015af, 0x8608e9035eb9d795,
      0x45c7b9fae739fee1, 0x9f5ae4f7a6b597ee, 0xfb771b6e0017757d,
      0x8dac6d29cfd8d027, 0x3c9ba4fb62ce6508, 0xa971fad8243844a7,
      0xd2126f49b2ea3b64, 0x5dd78fe7ac436861, 0xfe4004a6bb3494a8,
      0xe7c01cc63d770d7c, 0xa117075b8c801d37, 0xdf1dfe75f0e73069,
      0x7285b39700cefb98, 0x5e97ea1aa9a670eb, 0xe21872db2b9137a3,
      0x12630b02c6ca405e, 0xfe1f2d802151f97a, 0xb53b0ed3dea4fb02,
      0xc6d5ed56d1dbf9fd, 0xe5b92b558a5c70cb, 0xccd6eedf97277d08,
      0x08582fff2e1494ed, 0xa41f2b3d17f1c4c7, 0x29ec07e5ef950f3d,
      0x96aba32565a97084, 0xf26870eca10cebcd, 0xbe1432feb4d33361,
      0x21993a779845e6eb,
  };
#endif

#if UPDATE_GOLDEN
  (void)kGolden;  // Silence warning.
  for (size_t i = 0; i < kNumGoldenOutputs; ++i) {
    std::string str;
    ASSERT_TRUE(absl::Base64Unescape(cases[i].base64_data, &str));
    ASSERT_GT(str.size(), 32);
    uint64_t h = absl::hash_internal::LowLevelHashLenGt32(
        str.data(), str.size(), cases[i].seed, kSalt);
    printf("0x%016" PRIx64 ", ", h);
    if (i % 3 == 2) {
      printf("\n");
    }
  }
  printf("\n\n\n");
  EXPECT_FALSE(true);
#else
  for (size_t i = 0; i < kNumGoldenOutputs; ++i) {
    SCOPED_TRACE(::testing::Message()
                 << "i = " << i << "; input = " << cases[i].base64_data);
    std::string str;
    ASSERT_TRUE(absl::Base64Unescape(cases[i].base64_data, &str));
    ASSERT_GT(str.size(), 32);
    EXPECT_EQ(absl::hash_internal::LowLevelHashLenGt32(str.data(), str.size(),
                                                       cases[i].seed, kSalt),
              kGolden[i]);
  }
#endif
}

}  // namespace
