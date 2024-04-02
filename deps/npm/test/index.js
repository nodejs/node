const t = require('tap')
const spawn = require('@npmcli/promise-spawn')
const index = require.resolve('../index.js')
const packageIndex = require.resolve('../')
const { load: loadMockNpm } = require('./fixtures/mock-npm')

t.equal(index, packageIndex, 'index is main package require() export')
t.throws(() => require(index), {
  message: 'The programmatic API was removed in npm v8.0.0',
})

t.test('loading as main module will load the cli', async t => {
  const { npm, cache } = await loadMockNpm(t)
  const LS = require('../lib/commands/ls.js')
  const ls = new LS(npm)
  const p = await spawn(process.execPath, [index, 'ls', '-h', '--cache', cache])
  t.equal(p.code, 0)
  t.equal(p.signal, null)
  t.match(p.stdout, ls.usage)
})
