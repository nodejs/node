/*
 * Copyright 2024-2026 The OpenSSL Project Authors. All Rights Reserved.
 *
 * Licensed under the Apache License 2.0 (the "License").  You may not use
 * this file except in compliance with the License.  You can obtain a copy
 * in the file LICENSE in the source distribution or at
 * https://www.openssl.org/source/license.html
 */

#include "ml_dsa_local.h"
#include "ml_dsa_poly.h"

#if defined(OPENSSL_ML_DSA_S390X) && defined(__s390x__) && (__ARCH__ >= 12) && defined(__VX__)

#include <vecintrin.h>

#include <stdint.h>

/* Width of vector registers in bytes */
#define VECTOR_REG_WIDTH_BYTES 16
/*
 * __may_alias__ solves the undefined behavior problem in code like
 * vec_int32_t *out_vec_ptr = (vec_int32_t *)out->coeff;
 */
typedef int32_t vec_int32_t __attribute__((vector_size(VECTOR_REG_WIDTH_BYTES), __may_alias__));
typedef uint32_t vec_uint32_t __attribute__((vector_size(VECTOR_REG_WIDTH_BYTES), __may_alias__));

typedef int32_t vec_int32_alias_t __attribute__((vector_size(VECTOR_REG_WIDTH_BYTES)));
typedef uint32_t vec_uint32_alias_t __attribute__((vector_size(VECTOR_REG_WIDTH_BYTES)));

/* Our implementation of the vectorized algorithms assumes NUM_INT32_IN_VECTOR == 4. */
#define NUM_INT32_IN_VECTOR (VECTOR_REG_WIDTH_BYTES / ((int)sizeof(int32_t)))

/*
 * This file has multiple parts required for fast matrix multiplication,
 * 1) NTT (See https://eprint.iacr.org/2024/585.pdf)
 * NTT and NTT inverse transformations are Discrete Fourier Transforms in a
 * polynomial ring. Fast-Fourier Transformations can then be applied to make
 * multiplications n log(n). This uses the symmetry of the transformation to
 * reduce computations.
 *
 * 2) Montgomery multiplication
 * The multiplication of a.b mod q requires division by q which is a slow operation.
 *
 * When many multiplications mod q are required montgomery multiplication
 * can be used. This requires a number R > q such that R & q are coprime
 * (i.e. GCD(R, q) = 1), so that division happens using R instead of q.
 * If r is a power of 2 then this division can be done as a bit shift.
 *
 * Given that q = 2^23 - 2^13 + 1
 * We can chose a Montgomery multiplier of R = 2^32.
 *
 * To transform |a| into Montgomery form |m| we use
 *   m = a mod q * ((2^32)*(2^32) mod q)
 * which is then Montgomery reduced, removing the excess factor of R = 2^32.
 *
 * A good reference for optimizations around ML-DSA and Montgomery multiplication is
 * [Seiler 2018, Faster AVX2 optimized NTT multiplication for Ring-LWE lattice cryptography].
 */

/*
 * The table in FIPS 204 Appendix B uses the following formula
 * zeta[k]= 1753^bitrev(k) mod q for (k = 1..255) (The first value is not used).
 *
 * As this implementation uses montgomery form with a multiplier of 2^32.
 * The values need to be transformed i.e.
 *
 * zetasMontgomery[k] = reduce_montgomery(zeta[k] * (2^32 * 2^32 mod(q)))
 * reduce_montgomery() is defined below.
 */
static const int32_t zetas_montgomery[256] = {
    4193792, 25847, 5771523, 7861508, 237124, 7602457, 7504169, 466468,
    1826347, 2353451, 8021166, 6288512, 3119733, 5495562, 3111497, 2680103,
    2725464, 1024112, 7300517, 3585928, 7830929, 7260833, 2619752, 6271868,
    6262231, 4520680, 6980856, 5102745, 1757237, 8360995, 4010497, 280005,
    2706023, 95776, 3077325, 3530437, 6718724, 4788269, 5842901, 3915439,
    4519302, 5336701, 3574422, 5512770, 3539968, 8079950, 2348700, 7841118,
    6681150, 6736599, 3505694, 4558682, 3507263, 6239768, 6779997, 3699596,
    811944, 531354, 954230, 3881043, 3900724, 5823537, 2071892, 5582638,
    4450022, 6851714, 4702672, 5339162, 6927966, 3475950, 2176455, 6795196,
    7122806, 1939314, 4296819, 7380215, 5190273, 5223087, 4747489, 126922,
    3412210, 7396998, 2147896, 2715295, 5412772, 4686924, 7969390, 5903370,
    7709315, 7151892, 8357436, 7072248, 7998430, 1349076, 1852771, 6949987,
    5037034, 264944, 508951, 3097992, 44288, 7280319, 904516, 3958618,
    4656075, 8371839, 1653064, 5130689, 2389356, 8169440, 759969, 7063561,
    189548, 4827145, 3159746, 6529015, 5971092, 8202977, 1315589, 1341330,
    1285669, 6795489, 7567685, 6940675, 5361315, 4499357, 4751448, 3839961,
    2091667, 3407706, 2316500, 3817976, 5037939, 2244091, 5933984, 4817955,
    266997, 2434439, 7144689, 3513181, 4860065, 4621053, 7183191, 5187039,
    900702, 1859098, 909542, 819034, 495491, 6767243, 8337157, 7857917,
    7725090, 5257975, 2031748, 3207046, 4823422, 7855319, 7611795, 4784579,
    342297, 286988, 5942594, 4108315, 3437287, 5038140, 1735879, 203044,
    2842341, 2691481, 5790267, 1265009, 4055324, 1247620, 2486353, 1595974,
    4613401, 1250494, 2635921, 4832145, 5386378, 1869119, 1903435, 7329447,
    7047359, 1237275, 5062207, 6950192, 7929317, 1312455, 3306115, 6417775,
    7100756, 1917081, 5834105, 7005614, 1500165, 777191, 2235880, 3406031,
    7838005, 5548557, 6709241, 6533464, 5796124, 4656147, 594136, 4603424,
    6366809, 2432395, 2454455, 8215696, 1957272, 3369112, 185531, 7173032,
    5196991, 162844, 1616392, 3014001, 810149, 1652634, 4686184, 6581310,
    5341501, 3523897, 3866901, 269760, 2213111, 7404533, 1717735, 472078,
    7953734, 1723600, 6577327, 1910376, 6712985, 7276084, 8119771, 4546524,
    5441381, 6144432, 7959518, 6094090, 183443, 7403526, 1612842, 4834730,
    7826001, 3919660, 8332111, 7018208, 3937738, 1400424, 7534263, 1976782
};

/* clang-format off */
static const int32_t zetas_montgomery_twisted[256] = {
        -512,   1830765815,  -1929875197,  -1927777020,
  1640767044,   1477910809,   1612161321,   1640734244,
   308362795,  -1815525077,  -1374673746,  -1091570560,
 -1929495947,    515185418,   -285697463,    625853735,
  1727305304,   2082316400,  -1364982363,    858240904,
  1806278033,    222489249,   -346752664,    684667772,
  1654287831,   -878576920,  -1257667336,   -748618599,
   329347125,   1837364259,  -1443016191,  -1170414139,
 -1846138265,  -1631226336,  -1404529459,   1838055109,
  1594295556,  -1076973523,  -1898723371,   -594436433,
  -202001018,   -475984259,   -561427818,   1797021250,
 -1061813248,   2059733582,  -1661512036,  -1104976546,
 -1750224322,   -901666089,    418987550,   1831915354,
 -1925356481,    992097816,    879957085,   2024403852,
  1484874664,  -1636082790,   -285388938,  -1983539117,
 -1495136972,   -950076367,  -1714807468,   -952438994,
 -1574918426,   -654783358,   1350681040,  -1974159334,
 -2143979938,   1651689966,   1599739335,    140455868,
 -1285853322,  -1039411342,   -993005453,   1955560695,
 -1440787839,   1529189039,    568627425,  -2131021878,
  -783134478,   -247357818,   -588790216,   1518161567,
   289871780,    -86965172,  -1262003602,   1708872714,
  2135294595,   1787797780,  -1018755524,   1638590968,
  -889861154,   -120646188,   1665705315,  -1669960605,
  1321868266,   -916321552,   1225434135,   1155548552,
 -1784632064,   2143745727,    666258756,   1210558298,
   675310539,  -1261461889,  -1555941048,   -318346815,
 -1999506068,    628664288,  -1499481951,  -1729304567,
  -695180180,   1422575625,  -1375177022,   1424130039,
  1777179796,  -1185330463,    334803717,    235321234,
  -178766299,    168022241,   -518252219,   1206536195,
  1957047971,    985155485,   1146323032,   -894060583,
     -898413,    991903578,   1363007700,    746144248,
 -1363460237,    912367099,     30313376,  -1420958685,
  -605900043,    -44694137,   -326425359,   2032221021,
  2027833505,   1176904445,   1683520343,   1904936415,
    14253662,   -421552614,   -517299994,   1257750362,
  1014493059,   -818371957,   2027935493,   1926727421,
   863641634,   1747917559,  -1372618620,   1931587462,
  1819892094,   -325927721,    128353683,   1258381763,
  2124962073,    908452108,  -1123881662,    885133339,
 -1223601433,   1851023420,    137583815,   1629985060,
 -1920467227,  -1176751719,   -635454917,   1967222129,
 -1637785316,  -1354528380,   -642772911,      6363718,
 -1536588519,    -72690498,     45766801,  -1287922799,
   694382730,   -314284737,    671509323,   1136965287,
   235104447,    985022747,  -2070602177,   1779436848,
 -1045062171,    963438279,    419615363,   1116720495,
   831969620,  -1078959975,   1216882041,   1042326958,
  -300448763,    604552167,   -270590488,   1405999311,
   756955445,  -1021949427,  -1276805127,    713994584,
  -260312804,    608791571,    371462360,    940195360,
  1554794073,    173440395,  -1357098057,  -1542497136,
  1339088280,  -2126092136,   -384158533,   2061661096,
 -2040058689,  -1316619236,    827959816,   -883155599,
  -853476187,  -1039370342,   -596344472,   1726753854,
 -2047270595,      6087993,    702390549,  -1547952704,
 -1723816713,   -110126091,   -279505433,    394851342,
 -1591599802,    565464272,   -260424529,    283780712,
  -440824167,  -1758099916,    -71875109,    776003548,
  1119856485,  -1600929360,  -1208667170,   1123958026,
  1544891539,    879867910,  -1499603926,    201262506,
   155290193,  -1809756372,   2036925263,   1934038752,
  -973777462,    400711272,   -540420425,    374860238
};

static const int32_t neg_zetas_montgomery[256] = {
     4186625,      8354570,      2608894,       518909,
     8143293,       777960,       876248,      7913949,
     6554070,      6026966,       359251,      2091905,
     5260684,      2884855,      5268920,      5700314,
     5654953,      7356305,      1079900,      4794489,
      549488,      1119584,      5760665,      2108549,
     2118186,      3859737,      1399561,      3277672,
     6623180,        19422,      4369920,      8100412,
     5674394,      8284641,      5303092,      4849980,
     1661693,      3592148,      2537516,      4464978,
     3861115,      3043716,      4805995,      2867647,
     4840449,       300467,      6031717,       539299,
     1699267,      1643818,      4874723,      3821735,
     4873154,      2140649,      1600420,      4680821,
     7568473,      7849063,      7426187,      4499374,
     4479693,      2556880,      6308525,      2797779,
     3930395,      1528703,      3677745,      3041255,
     1452451,      4904467,      6203962,      1585221,
     1257611,      6441103,      4083598,      1000202,
     3190144,      3157330,      3632928,      8253495,
     4968207,       983419,      6232521,      5665122,
     2967645,      3693493,       411027,      2477047,
      671102,      1228525,        22981,      1308169,
      381987,      7031341,      6527646,      1430430,
     3343383,      8115473,      7871466,      5282425,
     8336129,      1100098,      7475901,      4421799,
     3724342,         8578,      6727353,      3249728,
     5991061,       210977,      7620448,      1316856,
     8190869,      3553272,      5220671,      1851402,
     2409325,       177440,      7064828,      7039087,
     7094748,      1584928,       812732,      1439742,
     3019102,      3881060,      3628969,      4540456,
     6288750,      4972711,      6063917,      4562441,
     3342478,      6136326,      2446433,      3562462,
     8113420,      5945978,      1235728,      4867236,
     3520352,      3759364,      1197226,      3193378,
     7479715,      6521319,      7470875,      7561383,
     7884926,      1613174,        43260,       522500,
      655327,      3122442,      6348669,      5173371,
     3556995,       525098,       768622,      3595838,
     8038120,      8093429,      2437823,      4272102,
     4943130,      3342277,      6644538,      8177373,
     5538076,      5688936,      2590150,      7115408,
     4325093,      7132797,      5894064,      6784443,
     3767016,      7129923,      5744496,      3548272,
     2994039,      6511298,      6476982,      1050970,
     1333058,      7143142,      3318210,      1430225,
      451100,      7067962,      5074302,      1962642,
     1279661,      6463336,      2546312,      1374803,
     6880252,      7603226,      6144537,      4974386,
      542412,      2831860,      1671176,      1846953,
     2584293,      3724270,      7786281,      3776993,
     2013608,      5948022,      5925962,       164721,
     6423145,      5011305,      8194886,      1207385,
     3183426,      8217573,      6764025,      5366416,
     7570268,      6727783,      3694233,      1799107,
     3038916,      4856520,      4513516,      8110657,
     6167306,       975884,      6662682,      7908339,
      426683,      6656817,      1803090,      6470041,
     1667432,      1104333,       260646,      3833893,
     2939036,      2235985,       420899,      2286327,
     8196974,       976891,      6767575,      3545687,
      554416,      4460757,        48306,      1362209,
     4442679,      6979993,       846154,      6403635
};

static const int32_t neg_zetas_montgomery_twisted[256] = {
         513,  -1830765814,   1929875198,   1927777021,
 -1640767043,  -1477910808,  -1612161320,  -1640734243,
  -308362794,   1815525078,   1374673747,   1091570561,
  1929495948,   -515185417,    285697464,   -625853734,
 -1727305303,  -2082316399,   1364982364,   -858240903,
 -1806278032,   -222489248,    346752665,   -684667771,
 -1654287830,    878576921,   1257667337,    748618600,
  -329347124,  -1837364258,   1443016192,   1170414140,
  1846138266,   1631226337,   1404529460,  -1838055108,
 -1594295555,   1076973524,   1898723372,    594436434,
   202001019,    475984260,    561427819,  -1797021249,
  1061813249,  -2059733581,   1661512037,   1104976547,
  1750224323,    901666090,   -418987549,  -1831915353,
  1925356482,   -992097815,   -879957084,  -2024403851,
 -1484874663,   1636082791,    285388939,   1983539118,
  1495136973,    950076368,   1714807469,    952438995,
  1574918427,    654783359,  -1350681039,   1974159335,
  2143979939,  -1651689965,  -1599739334,   -140455867,
  1285853323,   1039411343,    993005454,  -1955560694,
  1440787840,  -1529189038,   -568627424,   2131021879,
   783134479,    247357819,    588790217,  -1518161566,
  -289871779,     86965173,   1262003603,  -1708872713,
 -2135294594,  -1787797779,   1018755525,  -1638590967,
   889861155,    120646189,  -1665705314,   1669960606,
 -1321868265,    916321553,  -1225434134,  -1155548551,
  1784632065,  -2143745726,   -666258755,  -1210558297,
  -675310538,   1261461890,   1555941049,    318346816,
  1999506069,   -628664287,   1499481952,   1729304568,
   695180181,  -1422575624,   1375177023,  -1424130038,
 -1777179795,   1185330464,   -334803716,   -235321233,
   178766300,   -168022240,    518252220,  -1206536194,
 -1957047970,   -985155484,  -1146323031,    894060584,
      898414,   -991903577,  -1363007699,   -746144247,
  1363460238,   -912367098,    -30313375,   1420958686,
   605900044,     44694138,    326425360,  -2032221020,
 -2027833504,  -1176904444,  -1683520342,  -1904936414,
   -14253661,    421552615,    517299995,  -1257750361,
 -1014493058,    818371958,  -2027935492,  -1926727420,
  -863641633,  -1747917558,   1372618621,  -1931587461,
 -1819892093,    325927722,   -128353682,  -1258381762,
 -2124962072,   -908452107,   1123881663,   -885133338,
  1223601434,  -1851023419,   -137583814,  -1629985059,
  1920467228,   1176751720,    635454918,  -1967222128,
  1637785317,   1354528381,    642772912,     -6363717,
  1536588520,     72690499,    -45766800,   1287922800,
  -694382729,    314284738,   -671509322,  -1136965286,
  -235104446,   -985022746,   2070602178,  -1779436847,
  1045062172,   -963438278,   -419615362,  -1116720494,
  -831969619,   1078959976,  -1216882040,  -1042326957,
   300448764,   -604552166,    270590489,  -1405999310,
  -756955444,   1021949428,   1276805128,   -713994583,
   260312805,   -608791570,   -371462359,   -940195359,
 -1554794072,   -173440394,   1357098058,   1542497137,
 -1339088279,   2126092137,    384158534,  -2061661095,
  2040058690,   1316619237,   -827959815,    883155600,
   853476188,   1039370343,    596344473,  -1726753853,
  2047270596,     -6087992,   -702390548,   1547952705,
  1723816714,    110126092,    279505434,   -394851341,
  1591599803,   -565464271,    260424530,   -283780711,
   440824168,   1758099917,     71875110,   -776003547,
 -1119856484,   1600929361,   1208667171,  -1123958025,
 -1544891538,   -879867909,   1499603927,   -201262505,
  -155290192,   1809756373,  -2036925262,  -1934038751,
   973777463,   -400711271,    540420426,   -374860237
};
/* clang-format on */

static const vec_int32_t vec_q = { ML_DSA_Q, ML_DSA_Q, ML_DSA_Q, ML_DSA_Q };
static const vec_int32_t vec_q_inv = { ML_DSA_Q_INV, ML_DSA_Q_INV, ML_DSA_Q_INV, ML_DSA_Q_INV };

/*
 * @brief Reduce a in (-q, q) to a mod q in [0, q).
 *
 * @param a in (-q, q)
 * @returns a mod q in [0, q)
 */
static ossl_inline
    vec_int32_t
    reduce_once_signed(vec_int32_t a)
{
    /* mask is 11..11 when a is negative, else 0 */
    vec_uint32_t mask = -(((vec_uint32_t)a) >> 31);
    return a + (vec_int32_t)(mask & (vec_uint32_t)vec_q);
}

/*
 * @brief Reduce a in (-2q, q) to a mod q in [0, q).
 *
 * @param a in (-2q, q)
 * @returns a mod q in [0, q)
 */
static ossl_inline
    vec_int32_t
    reduce_twice_signed(vec_int32_t a)
{
    /* mask is 11..11 when a is negative, else 0 */
    vec_uint32_t mask = -(((vec_uint32_t)a) >> 31);
    /* b is in (-q, q) */
    vec_int32_t b = a + (vec_int32_t)(mask & (vec_uint32_t)vec_q);
    return reduce_once_signed(b);
}

/*
 * @brief Computes the Montgomery product of a and b.
 *        See [Seiler 2018, Algorithm 3].
 *
 * @param a is the first factor, assumed to be in [0, q).
 * @param a_twist is (int32)((uint32)a * ML_DSA_Q_INV).
 * @param b is the second factor.
 * @returns The Montgomery product of a and b in the range
 *          [0, q).
 */

static ossl_inline
    vec_int32_t
    montgomery_multiplication_vectorized(vec_int32_t a, vec_int32_t a_twist, vec_int32_t b)
{
    vec_uint32_t k = (vec_uint32_t)a_twist * (vec_uint32_t)b;
    vec_uint32_t c_u = vec_mulh((vec_uint32_alias_t)k, (vec_uint32_alias_t)vec_q);
    vec_int32_t c = (vec_int32_t)c_u;
    vec_int32_t z_high = vec_mulh((vec_int32_alias_t)a, (vec_int32_alias_t)b);
    vec_int32_t r = z_high - c;
    return reduce_twice_signed(r);
}

/*
 * @brief Reduce modulo q to an non-negative vector.
 *        Note that the constant v_scalar equals
 *        floor(2**(floor(log_2(q))-1 * 2**32/q)).
 *
 * @param a in the range -2**31..2**31-1
 * @returns a mod q in the range 0..q-1
 */
static ossl_inline
    vec_int32_t
    reduce_fully(vec_int32_t a)
{
    const int32_t v_scalar = 1074791296;
    const vec_int32_alias_t v = { v_scalar, v_scalar, v_scalar, v_scalar };
    vec_int32_t t = vec_mulh((vec_int32_alias_t)a, v) >> 21;
    t *= ML_DSA_Q;
    vec_int32_t r = a - t; /* in [0, q] */
    return reduce_once_signed(r);
}

void ossl_poly_ntt_mult_scalar_vec128(const POLY *lhs, const POLY *rhs, POLY *out)
{
    int i;
    const vec_int32_t *lhs_vec_ptr = (const vec_int32_t *)lhs->coeff;
    const vec_int32_t *rhs_vec_ptr = (const vec_int32_t *)rhs->coeff;
    vec_int32_t *out_vec_ptr = (vec_int32_t *)out->coeff;

    for (i = 0; i < ML_DSA_NUM_POLY_COEFFICIENTS / NUM_INT32_IN_VECTOR; i++) {
        vec_int32_t twist_vec = (vec_int32_t)((vec_uint32_t)lhs_vec_ptr[i] * (vec_uint32_t)vec_q_inv);
        out_vec_ptr[i] = montgomery_multiplication_vectorized(
            lhs_vec_ptr[i], twist_vec, rhs_vec_ptr[i]);
    }
}

/*
 * In place number theoretic transform of a given polynomial.
 *
 * See FIPS 204, Algorithm 41, NTT()
 * This function uses montgomery multiplication.
 *
 * @param p a polynomial that is used as the input, that is replaced with
 *        the NTT of the polynomial
 */
void ossl_ml_dsa_poly_ntt_vec128(POLY *p)
{
    int i, j, k;
    int step;
    int offset = ML_DSA_NUM_POLY_COEFFICIENTS;
    vec_int32_t *p_vec = (vec_int32_t *)p->coeff;

    /* Step: 1, 2, 4, 8, ..., 32 */
    for (step = 1; step < ML_DSA_NUM_POLY_COEFFICIENTS / 4; step <<= 1) {
        k = 0;
        offset >>= 1; /* Offset: 128, 64, 32, 16, ..., 4 */

        for (i = 0; i < step; i++) {
            const vec_int32_t zeta = { zetas_montgomery[step + i],
                zetas_montgomery[step + i],
                zetas_montgomery[step + i],
                zetas_montgomery[step + i] };
            const vec_int32_t zeta_twisted = { zetas_montgomery_twisted[step + i],
                zetas_montgomery_twisted[step + i],
                zetas_montgomery_twisted[step + i],
                zetas_montgomery_twisted[step + i] };

            for (j = k; j < k + offset; j += NUM_INT32_IN_VECTOR) {
                vec_int32_t w_even_vec = p_vec[j / NUM_INT32_IN_VECTOR];
                vec_int32_t w_odd_vec = p_vec[(j + offset) / NUM_INT32_IN_VECTOR];
                vec_int32_t t_odd_vec = montgomery_multiplication_vectorized(
                    zeta,
                    zeta_twisted,
                    w_odd_vec);
                vec_int32_t coeff_j_vec = (w_even_vec + t_odd_vec);
                vec_int32_t coeff_j_offset_vec = (w_even_vec - t_odd_vec);
                p_vec[j / NUM_INT32_IN_VECTOR] = coeff_j_vec;
                p_vec[(j + offset) / NUM_INT32_IN_VECTOR] = coeff_j_offset_vec;
            }
            k += 2 * offset;
        }
    }

    /* offset == 2*/
    k = 0;
    step = 64;
    offset = 2;
    for (j = 0; j < ML_DSA_NUM_POLY_COEFFICIENTS; j += 2 * NUM_INT32_IN_VECTOR) {
        const vec_int32_t zeta = {
            zetas_montgomery[step + j / NUM_INT32_IN_VECTOR],
            zetas_montgomery[step + j / NUM_INT32_IN_VECTOR],
            zetas_montgomery[step + j / NUM_INT32_IN_VECTOR + 1],
            zetas_montgomery[step + j / NUM_INT32_IN_VECTOR + 1]
        };
        const vec_int32_t zeta_twisted = {
            zetas_montgomery_twisted[step + j / NUM_INT32_IN_VECTOR],
            zetas_montgomery_twisted[step + j / NUM_INT32_IN_VECTOR],
            zetas_montgomery_twisted[step + j / NUM_INT32_IN_VECTOR + 1],
            zetas_montgomery_twisted[step + j / NUM_INT32_IN_VECTOR + 1]
        };

        vec_int32_t w_even_vec = {
            p_vec[j / NUM_INT32_IN_VECTOR][0],
            p_vec[j / NUM_INT32_IN_VECTOR][1],
            p_vec[j / NUM_INT32_IN_VECTOR + 1][0],
            p_vec[j / NUM_INT32_IN_VECTOR + 1][1]
        };
        vec_int32_t w_odd_vec = {
            p_vec[j / NUM_INT32_IN_VECTOR][2],
            p_vec[j / NUM_INT32_IN_VECTOR][3],
            p_vec[j / NUM_INT32_IN_VECTOR + 1][2],
            p_vec[j / NUM_INT32_IN_VECTOR + 1][3]
        };
        vec_int32_t t_odd_vec = montgomery_multiplication_vectorized(
            zeta,
            zeta_twisted,
            w_odd_vec);
        vec_int32_t coeff_j_vec = (w_even_vec + t_odd_vec);
        vec_int32_t coeff_j_offset_vec = (w_even_vec - t_odd_vec);
        p_vec[j / NUM_INT32_IN_VECTOR] = (vec_int32_t) {
            coeff_j_vec[0],
            coeff_j_vec[1],
            coeff_j_offset_vec[0],
            coeff_j_offset_vec[1]
        };
        p_vec[j / NUM_INT32_IN_VECTOR + 1] = (vec_int32_t) {
            coeff_j_vec[2],
            coeff_j_vec[3],
            coeff_j_offset_vec[2],
            coeff_j_offset_vec[3]
        };
    }

    /* offset == 1 */
    k = 0;
    step = 128;
    for (i = 0; i < step; i += NUM_INT32_IN_VECTOR) {
        const vec_int32_t zeta = {
            zetas_montgomery[step + i],
            zetas_montgomery[step + i + 1],
            zetas_montgomery[step + i + 2],
            zetas_montgomery[step + i + 3]
        };
        const vec_int32_t zeta_twisted = {
            zetas_montgomery_twisted[step + i],
            zetas_montgomery_twisted[step + i + 1],
            zetas_montgomery_twisted[step + i + 2],
            zetas_montgomery_twisted[step + i + 3]
        };

        vec_int32_t w_even_vec = {
            p_vec[k / NUM_INT32_IN_VECTOR][0],
            p_vec[k / NUM_INT32_IN_VECTOR][2],
            p_vec[k / NUM_INT32_IN_VECTOR + 1][0],
            p_vec[k / NUM_INT32_IN_VECTOR + 1][2]
        };
        vec_int32_t w_odd_vec = {
            p_vec[k / NUM_INT32_IN_VECTOR][1],
            p_vec[k / NUM_INT32_IN_VECTOR][3],
            p_vec[k / NUM_INT32_IN_VECTOR + 1][1],
            p_vec[k / NUM_INT32_IN_VECTOR + 1][3]
        };
        vec_int32_t t_odd_vec = montgomery_multiplication_vectorized(
            zeta,
            zeta_twisted,
            w_odd_vec);
        vec_int32_t coeff_j_vec = reduce_fully(w_even_vec + t_odd_vec);
        vec_int32_t coeff_j_offset_vec = reduce_fully(w_even_vec - t_odd_vec);

        p->coeff[k] = coeff_j_vec[0];
        p->coeff[k + 2] = coeff_j_vec[1];
        p->coeff[k + 4] = coeff_j_vec[2];
        p->coeff[k + 6] = coeff_j_vec[3];
        p->coeff[k + 1] = coeff_j_offset_vec[0];
        p->coeff[k + 2 + 1] = coeff_j_offset_vec[1];
        p->coeff[k + 4 + 1] = coeff_j_offset_vec[2];
        p->coeff[k + 6 + 1] = coeff_j_offset_vec[3];

        k += 2 * NUM_INT32_IN_VECTOR;
    }
}

/*
 * @brief In place inverse number theoretic transform of a given polynomial.
 * See FIPS 204, Algorithm 42,  NTT^-1()
 *
 * @param p a polynomial that is used as the input, that is overwritten with
 *          the inverse of the NTT.
 */
void ossl_ml_dsa_poly_ntt_inverse_vec128(POLY *p)
{
    /*
     * Step: 128, 64, 32, 16, ..., 1
     * Offset: 1, 2, 4, 8, ..., 128
     */
    int i, j, k, offset, step = ML_DSA_NUM_POLY_COEFFICIENTS;
    /*
     * The multiplicative inverse of 256 mod q, in Montgomery form is
     * ((256^-1 mod q) * ((2^32 * 2^32) mod q)) mod q = (8347681 * 2365951) mod 8380417
     */
    static const int32_t inverse_degree_montgomery = 41978;
    static const vec_int32_t vec_inverse_degree_montgomery = {
        inverse_degree_montgomery,
        inverse_degree_montgomery,
        inverse_degree_montgomery,
        inverse_degree_montgomery
    };
    static const int32_t inverse_degree_montgomery_twisted = -8395782;
    static const vec_int32_t vec_inverse_degree_montgomery_twisted = {
        inverse_degree_montgomery_twisted,
        inverse_degree_montgomery_twisted,
        inverse_degree_montgomery_twisted,
        inverse_degree_montgomery_twisted
    };

    vec_int32_t *p_vec = (vec_int32_t *)p->coeff;

    offset = 1;
    step >>= 1;
    k = 0;

    for (i = 0; i < step; i += NUM_INT32_IN_VECTOR) {
        /* offset == 1*/
        const vec_int32_t zeta = { neg_zetas_montgomery[step + (step - 1 - i)],
            neg_zetas_montgomery[step + (step - 1 - i - 1)],
            neg_zetas_montgomery[step + (step - 1 - i - 2)],
            neg_zetas_montgomery[step + (step - 1 - i - 3)] };
        const vec_int32_t zeta_twisted = { neg_zetas_montgomery_twisted[step + (step - 1 - i)],
            neg_zetas_montgomery_twisted[step + (step - 1 - i - 1)],
            neg_zetas_montgomery_twisted[step + (step - 1 - i - 2)],
            neg_zetas_montgomery_twisted[step + (step - 1 - i - 3)] };
        vec_int32_t even = { p->coeff[k],
            p->coeff[k + 2],
            p->coeff[k + 4],
            p->coeff[k + 6] };
        vec_int32_t odd = { p->coeff[k + 1],
            p->coeff[k + 1 + 2],
            p->coeff[k + 1 + 4],
            p->coeff[k + 1 + 6] };

        vec_int32_t coeff_j = (odd + even);
        vec_int32_t coeff_j_offset = montgomery_multiplication_vectorized(
            zeta,
            zeta_twisted,
            even - odd);

        p->coeff[k + 0] = coeff_j[0];
        p->coeff[k + 2] = coeff_j[1];
        p->coeff[k + 4] = coeff_j[2];
        p->coeff[k + 6] = coeff_j[3];
        p->coeff[k + 1 + 0] = coeff_j_offset[0];
        p->coeff[k + 1 + 2] = coeff_j_offset[1];
        p->coeff[k + 1 + 4] = coeff_j_offset[2];
        p->coeff[k + 1 + 6] = coeff_j_offset[3];

        k += 2 * NUM_INT32_IN_VECTOR;
    }

    /* offset == 2 */
    offset <<= 1;
    step >>= 1;
    k = 0;

    for (i = 0; i < step; i += 2) {
        const vec_int32_t zeta = { neg_zetas_montgomery[step + (step - 1 - i)],
            neg_zetas_montgomery[step + (step - 1 - i)],
            neg_zetas_montgomery[step + (step - 1 - i - 1)],
            neg_zetas_montgomery[step + (step - 1 - i - 1)] };
        const vec_int32_t zeta_twisted = { neg_zetas_montgomery_twisted[step + (step - 1 - i)],
            neg_zetas_montgomery_twisted[step + (step - 1 - i)],
            neg_zetas_montgomery_twisted[step + (step - 1 - i - 1)],
            neg_zetas_montgomery_twisted[step + (step - 1 - i - 1)] };

        j = k;
        vec_int32_t even = { p->coeff[j],
            p->coeff[j + 1],
            p->coeff[j + 4],
            p->coeff[j + 5] };
        vec_int32_t odd = { p->coeff[j + 2],
            p->coeff[j + 3],
            p->coeff[j + 6],
            p->coeff[j + 7] };

        vec_int32_t coeff_j = (odd + even);
        vec_int32_t coeff_j_offset = montgomery_multiplication_vectorized(
            zeta,
            zeta_twisted,
            even - odd);

        p_vec[j / NUM_INT32_IN_VECTOR] = (vec_int32_t) {
            coeff_j[0],
            coeff_j[1],
            coeff_j_offset[0],
            coeff_j_offset[1]
        };
        p_vec[j / NUM_INT32_IN_VECTOR + 1] = (vec_int32_t) {
            coeff_j[2],
            coeff_j[3],
            coeff_j_offset[2],
            coeff_j_offset[3]
        };
        k += 2 * 2 * offset;
    }

    /* offset >= 4 */
    for (offset <<= 1; offset < ML_DSA_NUM_POLY_COEFFICIENTS; offset <<= 1) {
        step >>= 1;
        k = 0;

        for (i = 0; i < step; i++) {
            const vec_int32_t zeta = { neg_zetas_montgomery[step + (step - 1 - i)],
                neg_zetas_montgomery[step + (step - 1 - i)],
                neg_zetas_montgomery[step + (step - 1 - i)],
                neg_zetas_montgomery[step + (step - 1 - i)] };
            const vec_int32_t zeta_twisted = { neg_zetas_montgomery_twisted[step + (step - 1 - i)],
                neg_zetas_montgomery_twisted[step + (step - 1 - i)],
                neg_zetas_montgomery_twisted[step + (step - 1 - i)],
                neg_zetas_montgomery_twisted[step + (step - 1 - i)] };

            for (j = k; j < k + offset; j += NUM_INT32_IN_VECTOR) {
                vec_int32_t even = p_vec[j / NUM_INT32_IN_VECTOR];
                vec_int32_t odd = p_vec[(j + offset) / NUM_INT32_IN_VECTOR];

                vec_int32_t coeff_j = (odd + even);
                vec_int32_t coeff_j_offset = montgomery_multiplication_vectorized(
                    zeta,
                    zeta_twisted,
                    even - odd);
                p_vec[j / NUM_INT32_IN_VECTOR] = coeff_j;
                p_vec[(j + offset) / NUM_INT32_IN_VECTOR] = coeff_j_offset;
            }
            k += 2 * offset;
        }
    }

    for (i = 0; i < ML_DSA_NUM_POLY_COEFFICIENTS / NUM_INT32_IN_VECTOR; i += 1) {
        p_vec[i] = montgomery_multiplication_vectorized(
            vec_inverse_degree_montgomery,
            vec_inverse_degree_montgomery_twisted,
            p_vec[i]);
    }
}

#endif
