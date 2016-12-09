var test = require('tap').test
var concat = require('concat-stream')

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var client = common.freshClient()

var testData = JSON.stringify({test: true})
var errorData = JSON.stringify({error: 'it went bad'})

test('streaming fetch', function (t) {
  server.expect('/test', function (req, res) {
    t.equal(req.method, 'GET', 'got expected method')

    res.writeHead(200, {
      'content-type': 'application/json'
    })

    res.end(testData)
  })

  server.expect('/error', function (req, res) {
    t.equal(req.method, 'GET', 'got expected method')

    res.writeHead(401, {
      'content-type': 'application/json'
    })

    res.end(errorData)
  })

  client.fetch(
    'http://localhost:1337/test',
    { streaming: true },
    function (er, res) {
      t.ifError(er, 'loaded successfully')

      var sink = concat(function (data) {
        t.deepEqual(data.toString(), testData)
        client.fetch(
          'http://localhost:1337/error',
          { streaming: true },
          function (er, res) {
            t.ok(er, 'got an error')
            server.close()
            t.end()
          }
        )
      })

      res.on('error', function (error) {
        t.ifError(error, 'no errors on stream')
      })

      res.pipe(sink)
    }
  )
})
