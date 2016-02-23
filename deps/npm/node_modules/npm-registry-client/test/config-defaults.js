var test = require('tap').test

require('./lib/server.js').close()
var common = require('./lib/common.js')

test('config defaults', function (t) {
  var client = common.freshClient()

  var proxy = client.config.proxy
  t.notOk(proxy.http, 'no default value for HTTP proxy')
  t.notOk(proxy.https, 'no default value for HTTPS proxy')
  t.notOk(proxy.localAddress, 'no default value for local address')

  var ssl = client.config.ssl
  t.notOk(ssl.ca, 'no default value for SSL certificate authority bundle')
  t.notOk(ssl.certificate, 'no default value for SSL client certificate')
  t.notOk(ssl.key, 'no default value for SSL client certificate key')
  t.equal(ssl.strict, true, 'SSL is strict by default')

  var retry = client.config.retry
  t.equal(retry.retries, 2, 'default retry count is 2')
  t.equal(retry.factor, 10, 'default retry factor is 10')
  t.equal(retry.minTimeout, 10000, 'retry minimum timeout is 10000 (10 seconds)')
  t.equal(retry.maxTimeout, 60000, 'retry maximum timeout is 60000 (60 seconds)')

  t.equal(client.config.userAgent, 'node/' + process.version, 'default userAgent')
  t.ok(client.log.info, "there's a default logger")
  t.equal(client.config.defaultTag, 'latest', 'default tag is "latest"')
  t.notOk(client.config.couchToken, 'no couchToken by default')
  t.notOk(client.config.sessionToken, 'no sessionToken by default')

  t.end()
})

test('missing HTTPS proxy defaults to HTTP proxy', function (t) {
  var client = common.freshClient({ proxy: { http: 'http://proxy.npm:8088/' }})

  t.equal(client.config.proxy.http, 'http://proxy.npm:8088/', 'HTTP proxy set')
  t.equal(client.config.proxy.http, client.config.proxy.https, 'HTTP === HTTPS')

  t.end()
})
