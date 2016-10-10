var test = require('tap').test
var u = 'http://admin:admin@localhost:15986/registry/_design/scratch/_list/'
var http = require('http')
var parse = require('parse-json-response')

test('lists global packages', function(t) {
  http.get(u + 'index/listAll' , parse(function(er, data, res) {
    t.ok(data.package, 'global package listed')
    t.end()
  }))
})

test('hides scoped packages', function(t) {
  http.get(u + 'index/listAll' , parse(function(er, data, res) {
    t.notOk(data['@npm/package'], 'scoped package listed')
    t.end()
  }))
})
