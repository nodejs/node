var test = require('tap').test

var common = require('./lib/common.js')
var client = common.freshClient()
var server = require('./lib/server.js')

function nop () {}

var metricId = 'this-is-a-uuid'
var from = new Date()
var to = new Date()
var metricInfo = {
  metricId: metricId,
  metrics: [{
    from: from.toISOString(),
    to: to.toISOString(),
    successfulInstalls: 0,
    failedInstalls: 1
  }]
}

test('sendAnonymousCLIMetrics call contract', function (t) {
  t.throws(function () {
    client.sendAnonymousCLIMetrics(undefined, metricInfo, nop)
  }, 'requires a URI')

  t.throws(function () {
    client.sendAnonymousCLIMetrics([], metricInfo, nop)
  }, 'requires URI to be a string')

  t.throws(function () {
    client.sendAnonymousCLIMetrics(common.registry, undefined, nop)
  }, 'requires params object')

  t.throws(function () {
    client.sendAnonymousCLIMetrics(common.registry, '', nop)
  }, 'params must be object')

  t.throws(function () {
    client.sendAnonymousCLIMetrics(common.registry, metricInfo, undefined)
  }, 'requires callback')

  t.throws(function () {
    client.sendAnonymousCLIMetrics(common.registry, metricInfo, 'callback')
  }, 'callback must be function')

  t.end()
})

test('sendAnonymousCLIMetrics', function (t) {
  server.expect('PUT', '/-/npm/anon-metrics/v1/' + metricId, function (req, res) {
    t.is(req.method, 'PUT')
    var b = ''
    req.setEncoding('utf8')
    req.on('data', function (d) {
      b += d
    })

    req.on('end', function () {
      var d = JSON.parse(b)
      t.isDeeply(d, metricInfo.metrics, 'PUT received metrics')

      res.statusCode = 200
      res.json({})
    })
  })

  client.sendAnonymousCLIMetrics(common.registry, metricInfo, function (err, res) {
    t.ifError(err, 'no errors')
    t.isDeeply(res, {}, 'result ok')
    t.end()
  })
})

test('cleanup', function (t) {
  server.close()
  t.end()
})
