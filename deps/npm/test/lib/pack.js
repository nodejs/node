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
  const Pack = requireInject('../../lib/pack.js', {
    '../../lib/utils/output.js': output,
    libnpmpack,
    npmlog: {
      notice: () => {},
      showProgress: () => {},
      clearProgress: () => {},
    },
  })
  const pack = new Pack({
    flatOptions: {
      unicode: false,
      json: false,
      dryRun: false,
    },
  })

  pack.exec([], er => {
    if (er)
      throw er

    const filename = `npm-${require('../../package.json').version}.tgz`
    t.strictSame(OUTPUT, [[filename]])
    t.end()
  })
})

t.test('should pack given directory', (t) => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'my-cool-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  const Pack = requireInject('../../lib/pack.js', {
    '../../lib/utils/output.js': output,
    libnpmpack,
    npmlog: {
      notice: () => {},
      showProgress: () => {},
      clearProgress: () => {},
    },
  })
  const pack = new Pack({
    flatOptions: {
      unicode: true,
      json: true,
      dryRun: true,
    },
  })

  pack.exec([testDir], er => {
    if (er)
      throw er

    const filename = 'my-cool-pkg-1.0.0.tgz'
    t.strictSame(OUTPUT, [[filename]])
    t.end()
  })
})

t.test('should pack given directory for scoped package', (t) => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: '@cool/my-pkg',
      version: '1.0.0',
    }, null, 2),
  })

  const Pack = requireInject('../../lib/pack.js', {
    '../../lib/utils/output.js': output,
    libnpmpack,
    npmlog: {
      notice: () => {},
      showProgress: () => {},
      clearProgress: () => {},
    },
  })
  const pack = new Pack({
    flatOptions: {
      unicode: true,
      json: true,
      dryRun: true,
    },
  })

  return pack.exec([testDir], er => {
    if (er)
      throw er

    const filename = 'cool-my-pkg-1.0.0.tgz'
    t.strictSame(OUTPUT, [[filename]])
    t.end()
  })
})

t.test('should log pack contents', (t) => {
  const Pack = requireInject('../../lib/pack.js', {
    '../../lib/utils/output.js': output,
    '../../lib/utils/tar.js': {
      ...require('../../lib/utils/tar.js'),
      logTar: () => {
        t.ok(true, 'logTar is called')
      },
    },
    libnpmpack,
    npmlog: {
      notice: () => {},
      showProgress: () => {},
      clearProgress: () => {},
    },
  })
  const pack = new Pack({
    flatOptions: {
      unicode: false,
      json: false,
      dryRun: false,
    },
  })

  pack.exec([], er => {
    if (er)
      throw er

    const filename = `npm-${require('../../package.json').version}.tgz`
    t.strictSame(OUTPUT, [[filename]])
    t.end()
  })
})
