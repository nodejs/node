
var fs = require ('fs')
  , join = require('path').join
  , file = join(__dirname, 'fixtures','all_npm.json')
  , JSONStream = require('../')
  , it = require('it-is').style('colour')
  , es = require('event-stream')
  , pending = 10
  , passed = true

  function randomObj () {
    return (
      Math.random () < 0.4
      ? {hello: 'eonuhckmqjk',
          whatever: 236515,
          lies: true,
          nothing: [null],
          stuff: [Math.random(),Math.random(),Math.random()]
        }
      : ['AOREC', 'reoubaor', {ouec: 62642}, [[[], {}, 53]]]
    )
  }

for (var ix = 0; ix < pending; ix++) (function (count) {
  var expected =  {}
    , stringify = JSONStream.stringifyObject()

  es.connect(
    stringify,
    es.writeArray(function (err, lines) {
      it(JSON.parse(lines.join(''))).deepEqual(expected)
      if (--pending === 0) {
        console.error('PASSED')
      }
    })
  )

  while (count --) {
    var key = Math.random().toString(16).slice(2)
    expected[key] = randomObj()
    stringify.write([ key, expected[key] ])
  }

  process.nextTick(function () {
    stringify.end()
  })
})(ix)
