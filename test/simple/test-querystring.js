require("../common");

// test using assert

var qs = require("querystring");

// folding block.
{
// [ wonkyQS, canonicalQS, obj ]
var qsTestCases = [
  ["foo=bar",  "foo=bar", {"foo" : "bar"}],
  ["foo=bar&foo=quux", "foo%5B%5D=bar&foo%5B%5D=quux", {"foo" : ["bar", "quux"]}],
  ["foo=1&bar=2", "foo=1&bar=2", {"foo" : 1, "bar" : 2}],
  ["my+weird+field=q1%212%22%27w%245%267%2Fz8%29%3F", "my%20weird%20field=q1!2%22'w%245%267%2Fz8)%3F", {"my weird field" : "q1!2\"'w$5&7/z8)?" }],
  ["foo%3Dbaz=bar", "foo%3Dbaz=bar", {"foo=baz" : "bar"}],
  ["foo=baz=bar", "foo=baz%3Dbar", {"foo" : "baz=bar"}],
    [ "str=foo&arr[]=1&arr[]=2&arr[]=3&obj[a]=bar&obj[b][]=4&obj[b][]=5&obj[b][]=6&obj[b][]=&obj[c][]=4&obj[c][]=5&obj[c][][somestr]=baz&obj[objobj][objobjstr]=blerg&somenull=&undef=", "str=foo&arr%5B%5D=1&arr%5B%5D=2&arr%5B%5D=3&obj%5Ba%5D=bar&obj%5Bb%5D%5B%5D=4&obj%5Bb%5D%5B%5D=5&obj%5Bb%5D%5B%5D=6&obj%5Bb%5D%5B%5D=&obj%5Bc%5D%5B%5D=4&obj%5Bc%5D%5B%5D=5&obj%5Bc%5D%5B%5D%5Bsomestr%5D=baz&obj%5Bobjobj%5D%5Bobjobjstr%5D=blerg&somenull=&undef=", {
    "str":"foo",
    "arr":[1,2,3],
    "obj":{
      "a":"bar",
      "b":[4,5,6,""],
      "c":[4,5,{"somestr":"baz"}],
      "objobj":{"objobjstr":"blerg"}
    },
    "somenull":"",
    "undef":""
  }],
  ["foo[bar][bla]=baz&foo[bar][bla]=blo", "foo%5Bbar%5D%5Bbla%5D%5B%5D=baz&foo%5Bbar%5D%5Bbla%5D%5B%5D=blo", {"foo":{"bar":{"bla":["baz","blo"]}}}],
  ["foo[bar][][bla]=baz&foo[bar][][bla]=blo", "foo%5Bbar%5D%5B%5D%5Bbla%5D=baz&foo%5Bbar%5D%5B%5D%5Bbla%5D=blo", {"foo":{"bar":[{"bla":"baz"},{"bla":"blo"}]}}],
  ["foo[bar][bla][]=baz&foo[bar][bla][]=blo", "foo%5Bbar%5D%5Bbla%5D%5B%5D=baz&foo%5Bbar%5D%5Bbla%5D%5B%5D=blo", {"foo":{"bar":{"bla":["baz","blo"]}}}],

  ["foo.bar.bla=baz&foo.bar.bla=blo", "foo%5Bbar%5D%5Bbla%5D%5B%5D=baz&foo%5Bbar%5D%5Bbla%5D%5B%5D=blo", {"foo":{"bar":{"bla":["baz","blo"]}}}],
  ["foo.bar[].bla=baz&foo[bar][][bla]=blo", "foo%5Bbar%5D%5B%5D%5Bbla%5D=baz&foo%5Bbar%5D%5B%5D%5Bbla%5D=blo", {"foo":{"bar":[{"bla":"baz"},{"bla":"blo"}]}}],
  ["foo[bar].bla[]=baz&foo.bar[bla][]=blo", "foo%5Bbar%5D%5Bbla%5D%5B%5D=baz&foo%5Bbar%5D%5Bbla%5D%5B%5D=blo", {"foo":{"bar":{"bla":["baz","blo"]}}}],

  ["foo['bar']['bla']=baz&foo[\"bar\"][\"bla\"]=blo", "foo%5Bbar%5D%5Bbla%5D%5B%5D=baz&foo%5Bbar%5D%5Bbla%5D%5B%5D=blo", {"foo":{"bar":{"bla":["baz","blo"]}}}],
  ["foo['bar'][]['bla']=baz&foo['bar'][][\"bla\"]=blo", "foo%5Bbar%5D%5B%5D%5Bbla%5D=baz&foo%5Bbar%5D%5B%5D%5Bbla%5D=blo", {"foo":{"bar":[{"bla":"baz"},{"bla":"blo"}]}}],
  ["foo[bar][\"bla\"][]=baz&foo[\"bar\"][bla][]=blo", "foo%5Bbar%5D%5Bbla%5D%5B%5D=baz&foo%5Bbar%5D%5Bbla%5D%5B%5D=blo", {"foo":{"bar":{"bla":["baz","blo"]}}}],

  [" foo = bar ", "%20foo%20=%20bar%20", {" foo ":" bar "}],
  ["foo=%zx", "foo=%25zx", {"foo":"%zx"}],
  ["foo=%EF%BF%BD", "foo=%EF%BF%BD", {"foo" : "\ufffd" }]
];

// [ wonkyQS, canonicalQS, obj ]
var qsColonTestCases = [
  ["foo:bar", "foo:bar", {"foo":"bar"}],
  ["foo:bar;foo:quux", "foo%5B%5D:bar;foo%5B%5D:quux", {"foo" : ["bar", "quux"]}],
  ["foo:1&bar:2;baz:quux", "foo:1%26bar%3A2;baz:quux", {"foo":"1&bar:2", "baz":"quux"}],
  ["foo%3Abaz:bar", "foo%3Abaz:bar", {"foo:baz":"bar"}],
  ["foo:baz:bar", "foo:baz%3Abar", {"foo":"baz:bar"}]
];

// [ wonkyObj, qs, canonicalObj ]
var extendedFunction = function () {};
extendedFunction.prototype = {a:"b"};
var qsWeirdObjects = [
  [ {regexp:/./g}, "regexp=", {"regexp":""} ],
  [ {regexp: new RegExp(".", "g")}, "regexp=", {"regexp":""} ],
  [ {fn:function () {}}, "fn=", {"fn":""}],
  [ {fn:new Function("")}, "fn=", {"fn":""} ],
  [ {math:Math}, "math=", {"math":""} ],
  [ {e:extendedFunction}, "e=", {"e":""} ],
  [ {d:new Date()}, "d=", {"d":""} ],
  [ {d:Date}, "d=", {"d":""} ],
  [ {f:new Boolean(false), t:new Boolean(true)}, "f=0&t=1", {"f":0, "t":1} ],
  [ {f:false, t:true}, "f=0&t=1", {"f":0, "t":1} ],
];
}

var qsNoMungeTestCases = [
  ["", {}],
  ["foo=bar&foo=baz", {"foo": ["bar", "baz"]}],
  ["blah=burp", {"blah": "burp"}],
  ["gragh=1&gragh=3&goo=2", {"gragh": ["1", "3"], "goo": "2"}],
  ["frappucino=muffin&goat%5B%5D=scone&pond=moose",
   {"frappucino": "muffin", "goat[]": "scone", "pond": "moose"}],
  ["obj%5Btrololol%5D=yes&obj%5Blololo%5D=no", {"obj": {"trololol": "yes", "lololo": "no"}}],
];

// test that the canonical qs is parsed properly.
qsTestCases.forEach(function (testCase) {
  assert.deepEqual(testCase[2], qs.parse(testCase[0]));
});

// test that the colon test cases can do the same
qsColonTestCases.forEach(function (testCase) {
  assert.deepEqual(testCase[2], qs.parse(testCase[0], ";", ":"));
});

// test the weird objects, that they get parsed properly
qsWeirdObjects.forEach(function (testCase) {
  assert.deepEqual(testCase[2], qs.parse(testCase[1]));
});

qsNoMungeTestCases.forEach(function (testCase) {
  assert.deepEqual(testCase[0], qs.stringify(testCase[1], "&", "=", false));
});

// test the nested qs-in-qs case
var f = qs.parse("a=b&q=x%3Dy%26y%3Dz");
f.q = qs.parse(f.q);
assert.deepEqual(f, { a : "b", q : { x : "y", y : "z" } });

// nested in colon
var f = qs.parse("a:b;q:x%3Ay%3By%3Az", ";", ":");
f.q = qs.parse(f.q, ";", ":");
assert.deepEqual(f, { a : "b", q : { x : "y", y : "z" } });


// now test stringifying
assert.throws(function () {
  var f = {};
  f.f = f;
  qs.stringify(f);
});

// basic
qsTestCases.forEach(function (testCase) {
  assert.equal(testCase[1], qs.stringify(testCase[2]));
});

qsColonTestCases.forEach(function (testCase) {
  assert.equal(testCase[1], qs.stringify(testCase[2], ";", ":"));
});

qsWeirdObjects.forEach(function (testCase) {
  assert.equal(testCase[1], qs.stringify(testCase[0]));
});

// nested
var f = qs.stringify({
  a : "b",
  q : qs.stringify({
    x : "y",
    y : "z"
  })
});
assert.equal(f, "a=b&q=x%3Dy%26y%3Dz");

assert.doesNotThrow(function () {
  qs.parse(undefined);
});

// nested in colon
var f = qs.stringify({
  a : "b",
  q : qs.stringify({
    x : "y",
    y : "z"
  }, ";", ":")
}, ";", ":");
assert.equal(f, "a:b;q:x%3Ay%3By%3Az");
