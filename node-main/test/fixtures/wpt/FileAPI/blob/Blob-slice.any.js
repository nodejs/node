// META: title=Blob slice
// META: script=../support/Blob.js
'use strict';

test_blob(function() {
  var blobTemp = new Blob(["PASS"]);
  return blobTemp.slice();
}, {
  expected: "PASS",
  type: "",
  desc: "no-argument Blob slice"
});

test(function() {
  var blob1, blob2;

  test_blob(function() {
    return blob1 = new Blob(["squiggle"]);
  }, {
    expected: "squiggle",
    type: "",
    desc: "blob1."
  });

  test_blob(function() {
    return blob2 = new Blob(["steak"], {type: "content/type"});
  }, {
    expected: "steak",
    type: "content/type",
    desc: "blob2."
  });

  test_blob(function() {
    return new Blob().slice(0,0,null);
  }, {
    expected: "",
    type: "null",
    desc: "null type Blob slice"
  });

  test_blob(function() {
    return new Blob().slice(0,0,undefined);
  }, {
    expected: "",
    type: "",
    desc: "undefined type Blob slice"
  });

  test_blob(function() {
    return new Blob().slice(0,0);
  }, {
    expected: "",
    type: "",
    desc: "no type Blob slice"
  });

  var arrayBuffer = new ArrayBuffer(16);
  var int8View = new Int8Array(arrayBuffer);
  for (var i = 0; i < 16; i++) {
    int8View[i] = i + 65;
  }

  var testData = [
    [
      ["PASSSTRING"],
      [{start:  -6, contents: "STRING"},
       {start: -12, contents: "PASSSTRING"},
       {start:   4, contents: "STRING"},
       {start:  12, contents: ""},
       {start: 0, end:  -6, contents: "PASS"},
       {start: 0, end: -12, contents: ""},
       {start: 0, end:   4, contents: "PASS"},
       {start: 0, end:  12, contents: "PASSSTRING"},
       {start: 7, end:   4, contents: ""}]
    ],

    // Test 3 strings
    [
      ["foo", "bar", "baz"],
      [{start:  0, end:  9, contents: "foobarbaz"},
       {start:  0, end:  3, contents: "foo"},
       {start:  3, end:  9, contents: "barbaz"},
       {start:  6, end:  9, contents: "baz"},
       {start:  6, end: 12, contents: "baz"},
       {start:  0, end:  9, contents: "foobarbaz"},
       {start:  0, end: 11, contents: "foobarbaz"},
       {start: 10, end: 15, contents: ""}]
    ],

    // Test string, Blob, string
    [
      ["foo", blob1, "baz"],
      [{start:  0, end:  3, contents: "foo"},
       {start:  3, end: 11, contents: "squiggle"},
       {start:  2, end:  4, contents: "os"},
       {start: 10, end: 12, contents: "eb"}]
    ],

    // Test blob, string, blob
    [
      [blob1, "foo", blob1],
      [{start:  0, end:  8, contents: "squiggle"},
       {start:  7, end:  9, contents: "ef"},
       {start: 10, end: 12, contents: "os"},
       {start:  1, end:  4, contents: "qui"},
       {start: 12, end: 15, contents: "qui"},
       {start: 40, end: 60, contents: ""}]
    ],

    // Test blobs all the way down
    [
      [blob2, blob1, blob2],
      [{start: 0,  end:  5, contents: "steak"},
       {start: 5,  end: 13, contents: "squiggle"},
       {start: 13, end: 18, contents: "steak"},
       {start:  1, end:  3, contents: "te"},
       {start:  6, end: 10, contents: "quig"}]
    ],

    // Test an ArrayBufferView
    [
      [int8View, blob1, "foo"],
      [{start:  0, end:  8, contents: "ABCDEFGH"},
       {start:  8, end: 18, contents: "IJKLMNOPsq"},
       {start: 17, end: 20, contents: "qui"},
       {start:  4, end: 12, contents: "EFGHIJKL"}]
    ],

    // Test a partial ArrayBufferView
    [
      [new Uint8Array(arrayBuffer, 3, 5), blob1, "foo"],
      [{start:  0, end:  8, contents: "DEFGHsqu"},
       {start:  8, end: 18, contents: "igglefoo"},
       {start:  4, end: 12, contents: "Hsquiggl"}]
    ],

    // Test type coercion of a number
    [
      [3, int8View, "foo"],
      [{start:  0, end:  8, contents: "3ABCDEFG"},
       {start:  8, end: 18, contents: "HIJKLMNOPf"},
       {start: 17, end: 21, contents: "foo"},
       {start:  4, end: 12, contents: "DEFGHIJK"}]
    ],

    [
      [(new Uint8Array([0, 255, 0])).buffer,
       new Blob(['abcd']),
       'efgh',
       'ijklmnopqrstuvwxyz'],
      [{start:  1, end:  4, contents: "\uFFFD\u0000a"},
       {start:  4, end:  8, contents: "bcde"},
       {start:  8, end: 12, contents: "fghi"},
       {start:  1, end: 12, contents: "\uFFFD\u0000abcdefghi"}]
    ]
  ];

  testData.forEach(function(data, i) {
    var blobs = data[0];
    var tests = data[1];
    tests.forEach(function(expectations, j) {
      test(function() {
        var blob = new Blob(blobs);
        assert_true(blob instanceof Blob);
        assert_false(blob instanceof File);

        test_blob(function() {
          return expectations.end === undefined
                 ? blob.slice(expectations.start)
                 : blob.slice(expectations.start, expectations.end);
        }, {
          expected: expectations.contents,
          type: "",
          desc: "Slicing test: slice (" + i + "," + j + ")."
        });
      }, "Slicing test (" + i + "," + j + ").");
    });
  });
}, "Slices");

var invalidTypes = [
  "\xFF",
  "te\x09xt/plain",
  "te\x00xt/plain",
  "te\x1Fxt/plain",
  "te\x7Fxt/plain"
];
invalidTypes.forEach(function(type) {
  test_blob(function() {
    var blob = new Blob(["PASS"]);
    return blob.slice(0, 4, type);
  }, {
    expected: "PASS",
    type: "",
    desc: "Invalid contentType (" + format_value(type) + ")"
  });
});

var validTypes = [
  "te(xt/plain",
  "te)xt/plain",
  "te<xt/plain",
  "te>xt/plain",
  "te@xt/plain",
  "te,xt/plain",
  "te;xt/plain",
  "te:xt/plain",
  "te\\xt/plain",
  "te\"xt/plain",
  "te/xt/plain",
  "te[xt/plain",
  "te]xt/plain",
  "te?xt/plain",
  "te=xt/plain",
  "te{xt/plain",
  "te}xt/plain",
  "te\x20xt/plain",
  "TEXT/PLAIN",
  "text/plain;charset = UTF-8",
  "text/plain;charset=UTF-8"
];
validTypes.forEach(function(type) {
  test_blob(function() {
    var blob = new Blob(["PASS"]);
    return blob.slice(0, 4, type);
  }, {
    expected: "PASS",
    type: type.toLowerCase(),
    desc: "Valid contentType (" + format_value(type) + ")"
  });
});
