const t = require('tap')
const logs = []
console.log = (...msg) => logs.push(msg)
const output = require('../../../lib/utils/output.js')
output('hello', 'world')
output('hello')
output('world')
t.strictSame(logs, [['hello', 'world'], ['hello'], ['world']])
