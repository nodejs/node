var tap = require('tap')
var readJson = require('../')
var path = require('path')
var p = path.resolve(__dirname, 'fixtures/erroneous.json')

tap.test('erroneous package data', function (t) {
  readJson(p, function (er, data) {
    t.ok(er instanceof Error)
    t.ok(er.message.match(/Unexpected token '\\''/))
    t.end()
  })
})
