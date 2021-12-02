const t = require('tap')
const index = require.resolve('../index.js')
const packageIndex = require.resolve('../')

t.equal(index, packageIndex, 'index is main package require() export')
t.throws(() => require(index), {
  message: 'The programmatic API was removed in npm v8.0.0',
})

t.test('loading as main module will load the cli', t => {
  const cwd = t.testdir()
  const { spawn } = require('child_process')
  const LS = require('../lib/commands/ls.js')
  const ls = new LS({})
  const p = spawn(process.execPath, [index, 'ls', '-h', '--cache', cwd])
  const out = []
  p.stdout.on('data', c => out.push(c))
  p.on('close', (code, signal) => {
    t.equal(code, 0)
    t.equal(signal, null)
    t.match(Buffer.concat(out).toString(), ls.usage)
    t.end()
  })
})
