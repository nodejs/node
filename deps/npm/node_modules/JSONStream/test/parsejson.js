

/*
 sometimes jsonparse changes numbers slightly.
*/

var r = Math.random()
  , Parser = require('jsonparse')
  , p = new Parser()
  , assert = require('assert')
  , times = 20
while (times --) {

  assert.equal(JSON.parse(JSON.stringify(r)), r, 'core JSON')

  p.onValue = function (v) {
    console.error('parsed', v)
    assert.equal(v,r)
  }
  console.error('correct', r)
  p.write (new Buffer(JSON.stringify([r])))



}
