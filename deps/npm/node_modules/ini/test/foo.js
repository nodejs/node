var i = require("../")
  , tap = require("tap")
  , test = tap.test
  , fs = require("fs")
  , path = require("path")
  , fixture = path.resolve(__dirname, "./fixtures/foo.ini")
  , data = fs.readFileSync(fixture, "utf8")
  , d
  , expectE = 'o = p\n'
            + 'a with spaces = b  c\n'
            + '" xa  n          p " = "\\"\\r\\nyoyoyo\\r\\r\\n"\n'
            + '[a]\n'
            + 'av = a val\n'
            + 'e = { o: p, a: '
            + '{ av: a val, b: { c: { e: "this value" '
            + '} } } }\nj = "\\"{ o: \\"p\\", a: { av:'
            + ' \\"a val\\", b: { c: { e: \\"this value'
            + '\\" } } } }\\""\n[a.b.c]\ne = 1\nj = 2\n'
  , expectD =
    { o: 'p',
      'a with spaces': 'b  c',
      " xa  n          p ":'"\r\nyoyoyo\r\r\n',
      a:
       { av: 'a val',
         e: '{ o: p, a: { av: a val, b: { c: { e: "this value" } } } }',
         j: '"{ o: "p", a: { av: "a val", b: { c: { e: "this value" } } } }"',
         b: { c: { e: '1', j: '2' } } }
    }

test("decode from file", function (t) {
  d = i.decode(data)
  t.deepEqual(d, expectD)
  t.end()
})

test("encode from data", function (t) {
  e = i.encode(expectD)
  t.deepEqual(e, expectE)
  t.end()
})
