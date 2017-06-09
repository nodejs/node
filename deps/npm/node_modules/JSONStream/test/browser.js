var test = require('tape')
var JSONStream = require('../')
var testData = '{"rows":[{"hello":"world"}, {"foo": "bar"}]}'

test('basic parsing', function (t) {
  t.plan(2)
  var parsed = JSONStream.parse("rows.*")
  var parsedKeys = {}
  parsed.on('data', function(match) {
    parsedKeys[Object.keys(match)[0]] = true
  })
  parsed.on('end', function() {
    t.equal(!!parsedKeys['hello'], true)
    t.equal(!!parsedKeys['foo'], true)
  })
  parsed.write(testData)
  parsed.end()
})