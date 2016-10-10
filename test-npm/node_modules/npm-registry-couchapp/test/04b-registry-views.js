var expect = require('./04b-registry-views-expect.js')

var test = require('tap').test
var u = 'http://admin:admin@localhost:15986/registry/_design/scratch/_view/'
var http = require('http')
var parse = require('parse-json-response')

Object.keys(expect).forEach(function(view) {
  test(view, function(t) {
    http.get(u + view, parse(function(er, data, res) {
      if (er)
        throw er
      expect[view] = data
      t.similar(data, expect[view])
      t.end()
    }))
  })
})
