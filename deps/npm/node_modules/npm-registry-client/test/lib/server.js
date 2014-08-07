// a fake registry server.

var http = require('http')
var server = http.createServer(handler)
var port = server.port = process.env.PORT || 1337
server.listen(port)

module.exports = server

server._expect = {}

function handler (req, res) {
  req.connection.setTimeout(1000)

  var u = '* ' + req.url
  , mu = req.method + ' ' + req.url

  var k = server._expect[mu] ? mu : server._expect[u] ? u : null
  if (!k) throw Error('unexpected request: ' + req.method + ' ' + req.url)

  var fn = server._expect[k].shift()
  if (!fn) throw Error('unexpected request' + req.method + ' ' + req.url)


  var remain = (Object.keys(server._expect).reduce(function (s, k) {
    return s + server._expect[k].length
  }, 0))
  if (remain === 0) server.close()
  else this.log.info("fake-registry", "TEST SERVER: %d reqs remain", remain)
  this.log.info("fake-registry", Object.keys(server._expect).map(function(k) {
    return [k, server._expect[k].length]
  }).reduce(function (acc, kv) {
    acc[kv[0]] = kv[1]
    return acc
  }, {}))

  res.json = json
  fn(req, res)
}

function json (o) {
  this.setHeader('content-type', 'application/json')
  this.end(JSON.stringify(o))
}

// this log is meanto to be overridden
server.log = require("npmlog")

server.expect = function (method, u, fn) {
  if (typeof u === 'function') {
    fn = u
    u = method
    method = '*'
  }
  u = method + ' ' + u
  server._expect[u] = server._expect[u] || []
  server._expect[u].push(fn)
}
