var expect = {
  'userByEmail?group=true' : {
    "rows": [
      {
        "key": [
          "3@example.com",
          "third"
        ],
        "value": 1
      },
      {
        "key": [
          "email@example.com",
          "user"
        ],
        "value": 1
      },
      {
        "key": [
          "other@example.com",
          "other"
        ],
        "value": 1
      }
    ]
  },
  'invalidUser': {
    "total_rows": 0,
    "offset": 0,
    "rows": []
  },
  'invalid?group=true': { "rows": [] },
  'conflicts?group=true': { "rows": [] }
}

var test = require('tap').test
var u = 'http://admin:admin@localhost:15986/_users/_design/scratch/_view/'
var http = require('http')
var parse = require('parse-json-response')

Object.keys(expect).forEach(function(view) {
  test(view, function(t) {
    http.get(u + view, parse(function(er, data, res) {
      if (er)
        throw er
      t.same(data, expect[view])
      t.end()
    }))
  })
})
