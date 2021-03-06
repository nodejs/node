const t = require('tap')
const requireInject = require('require-inject')
const mod = '../../../lib/utils/path.js'
const delim = require('../../../lib/utils/is-windows.js') ? ';' : ':'
Object.defineProperty(process, 'env', {
  value: {},
})
process.env.path = ['foo', 'bar', 'baz'].join(delim)
t.strictSame(requireInject(mod), ['foo', 'bar', 'baz'])
process.env.Path = ['a', 'b', 'c'].join(delim)
t.strictSame(requireInject(mod), ['a', 'b', 'c'])
process.env.PATH = ['x', 'y', 'z'].join(delim)
t.strictSame(requireInject(mod), ['x', 'y', 'z'])
