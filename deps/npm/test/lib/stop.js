const t = require('tap')
const stop = require('../../lib/stop.js')
t.isa(stop, Function)
t.equal(stop.usage, 'npm stop [-- <args>]')
