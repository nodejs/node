var server = require('./server.js')
var RC = require('../../')
var REGISTRY = 'http://localhost:' + server.port

// cheesy hackaround for test deps (read: nock) that rely on setImmediate
if (!global.setImmediate || !require('timers').setImmediate) {
  require('timers').setImmediate = global.setImmediate = function () {
    var args = [arguments[0], 0].concat([].slice.call(arguments, 1))
    setTimeout.apply(this, args)
  }
}

module.exports = {
  port: server.port,
  registry: REGISTRY,
  freshClient: function freshClient (config) {
    var client = new RC(config)
    server.log = client.log
    client.log.level = 'silent'

    return client
  }
}
