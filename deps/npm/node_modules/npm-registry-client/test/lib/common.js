var resolve = require("path").resolve
var server = require('./server.js')
var RC = require('../../')

module.exports = {
  freshClient : function freshClient(config) {
    config = config || {}
    config.cache = resolve(__dirname, '../fixtures/cache')
    config.registry = 'http://localhost:' + server.port

    var client = new RC(config)
    server.log = client.log
    client.log.level = 'silent'

    return client
  }
}
