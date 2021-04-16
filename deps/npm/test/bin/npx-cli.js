const t = require('tap')
const npx = require.resolve('../../bin/npx-cli.js')
const cli = require.resolve('../../lib/cli.js')
const npm = require.resolve('../../bin/npm-cli.js')

const logs = []
console.error = (...msg) => logs.push(msg)

t.afterEach(() => logs.length = 0)

t.test('npx foo -> npm exec -- foo', t => {
  process.argv = ['node', npx, 'foo']
  t.mock(npx, { [cli]: () => {} })
  t.strictSame(process.argv, ['node', npm, 'exec', '--', 'foo'])
  t.end()
})

t.test('npx -- foo -> npm exec -- foo', t => {
  process.argv = ['node', npx, '--', 'foo']
  t.mock(npx, { [cli]: () => {} })
  t.strictSame(process.argv, ['node', npm, 'exec', '--', 'foo'])
  t.end()
})

t.test('npx -x y foo -z -> npm exec -x y -- foo -z', t => {
  process.argv = ['node', npx, '-x', 'y', 'foo', '-z']
  t.mock(npx, { [cli]: () => {} })
  t.strictSame(process.argv, ['node', npm, 'exec', '-x', 'y', '--', 'foo', '-z'])
  t.end()
})

t.test('npx --x=y --no-install foo -z -> npm exec --x=y -- foo -z', t => {
  process.argv = ['node', npx, '--x=y', '--no-install', 'foo', '-z']
  t.mock(npx, { [cli]: () => {} })
  t.strictSame(process.argv, ['node', npm, 'exec', '--x=y', '--yes=false', '--', 'foo', '-z'])
  t.end()
})

t.test('transform renamed options into proper values', t => {
  process.argv = ['node', npx, '-y', '--shell=bash', '-p', 'foo', '-c', 'asdf']
  t.mock(npx, { [cli]: () => {} })
  t.strictSame(process.argv, ['node', npm, 'exec', '--yes', '--script-shell=bash', '--package', 'foo', '--call', 'asdf'])
  t.end()
})

// warn if deprecated switches/options are used
t.test('use a bunch of deprecated switches and options', t => {
  process.argv = [
    'node',
    npx,
    '--npm',
    '/some/npm/bin',
    '--node-arg=--harmony',
    '-n',
    '--require=foobar',
    '--reg=http://localhost:12345/',
    '-p',
    'foo',
    '--always-spawn',
    '--shell-auto-fallback',
    '--ignore-existing',
    '-q',
    'foobar',
  ]

  const expect = [
    'node',
    npm,
    'exec',
    '--registry',
    'http://localhost:12345/',
    '--package',
    'foo',
    '--loglevel',
    'warn',
    '--',
    'foobar',
  ]
  t.mock(npx, { [cli]: () => {} })
  t.strictSame(process.argv, expect)
  t.strictSame(logs, [
    ['npx: the --npm argument has been removed.'],
    ['npx: the --node-arg argument has been removed.'],
    ['npx: the --n argument has been removed.'],
    ['npx: the --always-spawn argument has been removed.'],
    ['npx: the --shell-auto-fallback argument has been removed.'],
    ['npx: the --ignore-existing argument has been removed.'],
    ['See `npm help exec` for more information'],
  ])
  t.end()
})
