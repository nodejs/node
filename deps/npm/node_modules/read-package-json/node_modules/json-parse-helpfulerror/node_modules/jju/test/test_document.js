var assert = require('assert')
var create = require('../lib/document').Document
var jju = require('..')

var str = '{ x\r\n:\n1, y: {"..z.": 123, t: null, s:"123", a:[ 1,2,{x:3},] }}\n'
var d = create(str)
assert.equal(d + '', str)
assert.deepEqual(d.get(''), {x:1,y:{'..z.':123,t:null,s:'123',a:[1,2,{x:3}]}})
assert.deepEqual(d.get('x'), 1)
assert.deepEqual(d.get('x.x'), undefined)
assert.deepEqual(d.get('x.x.x.x'), undefined)
assert.strictEqual(d.get('y.x'), undefined)
assert.deepEqual(d.get('y.s'), '123')
assert.strictEqual(d.get('y.t'), null)
assert.strictEqual(d.get('y.t.x'), undefined)
assert.equal(d.has(''), true)
assert.equal(d.has('x'), true)
assert.equal(d.has('x.x'), false)
assert.equal(d.has('x.x.x.x'), false)
assert.equal(d.has('y.x'), false)
assert.equal(d.has('y'), true)
assert.equal(d.has('y.s'), true)
assert.equal(d.has('y.t'), true)
assert.equal(d.has('a'), false)

// arrays
assert.deepEqual(d.get('y.a'), [1,2,{x:3}])
assert.deepEqual(d.get('y.a.0'), 1)
assert.deepEqual(d.get('y.a.2.x'), 3)
assert.deepEqual(d.get('y.a.10'), undefined)
assert.deepEqual(d.has('y.a.0'), true)
assert.deepEqual(d.has('y.a.10'), false)
assert.deepEqual(d.get('y.a.2'), {x:3})
assert.deepEqual(d.get('y.a.-1'), undefined)

// controversial
assert.strictEqual(d.get('y.s.0'), undefined)
assert.equal(d.has('y.s.0'), false)

// paths
assert.deepEqual(d.get([]), {x:1,y:{'..z.':123,t:null,s:'123',a:[1,2,{x:3}]}})
assert.strictEqual(d.has([]), true)
assert.strictEqual(d.get(['y','..z.']), 123)
assert.strictEqual(d.has(['y','..z.']), true)
assert.deepEqual(d.get(['y','a',2,'x']), 3)
assert.deepEqual(create('[1]').set(0, 4).get(''), [4])
assert.deepEqual(create('[1]').set(1, 4).get(''), [1,4])
assert.deepEqual(create('[1]').has(0), true)
assert.deepEqual(create('[1]').has(1), false)
assert.deepEqual(create('[1]').get(0), 1)

// invalid paths
assert.throws(function() { create('[1]').set(null, 4) }, /invalid path type/i)
assert.throws(function() { create('[1]').set({}, 4) }, /invalid path type/i)
assert.throws(function() { create('[1]').set(/./, 4) }, /invalid path type/i)
assert.throws(function() { create('[1]').set(function(){}, 4) }, /invalid path type/i)
assert.throws(function() { create('[1]').set(false, 4) }, /invalid path type/i)
assert.throws(function() { create('[1]').set(undefined, 4) }, /invalid path type/i)

// set root
assert.strictEqual(create(str).set('', 4).get(''), 4)
assert.strictEqual(create(str).set('', null).get(''), null)
assert.strictEqual(create(str).set('', {x:4}).get('x'), 4)
assert.deepEqual(create(str).set('', [1,2,3]).get(''), [1,2,3])
assert.strictEqual(create('1').set('', 4).get(''), 4)
assert.strictEqual(create('null').set('', 4).get(''), 4)
assert.strictEqual(create('[]').set('', 4).get(''), 4)
assert.strictEqual(create('{}').set('', 4).get(''), 4)

// set 1st level
assert.deepEqual(create('{}').set('x', 4).get('x'), 4)
assert.deepEqual(create('{a:{b:[]}}').set('a.b.0', 4).get('a'), {b:[4]})
//assert.deepEqual(create('1').set('x', 4).get('x'), 4)
//assert.deepEqual(create('null').set('x', 4).get('x'), 4)

// array: boundaries
assert.strictEqual(create('[]').set('0', 4).get('0'), 4)
assert.strictEqual(create('[1,2,3]').set('2', 4).get('2'), 4)
assert.strictEqual(create('[1,2,3]').set('3', 4).get('3'), 4)

// various error cases
assert.throws(function() { create('1').set('x', 4) }, /set key .* of an non-object/)
assert.throws(function() { create('null').set('x', 4) }, /set key .* of an non-object/)
assert.throws(function() { create('[]').set('x', 4) }, /set key .* of an array/)
assert.throws(function() { create('""').set('x', 4) }, /set key .* of an non-object/)
assert.throws(function() { create('{}').set('x.x.x', 4) }, /set key .* of an non-object/)
assert.throws(function() { create('1').set('1', 4) }, /set key .* of an non-object/)
assert.throws(function() { create('null').set('1', 4) }, /set key .* of an non-object/)
assert.throws(function() { create('""').set('1', 4) }, /set key .* of an non-object/)
assert.throws(function() { create('[]').set('-1', 4) }, /set key .* of an array/)
assert.throws(function() { create('[]').set('1', 4) }, /set key .* out of bounds/)
assert.throws(function() { create('[1,2,3]').set('4', 4) }, /set key .* out of bounds/)
assert.throws(function() { create('{a:{b:[]}}').set('a.b.x', 4) }, /set key .* of an array/)

// unsetting stuff
assert.throws(function() { create('[]').unset('') }, /can't remove root document/)

// arrays: handling spaces correctly
assert.equal(create("[]").set(0,{})+"", '[{}]')
assert.equal(create("[0]").set(1,{})+"", '[0,{}]')
assert.equal(create("[0,]").set(1,{})+"", '[0,{},]')
assert.equal(create("[    ]").set(0,{})+"", '[{}    ]')
assert.equal(create("[ 0  ,  ]").set(1,{})+"", '[ 0  , {},  ]')
assert.equal(create("[ 0   ]").set(1,{})+"", '[ 0, {}   ]')
assert.equal(create("{}").set('y',{})+"", '{"y":{}}')
assert.equal(create("{x:1}").set('y',{})+"", '{x:1,y:{}}')
assert.equal(create("{x:1,}").set('y',{})+"", '{x:1,y:{},}')
assert.equal(create("{    }").set('y',{})+"", '{"y":{}    }')
assert.equal(create("{ x:1  ,  }").set('y',{})+"", '{ x:1  , y:{},  }')
assert.equal(create("{ x:1   }").set('y',{})+"", '{ x:1, y:{}   }')

// deleting elements
assert.throws(function() { create('[]').unset('0') }, /unset key .* out of bounds/)
assert.throws(function() { create('[1,2]').unset('2') }, /unset key .* out of bounds/)
assert.throws(function() { create('[1,2,3]').unset('0') }, /in the middle of an array/)

// CommonJS assert spec is "awesome"
assert.deepEqual(create('[1,2]').unset('1').get(''), [1])
assert.deepEqual(create('[1,2]').unset('1').get('').length, 1)
assert.deepEqual(create('[1,2,3]').unset('2').unset('1').get(''), [1])
assert.deepEqual(create('[1,2,3]').unset('2').unset('1').get('').length, 1)
assert.deepEqual(create('[1]').unset('0').get(''), [])
assert.deepEqual(create('[1]').unset('0').get('').length, 0)

assert.deepEqual(create('{x:{y:"z"}, z:4}').unset('x').get(''), {z:4})
assert.throws(function() { create('[1,2]').unset('') }, /root/)

// getting crazy
//assert.deepEqual(create(str).set('a.b.c.d.e', 1).get('a'), {b:{c:{d:{e:1}}}})

// update: arrays
assert.deepEqual(create("[1]").update([2,3])+"", '[2,3]')
assert.deepEqual(create("[1]").update([2,3,4])+"", '[2,3,4]')
assert.deepEqual(create("[]").update([2])+"", '[2]')
assert.deepEqual(create("[2]").update([])+"", '[]')
assert.deepEqual(create("[2,3,4]").update([2,3])+"", '[2,3]')
assert.deepEqual(create("[2,3,4]").update([])+"", '[]')
assert.deepEqual(create("[]").update([2,3,4])+"", '[2,3,4]')
assert.deepEqual(create(" /*zz*/ [ 2 , 3 , 4 ] /*xx*/ ").update([])+"", ' /*zz*/ [    ] /*xx*/ ')
assert.deepEqual(create(" /*zz*/ [ ] /*xx*/ ").update([2,3,4])+"", ' /*zz*/ [2,3,4 ] /*xx*/ ')

// update: objects
assert.deepEqual(create("{x:1}").update({x:1,y:2,z:3})+"", '{x:1,y:2,z:3}')
assert.deepEqual(create("{x:1}").update({x:2,z:3,t:4})+"", '{x:2,z:3,t:4}')
assert.deepEqual(create("{}").update({x:1,y:2})+"", '{"x":1,"y":2}')
assert.deepEqual(create("{x:1}").update({})+"", '{}')
assert.deepEqual(create("{x:1,y:2}").update({x:1})+"", '{x:1}')
assert.deepEqual(create(" /*zz*/ { x /*a*/ : /*b*/ 2 , y:3 , z //\n: 4  } /*xx*/ ").update({})+"", ' /*zz*/ {     } /*xx*/ ')
assert.deepEqual(create(" /*zz*/ { } /*xx*/ ").update({x: 2, y: 3, z: 4})+"", ' /*zz*/ {"x":2,"y":3,"z":4 } /*xx*/ ')

// remove trailing comma
assert.deepEqual(create("{x:1,}").update({})+"", '{}')
assert.deepEqual(create("[0,]").update([])+"", '[]')
assert.deepEqual(create("[0 /*z*/ , /*z*/]").update([])+"", '[ /*z*/]')

// mode
assert.equal(create('{"test":123}', {mode:'json'}).update({q:1,w:2})+'', '{"q":1,"w":2}')

assert.equal(create('{1:2}').update({ a: 1, b: [1,2], c: 3})+'', '{a:1,b:[1,2],c:3}')

// undef
//assert.throws(function(){ jju.update(undefined, undefined) }, /root doc/)
assert.equal(jju.update(undefined, undefined), '')
assert.equal(jju.update(undefined, 42), '42')
assert.equal(jju.update(undefined, {x: 5}), '{"x":5}')

/*
 *  real test
 */
var upd = { name: 'yapm',
  version: '0.6.0',
  description: 'npm wrapper allowing to use package.yaml instead of package.json',
  author: { name: 'Alex Kocharin', email: 'alex@kocharin.ru' },
  keywords:
   [ 'package manager',
     'modules',
     'install',
     'package.yaml',
     'package.json5',
     'yaml',
     'json5',
     'npm' ],
  preferGlobal: true,
  homepage: 'https://npmjs.org/doc/',
  repository: { type: 'git', url: 'https://github.com/rlidwka/yapm' },
  bugs: { url: 'http://github.com/rlidwka/yapm/issues' },
  main: './yapm.js',
  bin: { yapm: './yapm.js' },
  dependencies: { npm: '*', 'js-yaml': '*', through: '*', 'json5-utils': '*' },
  devDependencies: { async: '*' },
  optionalDependencies: { 'yaml-update': '*' },
  test_nonascii: 'тест' }

assert.deepEqual(create(create('{"garbage":"garbage"}').update(upd)).get(''), upd)
assert.deepEqual(JSON.parse(create('{"garbage":"garbage"}', {mode:'json',legacy:true}).update(upd)), upd)

//console.log(create('{"garbage":"garbage"}').update(upd)+'')

//assert.deepEqual(create("  [  ]  //").set(0,{})+""  [  ,{}]  //


//node -e 'console.log(require("./document").Document("{}").set("",[1,2,3])+"")'[1, 2, 3]

//alex@elu:~/json5-utils/lib$ node -e 'console.log(require("./document").Document("[]").set("0",[1,2,3]).get(""))'
//[ [ 1, 2, 3 ] ]


/*assert.equal(create('"test"').get(''), 'test')
assert.equal(create('"test"').get([]), 'test')
assert.equal(create('"test"').get(false), 'test')
assert.equal(create(undefined).get(''), undefined)

//assert.equal(create('"test"').set('', 'foo').toString(), '"foo"')
*/
