'use strict'

const nock = require('nock')

// Uncomment this to find requests that aren't matching
// nock.emitter.on('no match', req => console.log(req.options))

module.exports = tnock
function tnock (t, host, opts) {
  nock.disableNetConnect()
  const server = nock(host, opts)
  t.teardown(function () {
    nock.enableNetConnect()
    server.done()
  })
  return server
}
