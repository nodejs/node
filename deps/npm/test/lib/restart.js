const t = require('tap')
const restart = require('../../lib/restart.js')
t.isa(restart, Function)
t.equal(restart.usage, 'npm restart [-- <args>]')
