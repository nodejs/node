const t = require('tap')
const mockGlobals = require('@npmcli/mock-globals')
const tmock = require('../fixtures/tmock')

const npm = require.resolve('../../bin/npm-cli.js')
const npx = require.resolve('../../bin/npx-cli.js')

const mockNpx = (t, argv) => {
  const logs = []
  mockGlobals(t, {
    'process.argv': argv,
    'console.error': (...msg) => logs.push(msg),
  })
  tmock(t, '{BIN}/npx-cli.js', { '{LIB}/cli.js': () => {} })
  return {
    logs,
    argv: process.argv,
  }
}

t.test('npx foo -> npm exec -- foo', async t => {
  const { argv } = mockNpx(t, ['node', npx, 'foo'])
  t.strictSame(argv, ['node', npm, 'exec', '--', 'foo'])
})

t.test('npx -- foo -> npm exec -- foo', async t => {
  const { argv } = mockNpx(t, ['node', npx, '--', 'foo'])
  t.strictSame(argv, ['node', npm, 'exec', '--', 'foo'])
})

t.test('npx -x y foo -z -> npm exec -x y -- foo -z', async t => {
  const { argv } = mockNpx(t, ['node', npx, '-x', 'y', 'foo', '-z'])
  t.strictSame(argv, ['node', npm, 'exec', '-x', 'y', '--', 'foo', '-z'])
})

t.test('npx --x=y --no-install foo -z -> npm exec --x=y -- foo -z', async t => {
  const { argv } = mockNpx(t, ['node', npx, '--x=y', '--no-install', 'foo', '-z'])
  t.strictSame(argv, ['node', npm, 'exec', '--x=y', '--yes=false', '--', 'foo', '-z'])
})

t.test('transform renamed options into proper values', async t => {
  const { argv } = mockNpx(t, ['node', npx, '-y', '--shell=bash', '-p', 'foo', '-c', 'asdf'])
  t.strictSame(argv, [
    'node',
    npm,
    'exec',
    '--yes',
    '--script-shell=bash',
    '--package',
    'foo',
    '--call',
    'asdf',
  ])
})

// warn if deprecated switches/options are used
t.test('use a bunch of deprecated switches and options', async t => {
  const { argv, logs } = mockNpx(t, [
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
  ])

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
  t.strictSame(argv, expect)
  t.strictSame(logs, [
    ['npx: the --npm argument has been removed.'],
    ['npx: the --node-arg argument has been removed.'],
    ['npx: the --n argument has been removed.'],
    ['npx: the --always-spawn argument has been removed.'],
    ['npx: the --shell-auto-fallback argument has been removed.'],
    ['npx: the --ignore-existing argument has been removed.'],
    ['See `npm help exec` for more information'],
  ])
})
