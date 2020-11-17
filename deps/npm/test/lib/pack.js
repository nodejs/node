const t = require('tap')
const requireInject = require('require-inject')

const OUTPUT = []
const output = (...msg) => OUTPUT.push(msg)

const libnpmpack = async (spec, opts) => {
  if (!opts)
    throw new Error('expected options object')

  return ''
}

t.afterEach(cb => {
  OUTPUT.length = 0
  cb()
})

t.test('should pack current directory with no arguments', (t) => {
  const pack = requireInject('../../lib/pack.js', {
    '../../lib/utils/output.js': output,
    '../../lib/npm.js': {
      flatOptions: {
        unicode: false,
        json: false,
        dryRun: false,
      },
    },
    libnpmpack,
    npmlog: {
      notice: () => {},
      showProgress: () => {},
      clearProgress: () => {},
    },
  })

  return pack([], er => {
    if (er)
      throw er

    const filename = `npm-${require('../../package.json').version}.tgz`
    t.strictSame(OUTPUT, [[filename]])
  })
})

t.test('should pack given directory', (t) => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  const pack = requireInject('../../lib/pack.js', {
    '../../lib/utils/output.js': output,
    '../../lib/npm.js': {
      flatOptions: {
        unicode: true,
        json: true,
        dryRun: true,
      },
    },
    libnpmpack,
    npmlog: {
      notice: () => {},
      showProgress: () => {},
      clearProgress: () => {},
    },
  })

  return pack([testDir], er => {
    if (er)
      throw er

    const filename = 'my-cool-pkg-1.0.0.tgz'
    t.strictSame(OUTPUT, [[filename]])
  })
})

t.test('should pack given directory for scoped package', (t) => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: '@cool/my-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  const pack = requireInject('../../lib/pack.js', {
    '../../lib/utils/output.js': output,
    '../../lib/npm.js': {
      flatOptions: {
        unicode: true,
        json: true,
        dryRun: true,
      },
    },
    libnpmpack,
    npmlog: {
      notice: () => {},
      showProgress: () => {},
      clearProgress: () => {},
    },
  })

  return pack([testDir], er => {
    if (er)
      throw er

    const filename = 'cool-my-pkg-1.0.0.tgz'
    t.strictSame(OUTPUT, [[filename]])
  })
})

t.test('should log pack contents', (t) => {
  const pack = requireInject('../../lib/pack.js', {
    '../../lib/utils/output.js': output,
    '../../lib/utils/tar.js': {
      ...require('../../lib/utils/tar.js'),
      logTar: () => {
        t.ok(true, 'logTar is called')
      },
    },
    '../../lib/npm.js': {
      flatOptions: {
        unicode: false,
        json: false,
        dryRun: false,
      },
    },
    libnpmpack,
    npmlog: {
      notice: () => {},
      showProgress: () => {},
      clearProgress: () => {},
    },
  })

  return pack([], er => {
    if (er)
      throw er

    const filename = `npm-${require('../../package.json').version}.tgz`
    t.strictSame(OUTPUT, [[filename]])
  })
})
