const { test } = require('tap')
const requireInject = require('require-inject')
const mockNpm = require('../fixtures/mock-npm')

test('bin', (t) => {
  t.plan(4)
  const dir = '/bin/dir'

  const Bin = require('../../lib/bin.js')

  const npm = mockNpm({
    bin: dir,
    config: { global: false },
    output: (output) => {
      t.equal(output, dir, 'prints the correct directory')
    },
  })
  const bin = new Bin(npm)
  t.match(bin.usage, 'bin', 'usage has command name in it')

  bin.exec([], (err) => {
    t.ifError(err, 'npm bin')
    t.ok('should have printed directory')
  })
})

test('bin -g', (t) => {
  t.plan(3)
  const consoleError = console.error
  t.tearDown(() => {
    console.error = consoleError
  })

  console.error = (output) => {
    t.fail('should not have printed to console.error')
  }
  const dir = '/bin/dir'

  const Bin = requireInject('../../lib/bin.js', {
    '../../lib/utils/path.js': [dir],
  })

  const npm = mockNpm({
    bin: dir,
    config: { global: true },
    output: (output) => {
      t.equal(output, dir, 'prints the correct directory')
    },
  })
  const bin = new Bin(npm)

  bin.exec([], (err) => {
    t.ifError(err, 'npm bin')
    t.ok('should have printed directory')
  })
})

test('bin -g (not in path)', (t) => {
  t.plan(4)
  const consoleError = console.error
  t.tearDown(() => {
    console.error = consoleError
  })

  console.error = (output) => {
    t.equal(output, '(not in PATH env variable)', 'prints env warning')
  }
  const dir = '/bin/dir'

  const Bin = requireInject('../../lib/bin.js', {
    '../../lib/utils/path.js': ['/not/my/dir'],
  })
  const npm = mockNpm({
    bin: dir,
    config: { global: true },
    output: (output) => {
      t.equal(output, dir, 'prints the correct directory')
    },
  })
  const bin = new Bin(npm)

  bin.exec([], (err) => {
    t.ifError(err, 'npm bin')
    t.ok('should have printed directory')
  })
})
