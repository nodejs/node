const { test } = require('tap')
const requireInject = require('require-inject')

test('bin', (t) => {
  t.plan(3)
  const dir = '/bin/dir'

  const Bin = requireInject('../../lib/bin.js', {
    '../../lib/utils/output.js': (output) => {
      t.equal(output, dir, 'prints the correct directory')
    },
  })

  const bin = new Bin({ bin: dir, flatOptions: { global: false } })

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
    '../../lib/utils/output.js': (output) => {
      t.equal(output, dir, 'prints the correct directory')
    },
  })

  const bin = new Bin({ bin: dir, flatOptions: { global: true } })

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
    '../../lib/utils/output.js': (output) => {
      t.equal(output, dir, 'prints the correct directory')
    },
  })
  const bin = new Bin({ bin: dir, flatOptions: { global: true } })

  bin.exec([], (err) => {
    t.ifError(err, 'npm bin')
    t.ok('should have printed directory')
  })
})
