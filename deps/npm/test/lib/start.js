const t = require('tap')
const start = require('../../lib/start.js')
t.isa(start, Function)
t.equal(start.usage, 'npm start [-- <args>]')
