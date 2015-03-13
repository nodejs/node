var assert = require('assert')
var parse = require('../').parse

function addTest(arg, bulk) {
  function fn_json5() {
    //console.log('testing: ', arg)
    try {
      var x = parse(arg)
    } catch(err) {
      x = 'fail'
    }
    try {
      var z = eval('(function(){"use strict"\nreturn ('+String(arg)+'\n)\n})()')
    } catch(err) {
      z = 'fail'
    }
    assert.deepEqual(x, z)
  }

  function fn_strict() {
    //console.log('testing: ', arg)
    try {
      var x = parse(arg, {mode: 'json'})
    } catch(err) {
      x = 'fail'
    }
    try {
      var z = JSON.parse(arg)
    } catch(err) {
      z = 'fail'
    }
    assert.deepEqual(x, z)
  }

  if (typeof(describe) === 'function' && !bulk) {
    it('test_parse_json5: ' + JSON.stringify(arg), fn_json5)
    it('test_parse_strict: ' + JSON.stringify(arg), fn_strict)
  } else {
    fn_json5()
    fn_strict()
  }
}

addTest('"\\uaaaa\\u0000\\uFFFF\\uFaAb"')
addTest(' "\\xaa\\x00\xFF\xFa\0\0"  ')
addTest('"\\\'\\"\\b\\f\\t\\n\\r\\v"')
addTest('"\\q\\w\\e\\r\\t\\y\\\\i\\o\\p\\[\\/\\\\"')
addTest('"\\\n\\\r\n\\\n"')
addTest('\'\\\n\\\r\n\\\n\'')
addTest('  null')
addTest('true  ')
addTest('false')
addTest(' Infinity ')
addTest('+Infinity')
addTest('[]')
addTest('[ 0xA2, 0X024324AaBf]')
addTest('-0x12')
addTest('  [1,2,3,4,5]')
addTest('[1,2,3,4,5,]  ')
addTest('[1e-13]')
addTest('[null, true, false]')
addTest('  [1,2,"3,4,",5,]')
addTest('[ 1,\n2,"3,4,"  \r\n,\n5,]')
addTest('[  1  ,  2  ,  3  ,  4  ,  5  ,  ]')
addTest('{}  ')
addTest('{"2":1,"3":null,}')
addTest('{ "2 " : 1 , "3":null  , }')
addTest('{ \"2\"  : 25e245 ,  \"3\": 23 }')
addTest('{"2":1,"3":nul,}')
addTest('{:1,"3":nul,}')
addTest('[1,2] // ssssssssss 3,4,5,]  ')
addTest('[1,2 , // ssssssssss \n//xxx\n3,4,5,]  ')
addTest('[1,2 /* ssssssssss 3,4,*/ /* */ , 5 ]  ')
addTest('[1,2 /* ssssssssss 3,4,*/ /* * , 5 ]  ')
addTest('{"3":1,"3":,}')
addTest('{ чйуач:1, щцкшчлм  : 4,}')
addTest('{ qef-:1 }')
addTest('{ $$$:1 , ___: 3}')
addTest('{3:1,2:1}')
addTest('{3.4e3:1}')
addTest('{-3e3:1}')
addTest('{+3e3:1}')
addTest('{.3e3:1}')

for (var i=0; i<200; i++) {
  addTest('"' + String.fromCharCode(i) + '"', true)
}

// strict JSON test cases
addTest('"\\xaa"')
addTest('"\\0"')
addTest('"\0"')
addTest('"\\v"')
addTest('{null: 123}')
addTest("{'null': 123}")

assert.throws(function() {
  parse('0o')
})

assert.strictEqual(parse('01234567'), 342391)
assert.strictEqual(parse('0o1234567'), 342391)

// undef
assert.strictEqual(parse(undefined), undefined)

// whitespaces
addTest('[1,\r\n2,\r3,\n]')
'\u0020\u00A0\uFEFF\x09\x0A\x0B\x0C\x0D\u1680\u180E\u2000\u2001\u2002\u2003\u2004\u2005\u2006\u2007\u2008\u2009\u200A\u2028\u2029\u202F\u205F\u3000'.split('').forEach(function(x) {
  addTest(x+'[1,'+x+'2]'+x)
  addTest('"'+x+'"'+x)
})
'\u000A\u000D\u2028\u2029'.split('').forEach(function(x) {
  addTest(x+'[1,'+x+'2]'+x)
  addTest('"\\'+x+'"'+x)
})

/* weird ES6 stuff, not working

if (process.version > 'v0.11.7') {
  assert(Array.isArray(parse('{__proto__:[]}').__proto__))
  assert.equal(parse('{__proto__:{xxx:5}}').xxx, undefined)
  assert.equal(parse('{__proto__:{xxx:5}}').__proto__.xxx, 5)

  var o1 = parse('{"__proto__":[]}')
  assert.deepEqual([], o1.__proto__)
  assert.deepEqual(["__proto__"], Object.keys(o1))
  assert.deepEqual([], Object.getOwnPropertyDescriptor(o1, "__proto__").value)
  assert.deepEqual(["__proto__"], Object.getOwnPropertyNames(o1))
  assert(o1.hasOwnProperty("__proto__"))
  assert(Object.prototype.isPrototypeOf(o1))

  // Parse a non-object value as __proto__.
  var o2 = JSON.parse('{"__proto__":5}')
  assert.deepEqual(5, o2.__proto__)
  assert.deepEqual(["__proto__"], Object.keys(o2))
  assert.deepEqual(5, Object.getOwnPropertyDescriptor(o2, "__proto__").value)
  assert.deepEqual(["__proto__"], Object.getOwnPropertyNames(o2))
  assert(o2.hasOwnProperty("__proto__"))
  assert(Object.prototype.isPrototypeOf(o2))
}*/

assert.throws(parse.bind(null, "{-1:42}"))

for (var i=0; i<100; i++) {
  var str = '-01.e'.split('')

  var rnd = [1,2,3,4,5].map(function(x) {
    x = ~~(Math.random()*str.length)
    return str[x]
  }).join('')

  try {
    var x = parse(rnd)
  } catch(err) {
    x = 'fail'
  }
  try {
    var y = JSON.parse(rnd)
  } catch(err) {
    y = 'fail'
  }
  try {
    var z = eval(rnd)
  } catch(err) {
    z = 'fail'
  }
  //console.log(rnd, x, y, z)
  if (x !== y && x !== z) throw 'ERROR'
}
