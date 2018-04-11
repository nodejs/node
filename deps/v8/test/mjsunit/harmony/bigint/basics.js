// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --harmony-bigint

'use strict'

const minus_one = BigInt(-1);
const zero = BigInt(0);
const another_zero = BigInt(0);
const one = BigInt(1);
const another_one = BigInt(1);
const two = BigInt(2);
const three = BigInt(3);
const six = BigInt(6);

// BigInt
{
  assertSame(BigInt, BigInt.prototype.constructor)
}{
  assertThrows(() => new BigInt, TypeError);
  assertThrows(() => new BigInt(), TypeError);
  assertThrows(() => new BigInt(0), TypeError);
  assertThrows(() => new BigInt(0n), TypeError);
  assertThrows(() => new BigInt("0"), TypeError);
}{
  class C extends BigInt { constructor() { throw 42 } };
  assertThrowsEquals(() => new C, 42);
}

// ToBigInt, NumberToBigInt, BigInt
{
  assertThrows(() => BigInt(undefined), TypeError);
  assertThrows(() => BigInt(null), TypeError);
  assertThrows(() => BigInt({}), SyntaxError);
  assertThrows(() => BigInt("foo"), SyntaxError);

  assertThrows(() => BigInt("1j"), SyntaxError);
  assertThrows(() => BigInt("0b1ju"), SyntaxError);
  assertThrows(() => BigInt("0o1jun"), SyntaxError);
  assertThrows(() => BigInt("0x1junk"), SyntaxError);
}{
  assertSame(BigInt(true), 1n);
  assertSame(BigInt(false), 0n);
  assertSame(BigInt(""), 0n);
  assertSame(BigInt(" 42"), 42n);
  assertSame(BigInt("0b101010"), 42n);
  assertSame(BigInt("  0b101011"), 43n);
  assertSame(BigInt("0x2a  "), 42n);
  assertSame(BigInt("    0x2b"), 43n);
  assertSame(BigInt("0o52"), 42n);
  assertSame(BigInt("     0o53\n"), 43n);
  assertSame(BigInt(-0), 0n);
  assertSame(BigInt(42), 42n);
  assertSame(BigInt(42n), 42n);
  assertSame(BigInt(Object(42n)), 42n);
  assertSame(BigInt(2**53 - 1), 9007199254740991n);
  assertSame(BigInt(Object(2**53 - 1)), 9007199254740991n);
  assertSame(BigInt([]), 0n);
}{
  assertThrows(() => BigInt(NaN), RangeError);
  assertThrows(() => BigInt(-Infinity), RangeError);
  assertThrows(() => BigInt(+Infinity), RangeError);
  assertThrows(() => BigInt(4.00000001), RangeError);
  assertThrows(() => BigInt(Object(4.00000001)), RangeError);
  assertThrows(() => BigInt(2**53), RangeError);
  assertThrows(() => BigInt(2**1000), RangeError);
}

// BigInt.prototype[Symbol.toStringTag]
{
  const toStringTag = Object.getOwnPropertyDescriptor(
      BigInt.prototype, Symbol.toStringTag);
  assertTrue(toStringTag.configurable);
  assertFalse(toStringTag.enumerable);
  assertFalse(toStringTag.writable);
  assertEquals("BigInt", toStringTag.value);
}

// Object.prototype.toString
{
  const toString = Object.prototype.toString;

  assertEquals("[object BigInt]", toString.call(42n));
  assertEquals("[object BigInt]", toString.call(Object(42n)));

  delete BigInt.prototype[Symbol.toStringTag];
  assertEquals("[object Object]", toString.call(42n));
  assertEquals("[object Object]", toString.call(Object(42n)));

  BigInt.prototype[Symbol.toStringTag] = "foo";
  assertEquals("[object foo]", toString.call(42n));
  assertEquals("[object foo]", toString.call(Object(42n)));
}

// typeof
{
  assertEquals(typeof zero, "bigint");
  assertEquals(typeof one, "bigint");
}{
  assertEquals(%Typeof(zero), "bigint");
  assertEquals(%Typeof(one), "bigint");
}{
  assertTrue(typeof 1n === "bigint");
  assertFalse(typeof 1n === "BigInt");
  assertFalse(typeof 1 === "bigint");
}

// ToString
{
  assertEquals(String(zero), "0");
  assertEquals(String(one), "1");
}

// .toString(radix)
{
  // Single-digit BigInts: random-generated inputs close to kMaxInt.
  // Expectations computed with the following Python program:
  //   def Format(x, base):
  //     s = ""
  //     while x > 0:
  //       s = "0123456789abcdefghijklmnopqrstuvwxyz"[x % base] + s
  //       x = x / base
  //     return s
  assertEquals("10100110000100101000011100101", BigInt(0x14c250e5).toString(2));
  assertEquals("-110110100010011111001011111", BigInt(-0x6d13e5f).toString(2));
  assertEquals("1001222020000100000", BigInt(0x18c72873).toString(3));
  assertEquals("-1212101122110102020", BigInt(-0x2b19aebe).toString(3));
  assertEquals("120303133110120", BigInt(0x18cdf518).toString(4));
  assertEquals("-113203101020122", BigInt(-0x178d121a).toString(4));
  assertEquals("1323302233400", BigInt(0x18de6256).toString(5));
  assertEquals("-2301033210212", BigInt(-0x25f7f454).toString(5));
  assertEquals("131050115130", BigInt(0x211f0d5e).toString(6));
  assertEquals("-104353333321", BigInt(-0x186bbe91).toString(6));
  assertEquals("25466260221", BigInt(0x2f69f47e).toString(7));
  assertEquals("-31051540346", BigInt(-0x352c7efa).toString(7));
  assertEquals("5004630525", BigInt(0x28133155).toString(8));
  assertEquals("-7633240703", BigInt(-0x3e6d41c3).toString(8));
  assertEquals("705082365", BigInt(0x121f4264).toString(9));
  assertEquals("-780654431", BigInt(-0x1443b36e).toString(9));
  assertEquals("297019028", BigInt(0x11b42694).toString(10));
  assertEquals("-721151126", BigInt(-0x2afbe496).toString(10));
  assertEquals("312914074", BigInt(0x27ca6879).toString(11));
  assertEquals("-198025592", BigInt(-0x1813d3a7).toString(11));
  assertEquals("191370997", BigInt(0x2d14f083).toString(12));
  assertEquals("-1b8aab4a2", BigInt(-0x32b52efa).toString(12));
  assertEquals("7818062c", BigInt(0x1c84a48c).toString(13));
  assertEquals("-7529695b", BigInt(-0x1badffee).toString(13));
  assertEquals("6bc929c4", BigInt(0x2b0a91d0).toString(14));
  assertEquals("-63042008", BigInt(-0x270dff78).toString(14));
  assertEquals("5e8b8dec", BigInt(0x3cd27d7f).toString(15));
  assertEquals("-4005433d", BigInt(-0x28c0821a).toString(15));
  assertEquals("10b35ca3", BigInt(0x10b35ca3).toString(16));
  assertEquals("-23d4d9d6", BigInt(-0x23d4d9d6).toString(16));
  assertEquals("28c3d5e3", BigInt(0x3d75d48c).toString(17));
  assertEquals("-10c06328", BigInt(-0x1979b7f0).toString(17));
  assertEquals("eb8d349", BigInt(0x1dacf0a5).toString(18));
  assertEquals("-1217015h", BigInt(-0x28b3c23f).toString(18));
  assertEquals("1018520b", BigInt(0x357da01a).toString(19));
  assertEquals("-9c64e33", BigInt(-0x1b0e9571).toString(19));
  assertEquals("d7bf9ab", BigInt(0x3309daa3).toString(20));
  assertEquals("-58h0h9h", BigInt(-0x14c30c55).toString(20));
  assertEquals("64igi9h", BigInt(0x1fdd329c).toString(21));
  assertEquals("-45cbc4a", BigInt(-0x15cf9682).toString(21));
  assertEquals("7bi7d1h", BigInt(0x32f0dfe3).toString(22));
  assertEquals("-61j743l", BigInt(-0x291ff61f).toString(22));
  assertEquals("5g5gg25", BigInt(0x325a10bd).toString(23));
  assertEquals("-3359flb", BigInt(-0x1bb653c9).toString(23));
  assertEquals("392f5ec", BigInt(0x267ed69c).toString(24));
  assertEquals("-2ab3icb", BigInt(-0x1bbf7bab).toString(24));
  assertEquals("3jb2afo", BigInt(0x36f93c24).toString(25));
  assertEquals("-30bcheh", BigInt(-0x2bec76fa).toString(25));
  assertEquals("3845agk", BigInt(0x3d04bf64).toString(26));
  assertEquals("-1gpjl3g", BigInt(-0x1e720b1a).toString(26));
  assertEquals("20bpaf0", BigInt(0x2e8ff627).toString(27));
  assertEquals("-292i3c2", BigInt(-0x35f751fe).toString(27));
  assertEquals("266113k", BigInt(0x3fd26738).toString(28));
  assertEquals("-1eh16bo", BigInt(-0x2bb5726c).toString(28));
  assertEquals("19gj7qa", BigInt(0x2f28e8d8).toString(29));
  assertEquals("-13a0apf", BigInt(-0x278b4588).toString(29));
  assertEquals("iasrb8", BigInt(0x1a99b3be).toString(30));
  assertEquals("-frlhoc", BigInt(-0x17106f48).toString(30));
  assertEquals("bfe4p2", BigInt(0x139f1ea3).toString(31));
  assertEquals("-ioal1a", BigInt(-0x200e49fa).toString(31));
  assertEquals("m0v0kf", BigInt(0x2c0f828f).toString(32));
  assertEquals("-g4bab5", BigInt(-0x2045a965).toString(32));
  assertEquals("9i1kit", BigInt(0x16450a9f).toString(33));
  assertEquals("-fqb0e7", BigInt(-0x24d9e889).toString(33));
  assertEquals("gb9r6m", BigInt(0x2c3acf46).toString(34));
  assertEquals("-jcaemv", BigInt(-0x346f72b3).toString(34));
  assertEquals("cw4mbk", BigInt(0x2870cdcb).toString(35));
  assertEquals("-hw4eki", BigInt(-0x3817c29b).toString(35));
  assertEquals("alzwgj", BigInt(0x263e2c13).toString(36));
  assertEquals("-bo4ukz", BigInt(-0x2a0f97d3).toString(36));

  // Multi-digit BigInts.
  // Test parseInt/toString round trip on a list of randomly generated
  // string representations of numbers in various bases.
  var positive = [0, 0,  // Skip base 0 and 1.
    "1100110001100010110011110110010010001011100111100101111000111101100001000",
    "1001200022210010220101120212021002011002201122200002211102120120021011020",
    "1111113020012203332320220022231110130001001320122012131311333110012023232",
    "4214313040222110434114402342013144321401424143322013320403411012033300312",
    "5025302003542512450341430541203424555035430434034243510233043041501130015",
    "6231052230016515343200525230300322104013130605414211331345043144525012021",
    "1146340505617030644211355340006353546230356336306352536433054143503442135",
    "7262360724624787621528668212168232276348417717770383567066203032200270570",
    "7573792356581293501680046955899735043496925151216904903504319328753434194",
    "4a627927557579898720a42647639128174a8689889766a219342133671449069a2235011",
    "1a574a5848289924996342a32893380690322330393633b587ba5a15b7b82080222400464",
    "5163304c74c387b7a443c92466688595b671a3329b42083b1499b0c10a74a9298a06c3a5a",
    "4b63c834356a03c80946133284a709cbbc2a75022757207dc31c14abd4c160dc122327c17",
    "d8d59cbb4ca2860de7c002eee4ab3c215b90069200d20dbdc0111cb1e1bab97e8c7609670",
    "22d4b69398a7f848e6ae36798811cd1a63d90f340d8607f3ce5566c97c18468787eb2b9fd",
    "1176gf69afd32cc105fa70c705927a384dbdb1g8d952f28028g31ebdc9e32a89f16e825ee",
    "5d64b74f4d70632h4ee07h7c1e2da9125c42g2727f4b6d95e5cec6ga49566hh731ab5f544",
    "7ff8cg7f05dd72916a09a4761ii7b0ibcg68ba39b10436f14efg76ge817317badcbi4gffc",
    "6d7c4hci6cd72e4ja26j354i12i71gb0cbj12gi145j91h02hde3b72c65geb7ff9bi9d0c2b",
    "c96997f50abe425d13a53kk4af631kg7db208ka5j5bfg8ca5f9c0bjf69j5kgg4jb5h7hi86",
    "3g5fd800d9ib9j0i8all5jgb23dh9483ab6le5ad9g4kja8a0b3j5jbjfge7k5fffg2kbheee",
    "9j1119d1cd61kmdm7kma105cki313f678fc3h25f4664281bbmg3fk97kfbh7d48j89j178ch",
    "d2933cdc9jfe4hl3794kb3e13dg2lihad968ib9jg19dgf1fi482b27ji0d10c6kfkdge5764",
    "bf6o0njkm1ij5in5nh7h94584bd80el02b07el5ojk9k9g0gn906do70gbbnckl048c0kdmao",
    "8gb7jnge9p9cdgigo394oa33gfaenc3gnb53eceg4b8511gkkm88b0dod85e5bggpc861d7d5",
    "qbbnqhkpleb4o8ndaddpc34h5b2iljn3jgnjdn5k57bi3n9i09hjle9hqgqdpgbnk499mak56",
    "akg7e2976arn8i2m53gif0dp59bmfd7mk9erlg2qm3fc76da9glf397eh4ooij9il0nfl9gac",
    "mehpbfrj5ah2ef3p2hl637gjp1pm5grqn4037pm1qfgfpr9cfljfc145hljehjjb48bb1n6en",
    "rg6ik3agnb3p6t2rtja9h4il76i8fkqlt6gplap3fq6pfr7bbcfcp5ffncf3nm4kamap39hse",
    "bk8rp9r9r8pltdqpb7euc6s9rcm33969pcq6uk3mtfoktt86di8589oacbam5tn29b9b6dq3j",
    "npth8juld44rss3e57iigjg65po3d1h02heo4r103jmg3ocv89buqtgiov35k39rdf8j9t4ca",
    "vrmqlwrrrd0uml1womae49jpa9tadh44fw7mucgk06l0uk4uqwuo37t6kwn7wwrm3a6oq081s",
    "n5cft6gvufqd8iksquu2amghokk17gbtpguidc290af634p7k7rhmfu7bf1s62ej4megoa1j4",
    "3v3gcrmlfc2tl0tefgkiogj41f6y2tmj9w5bxke8y03xqf49ox8gh9wbrhycrkluicqajtnur",
    "z2m7b0sy2tzergtkqts5yj0dkrlfkxls81ijgxgfequizpntcwggv2d4rdzcncd0kj9mrmnrb",
  ];
  var negative = [0, 0,  // Skip base 0 and 1.
    "-100010011110111010111111110001100100111010101000001011010010101100101000",
    "-110012122000122102021210112200001000122011010120101201001122000002022102",
    "-203210320111001002200122200001312300221100221321010300023323201113122333",
    "-133042441230110320040323303341320302144241224443231311022240124413104131",
    "-311325230504055004330150145105331121322231155401110315251422505233103112",
    "-643153641664240231336166403516403454646560261062114326443664602606315326",
    "-200057252627665476551635525303641543165622340301637556323453513664337277",
    "-826688166214270516331644053744613530235020517172322840763172114078364165",
    "-743042397390679269240157150971957535458122650450558451124173993544604852",
    "-73528688500003573942a56a504a2996a1384129563098512a63196697975038692aaa63",
    "-616576a2948a9029316290168b71137b027851639a0283150b125b664b74b767a3597805",
    "-b875467540719b371b7a36047a7886872a5399c4c630c37149bc3182917a7a7c124475bb",
    "-3860411b61d35977721bc81bd715c386c9b70a752940913d265505d8c7c5dd2624b591d7",
    "-bad5dd79b083ee0da9a6296664e72c246d827762357116ae7076a22bb369acbc3a201d03",
    "-f9b37352aff265124303942a463917a252ff1a2ff4a33777f490b4c103bdcd1a655dbe2c",
    "-805fg8c74125214g383a8d8g573c49fa7c4035fbc6db61g5gb5g6beb8f90dae4a9a5g7cc",
    "-70aae113459d3h5084b1gg209g3695d20e78d01gcbb71bh1bd4gdge31haf5hc02dghf14e",
    "-c55a57haf47b7ih2gh6ea93098ig02b42icga6ead254e0aeeic7g53h5fd6637ge03b2e20",
    "-e32f7204624ie596j731g72136cejc25ebbgb0140i4997fcdf477f021d86ci4e10db543a",
    "-i7f32c817i3cac1c24c7786k6ig185f47cj1471ki6bb7agiae838027gjge9g59if9f88g6",
    "-i30aha2030a9605c270h92e1ca3i02j996hl918gh52fbhb7i16ik1i919ieak3cj384kb61",
    "-58jmem8e59li67aellid2083dabh4kh51ci1jg7c6a3k4l1hdgfkdha0fglfm4805kida5b9",
    "-cl9iecjg9ak087cad4151lll44296heae2349g70fbjj37998m2ddn6427fgcl2aknhgn1a1",
    "-alfjfhho4gf8bi4j2bi3743mhg2aache4c6jcinkmf5ddm7kf9gg350hlja16ealbdlk201j",
    "-bhh1146ho3o2m3b839c565hbgjnhjh96oofbmdl7gn8h4f94kli94hkk180o79pc4d2l0721",
    "-p00gknh7e05k6a3apg6i9lb46f4a9qeeiq1778ak8il5dcponk5gl2fiednb4pmo1agmoqph",
    "-4j8lo4d4p508fnd2hkfb76e8ri81k6hq0op3pr14ca0cn96pccplk7rbahc9cdkdce1q16dn",
    "-ednlo3ogf2i8annrel9rm323bpf00meed3oi47n0qrdgnd2n3il4bnsc9s2jd7loh44im8ra",
    "-bjjg6fsbpcc2tc1o09m9r6fd6eoq5480har62a5offn9thcfahbno9kf9magl2akl0jgncj9",
    "-sonuhat2h60glpbpej9jjado2s5l86122d26tudoc1d6aic2oitu793gk0mlac3dk1dufp1q",
    "-i9pbvm53ubh8jqifuarauch8cbgk9cjsl6rlioka1phs1lskg1oosll23hjoli2subgr1rto",
    "-w1ncn5t60b5dv669ekwnvk8n2g7djrsl8cdkwun8o3m5divc3jhnkp2381rhj70gc71a6wff",
    "-buiq8v33p5ex44ps4s45enj6lrluivm19lcowkvntu72u0xguw13bxgxxe7mdlwt1a4qksae",
    "-woiycfmea6i12r2yai49mf4lbd7w2jdoebiogfhnh1i4rwgox57obci8qbsfpb4w07nu19m5",
    "-tbttuip1r6ioca6g6dw354o4m78qep9yh03nojx47yq29fqime6zstwllb74501qct8eskxn",
  ];
  for (var base = 2; base <= 36; base++) {
    var input = positive[base];
    assertEquals(input, BigInt.parseInt(input, base).toString(base));
    input = negative[base];
    assertEquals(input, BigInt.parseInt(input, base).toString(base));
  }
}

// .parseInt
{
  assertEquals("hellobigint", BigInt.parseInt("hellobigint", 32).toString(32));
  assertEquals("abc", BigInt.parseInt("101010111100", 2).toString(16));
  // Detect "0x" prefix.
  assertEquals("f00dcafe", BigInt.parseInt("0xf00dcafe").toString(16));
  // Default base is 10, trailing junk is skipped.
  assertEquals("abc", BigInt.parseInt("2748junk").toString(16));
  // Objects are converted to string.
  let obj = {toString: () => "0x12345"};
  assertEquals("12345", BigInt.parseInt(obj).toString(16));
  // Empty and invalid strings throw.
  assertThrows("BigInt.parseInt('')", SyntaxError);
  assertThrows("BigInt.parseInt('nope', 2)", SyntaxError);
}

// .valueOf
{
  assertEquals(Object(zero).valueOf(), another_zero);
  assertThrows(() => { return BigInt.prototype.valueOf.call("string"); },
               TypeError);
  assertEquals(-42n, Object(-42n).valueOf());
}

// ToBoolean
{
  assertTrue(!zero);
  assertFalse(!!zero);
  assertTrue(!!!zero);

  assertFalse(!one);
  assertTrue(!!one);
  assertFalse(!!!one);

  // This is a hack to test Object::BooleanValue.
  assertTrue(%CreateIterResultObject(42, one).done);
  assertFalse(%CreateIterResultObject(42, zero).done);
}

// ToNumber
{
  assertThrows(() => isNaN(zero), TypeError);
  assertThrows(() => isNaN(one), TypeError);

  assertThrows(() => +zero, TypeError);
  assertThrows(() => +one, TypeError);
}
{
  let Zero = {valueOf() { return zero }};
  let One = {valueOf() { return one }};

  assertThrows(() => isNaN(Zero), TypeError);
  assertThrows(() => isNaN(One), TypeError);

  assertThrows(() => +Zero, TypeError);
  assertThrows(() => +One, TypeError);
}{
  let Zero = {valueOf() { return Object(NaN) }, toString() { return zero }};
  let One = {valueOf() { return one }, toString() { return NaN }};

  assertThrows(() => isNaN(Zero), TypeError);
  assertThrows(() => isNaN(One), TypeError);

  assertThrows(() => +Zero, TypeError);
  assertThrows(() => +One, TypeError);
}

// ToObject
{
  const ToObject = x => (new Function("", "return this")).call(x);

  function test(x) {
    const X = ToObject(x);
    assertEquals(typeof x, "bigint");
    assertEquals(typeof X, 'object');
    assertEquals(X.constructor, BigInt);
    assertTrue(X == x);
  }

  test(0n);
  test(-1n);
  test(1n);
  test(2343423423423423423424234234234235234524353453452345324523452345234534n);
}{
  function test(x) {
    const X = Object(x);
    assertEquals(typeof x, "bigint");
    assertEquals(typeof X, 'object');
    assertEquals(X.constructor, BigInt);
    assertTrue(X == x);
  }

  test(0n);
  test(-1n);
  test(1n);
  test(2343423423423423423424234234234235234524353453452345324523452345234534n);
}

// Literals
{
  // Invalid literals.
  assertThrows("00n", SyntaxError);
  assertThrows("01n", SyntaxError);
  assertThrows("0bn", SyntaxError);
  assertThrows("0on", SyntaxError);
  assertThrows("0xn", SyntaxError);
  assertThrows("1.n", SyntaxError);
  assertThrows("1.0n", SyntaxError);
  assertThrows("1e25n", SyntaxError);

  // Various radixes.
  assertTrue(12345n === BigInt(12345));
  assertTrue(0xabcden === BigInt(0xabcde));
  assertTrue(0xAbCdEn === BigInt(0xabcde));
  assertTrue(0o54321n === BigInt(0o54321));
  assertTrue(0b1010101n === BigInt(0b1010101));
}

// Binary ops.
{
  let One = {valueOf() { return one }};
  assertTrue(one + two === three);
  assertTrue(One + two === three);
  assertTrue(two + One === three);
  assertEquals("hello1", "hello" + one);
  assertEquals("2hello", two + "hello");
  assertThrows("one + 2", TypeError);
  assertThrows("2 + one", TypeError);
  assertThrows("one + 0.5", TypeError);
  assertThrows("0.5 + one", TypeError);
  assertThrows("one + null", TypeError);
  assertThrows("null + one", TypeError);

  assertTrue(three - two === one);
  assertThrows("two - 1", TypeError);
  assertThrows("2 - one", TypeError);
  assertThrows("two - 0.5", TypeError);
  assertThrows("2.5 - one", TypeError);

  assertTrue(two * three === six);
  assertTrue(two * One === two);
  assertTrue(One * two === two);
  assertThrows("two * 1", TypeError);
  assertThrows("1 * two", TypeError);
  assertThrows("two * 1.5", TypeError);
  assertThrows("1.5 * two", TypeError);

  assertTrue(six / three === two);
  assertThrows("six / 3", TypeError);
  assertThrows("3 / three", TypeError);
  assertThrows("six / 0.5", TypeError);
  assertThrows("0.5 / six", TypeError);
  assertThrows("zero / zero", RangeError);
  assertThrows("zero / 0", TypeError);

  assertTrue(three % two === one);
  assertThrows("three % 2", TypeError);
  assertThrows("3 % two", TypeError);
  assertThrows("three % 2.5", TypeError);
  assertThrows("3.5 % two", TypeError);
  assertThrows("three % zero", RangeError);
  assertThrows("three % 0", TypeError);
}

// Bitwise binary ops.
{
  let One = {valueOf() { return one }};
  assertTrue((three & one) === one);
  assertTrue((BigInt(-2) & zero) === zero);
  assertTrue((three & One) === one);
  assertTrue((One & three) === one);
  assertThrows("three & 1", TypeError);
  assertThrows("1 & three", TypeError);
  assertThrows("three & true", TypeError);
  assertThrows("true & three", TypeError);
  assertThrows("three & {valueOf: function() { return 1; }}", TypeError);
  assertThrows("({valueOf: function() { return 1; }}) & three", TypeError);

  assertTrue((two | one) === three);
  assertThrows("two | 0", TypeError);
  assertThrows("0 | two", TypeError);
  assertThrows("two | undefined", TypeError);
  assertThrows("undefined | two", TypeError);

  assertTrue((three ^ one) === two);
  assertThrows("three ^ 1", TypeError);
  assertThrows("1 ^ three", TypeError);
  assertThrows("three ^ 2.5", TypeError);
  assertThrows("2.5 ^ three", TypeError);
}

// Shift ops.
{
  assertTrue(one << one === two);
  assertThrows("one << 1", TypeError);
  assertThrows("1 << one", TypeError);
  assertThrows("one << true", TypeError);
  assertThrows("true << one", TypeError);

  assertTrue(three >> one === one);
  assertThrows("three >> 1", TypeError);
  assertThrows("0xbeef >> one", TypeError);
  assertThrows("three >> 1.5", TypeError);
  assertThrows("23.45 >> three", TypeError);

  assertThrows("three >>> one", TypeError);
  assertThrows("three >>> 1", TypeError);
  assertThrows("0xbeef >>> one", TypeError);
  assertThrows("three >>> {valueOf: function() { return 1; }}", TypeError);
  assertThrows("({valueOf: function() { return 1; }}) >>> one", TypeError);
}

// Unary ops.
{
  let One = {valueOf() { return one }};
  assertTrue(~minus_one === zero);
  assertTrue(-minus_one === one);
  assertTrue(-One === minus_one);
  assertTrue(~~two === two);
  assertTrue(-(-two) === two);
  assertTrue(~One === BigInt(-2));

  let a = minus_one;
  assertTrue(a++ === minus_one);
  assertTrue(a === zero);
  assertTrue(a++ === zero);
  assertTrue(a === one);
  assertTrue(++a === two);
  assertTrue(a === two);
  assertTrue(--a === one);
  assertTrue(a === one);
  assertTrue(a-- === one);
  assertTrue(a === zero);
  assertTrue(a-- === zero);
  assertTrue(a === minus_one);

  a = {valueOf() { return minus_one }};
  assertTrue(a++ === minus_one);
  assertTrue(a++ === zero);
  assertTrue(a === one);

  a = {valueOf() { return one }};
  assertTrue(a-- === one);
  assertTrue(a-- === zero);
  assertTrue(a === minus_one);
}

// ToPropertyKey
{
  let obj = {};
  assertEquals(obj[0n], undefined);
  assertEquals(obj[0n] = 42, 42);
  assertEquals(obj[0n], 42);
  assertEquals(obj[0], 42);
  obj[0]++;
  assertEquals(obj[1n - 1n], 43);
  assertEquals(Reflect.get(obj, -0n), 43);
  assertEquals(obj[{toString() {return 0n}}], 43);
  assertEquals(Reflect.ownKeys(obj), ["0"]);
}{
  let obj = {};
  const unsafe = 9007199254740993n;
  assertEquals(obj[unsafe] = 23, 23);
  assertEquals(obj[unsafe], 23);
  assertEquals(Reflect.ownKeys(obj), ["9007199254740993"]);
  assertEquals(obj[9007199254740993], undefined);
  delete obj[unsafe];
  assertEquals(Reflect.ownKeys(obj), []);
}{
  let arr = [];
  assertFalse(4n in arr);
  arr[4n] = 42;
  assertTrue(4n in arr);
  let enumkeys = 0;
  for (const key in arr) {
    enumkeys++;
    assertSame(key, "4");
  }
  assertEquals(enumkeys, 1);
}{
  let str = "blubb";
  assertEquals(str[2n], "u");
  assertThrows(() => str.slice(2n), TypeError);
}{
  let obj = {};
  let key = 0;

  function set_key(x) { obj[key] = x }
  set_key("aaa");
  set_key("bbb");
  key = 0n;
  set_key("ccc");
  assertEquals(obj[key], "ccc");

  function get_key() { return obj[key] }
  assertEquals(get_key(), "ccc");
  assertEquals(get_key(), "ccc");
  key = 0;
  assertEquals(get_key(), "ccc");
}{
  assertSame(%ToName(0n), "0");
  assertSame(%ToName(-0n), "0");

  const unsafe = 9007199254740993n;
  assertSame(%ToName(unsafe), "9007199254740993");
}
