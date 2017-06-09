var test = require('tap').test
var v = require('../npm-user-validate.js').username

test('username must be lowercase', function (t) {
  var err = v('ERRR')
  t.type(err, 'object')
  t.match(err.message, /lowercase/)
  t.end()
})

test('username may not contain non-url-safe chars', function (t) {
  var err = v('f  ')
  t.type(err, 'object')
  t.match(err.message, /url-safe/)
  t.end()
})

test('username may not start with "."', function (t) {
  var err = v('.username')
  t.type(err, 'object')
  t.match(err.message, /start with.*\./)
  t.end()
})

test('username may not be longer than 214 characters', function (t) {
  var err = v('bacon-ipsum-dolor-amet-tongue-short-loin-landjaeger-tenderloin-ball-tip-pork-loin-porchetta-pig-pork-chop-beef-ribs-pork-belly--shankle-t-bone-turducken-tongue-landjaeger-pork-loin-beef-chicken-short-loin-and-pickle')
  t.type(err, 'object')
  t.match(err.message, /less than or equal to 214/)
  t.end()
});

test('username may be as long as 214 characters', function (t) {
  var err = v('bacon-ipsum-dolor-amet-tongue-short-loin-landjaeger-tenderloin-ball-tip-pork-loin-porchetta-pig-pork-chop-beef-ribs-pork-belly--shankle-t-bone-turducken-tongue-landjaeger-pork-loin-beef-chicken-short-loin-porchetta')
  t.type(err, 'null')
  t.end()
});

test('username is ok', function (t) {
  var err = v('ente')
  t.type(err, 'null')
  t.end()
})
