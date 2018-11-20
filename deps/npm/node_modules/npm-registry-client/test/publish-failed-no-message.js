var createReadStream = require('fs').createReadStream

var test = require('tap').test

var server = require('./lib/server.js')
var common = require('./lib/common.js')
var config = { retry: { retries: 0 } }
var client = common.freshClient(config)

var URI = 'http://localhost:1337/'
var USERNAME = 'username'
var PASSWORD = '%1234@asdf%'
var EMAIL = 'i@izs.me'
var METADATA = require('../package.json')
var ACCESS = 'public'
// not really a tarball, but doesn't matter
var BODY_PATH = require.resolve('../package.json')
var BODY = createReadStream(BODY_PATH)
var AUTH = {
  username: USERNAME,
  password: PASSWORD,
  email: EMAIL
}
var PARAMS = {
  metadata: METADATA,
  access: ACCESS,
  body: BODY,
  auth: AUTH
}

test('publish with a 500 response but no message', function (t) {
  server.expect('/npm-registry-client', function (req, res) {
    res.statusCode = 500
    res.json({ success: false })
  })

  client.publish(URI, PARAMS, function (er, data) {
    t.ok(er, 'got expected error')
    t.notOk(data, 'no payload on failure')

    t.end()
  })
})
