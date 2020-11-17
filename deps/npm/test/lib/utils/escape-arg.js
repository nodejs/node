const requireInject = require('require-inject')
const t = require('tap')
const getEscape = win => requireInject('../../../lib/utils/escape-arg.js', {
  '../../../lib/utils/is-windows.js': win,
  path: require('path')[win ? 'win32' : 'posix'],
})

const winEscape = getEscape(true)
const nixEscape = getEscape(false)

t.equal(winEscape('hello/to the/world'), '"hello\\to the\\world"')
t.equal(nixEscape(`hello/to-the/world`), `hello/to-the/world`)
t.equal(nixEscape(`hello/to the/world`), `'hello/to the/world'`)
t.equal(nixEscape(`hello/to%the/world`), `'hello/to%the/world'`)
t.equal(nixEscape(`hello/to'the/world`), `'hello/to'"'"'the/world'`)
