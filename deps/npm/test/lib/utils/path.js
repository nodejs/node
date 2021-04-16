const t = require('tap')
const mod = '../../../lib/utils/path.js'
const delim = require('../../../lib/utils/is-windows.js') ? ';' : ':'
Object.defineProperty(process, 'env', {
  value: {},
})
process.env.path = ['foo', 'bar', 'baz'].join(delim)
t.strictSame(t.mock(mod), ['foo', 'bar', 'baz'])
process.env.Path = ['a', 'b', 'c'].join(delim)
t.strictSame(t.mock(mod), ['a', 'b', 'c'])
process.env.PATH = ['x', 'y', 'z'].join(delim)
t.strictSame(t.mock(mod), ['x', 'y', 'z'])
