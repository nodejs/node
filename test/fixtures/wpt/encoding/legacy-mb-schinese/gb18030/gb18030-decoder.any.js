// META: script=./resources/ranges.js

const decode = (input, output, desc) => {
  test(function () {
    for (const encoding of ["gb18030", "gbk"]) {
      assert_equals(
        new TextDecoder(encoding).decode(new Uint8Array(input)),
        output,
      );
    }
  }, "gb18030 decoder: " + desc);
};

decode([115], "s", "ASCII");
decode([0x80], "\u20AC", "euro");
decode([0xFF], "\uFFFD", "initial byte out of accepted ranges");
decode([0x81], "\uFFFD", "end of queue, gb18030 first not 0");
decode([0x81, 0x28], "\ufffd(", "two bytes 0x81 0x28");
decode([0x81, 0x40], "\u4E02", "two bytes 0x81 0x40");
decode([0x81, 0x7E], "\u4E8A", "two bytes 0x81 0x7e");
decode([0x81, 0x7F], "\ufffd\u007f", "two bytes 0x81 0x7f");
decode([0x81, 0x80], "\u4E90", "two bytes 0x81 0x80");
decode([0x81, 0xFE], "\u4FA2", "two bytes 0x81 0xFE");
decode([0x81, 0xFF], "\ufffd", "two bytes 0x81 0xFF");
decode([0xFE, 0x40], "\uFA0C", "two bytes 0xFE 0x40");
decode([0xFE, 0xFE], "\uE4C5", "two bytes 0xFE 0xFE");
decode([0xFE, 0xFF], "\ufffd", "two bytes 0xFE 0xFF");
decode([0x81, 0x30], "\ufffd", "two bytes 0x81 0x30");
decode([0x81, 0x30, 0xFE], "\ufffd", "three bytes 0x81 0x30 0xFE");
decode([0x81, 0x30, 0xFF], "\ufffd0\ufffd", "three bytes 0x81 0x30 0xFF");
decode(
  [0x81, 0x30, 0xFE, 0x29],
  "\ufffd0\ufffd)",
  "four bytes 0x81 0x30 0xFE 0x29",
);
decode([0xFE, 0x39, 0xFE, 0x39], "\ufffd", "four bytes 0xFE 0x39 0xFE 0x39");
decode([0x81, 0x35, 0xF4, 0x36], "\u1E3E", "pointer 7458");
decode([0x81, 0x35, 0xF4, 0x37], "\ue7c7", "pointer 7457");
decode([0x81, 0x35, 0xF4, 0x38], "\u1E40", "pointer 7459");
decode([0x84, 0x31, 0xA4, 0x39], "\uffff", "pointer 39419");
decode([0x84, 0x31, 0xA5, 0x30], "\ufffd", "pointer 39420");
decode([0x8F, 0x39, 0xFE, 0x39], "\ufffd", "pointer 189999");
decode([0x90, 0x30, 0x81, 0x30], "\u{10000}", "pointer 189000");
decode([0xE3, 0x32, 0x9A, 0x35], "\u{10FFFF}", "pointer 1237575");
decode([0xE3, 0x32, 0x9A, 0x36], "\ufffd", "pointer 1237576");
decode([0x83, 0x36, 0xC8, 0x30], "\uE7C8", "legacy ICU special case 1");
decode([0xA1, 0xAD], "\u2026", "legacy ICU special case 2");
decode([0xA1, 0xAB], "\uFF5E", "legacy ICU special case 3");

let i = 0;
for (const range of ranges) {
  const pointer = range[0];
  decode(
    [
      Math.floor(pointer / 12600) + 0x81,
      Math.floor((pointer % 12600) / 1260) + 0x30,
      Math.floor((pointer % 1260) / 10) + 0x81,
      pointer % 10 + 0x30,
    ],
    range[1],
    "range " + i++,
  );
}
