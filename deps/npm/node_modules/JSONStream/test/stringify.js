
var fs = require ('fs')
  , join = require('path').join
  , file = join(__dirname, 'fixtures','all_npm.json')
  , JSONStream = require('../')
  , it = require('it-is').style('colour')

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

var expected =  []
  , stringify = JSONStream.stringify()
  , es = require('event-stream')
  , stringified = ''
  , called = 0
  , count = 10
  , ended = false
  
while (count --)
  expected.push(randomObj())

  es.connect(
    es.readArray(expected),
    stringify,
    //JSONStream.parse([/./]),
    es.writeArray(function (err, lines) {
      
      it(JSON.parse(lines.join(''))).deepEqual(expected)
      console.error('PASSED')
    })
  )
