'use strict'

const nock = require('nock')

// TODO (other tests actually make network calls today, which is bad)
// nock.disableNetConnect()

module.exports = tnock
function tnock (t, host) {
  const server = nock(host)
  t.teardown(function () {
    server.done()
  })
  return server
}
