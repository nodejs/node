var server = require('./server.js')
var RC = require('../../')
var REGISTRY = 'http://localhost:' + server.port

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
