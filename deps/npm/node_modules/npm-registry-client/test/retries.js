var tap = require('tap')
var server = require('./fixtures/server.js')
var RC = require('../')
var pkg = { _id: 'some-package@1.2.3',
            name: 'some-package',
            version: '1.2.3' }
var client = new RC({
    'fetch-retries': 6
  , 'fetch-retry-mintimeout': 10
  , 'fetch-retry-maxtimeout': 100
  , cache: __dirname + '/fixtures/cache'
  , registry: 'http://localhost:' + server.port })

tap.test('create new user account', function (t) {
  // first time, return a 408
  server.expect('GET', '/some-package/1.2.3', function (req, res) {
    res.statusCode = 408
    res.end('Timeout')
  })
  // then, slam the door in their face
  server.expect('GET', '/some-package/1.2.3', function (req, res) {
    res.destroy()
  })
  // then, blame someone else
  server.expect('GET', '/some-package/1.2.3', function (req, res) {
    res.statusCode = 502
    res.end('Gateway Timeout')
  })
  // 'No one's home right now, come back later'
  server.expect('GET', '/some-package/1.2.3', function (req, res) {
    res.statusCode = 503
    res.setHeader('retry-after', '10')
    res.end('Come back later')
  })
  // finally, you may enter.
  server.expect('GET', '/some-package/1.2.3', function (req, res) {
    res.statusCode = 200
    res.json(pkg)
  })

  client.get('/some-package/1.2.3', function (er, data, raw, res) {
    if (er) throw er
    t.deepEqual(data, pkg)
    t.end()
  })
})
