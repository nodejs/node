const t = require('tap')
const requireInject = require('require-inject')

let result = []

const noop = () => null
const npm = {
  flatOptions: {
    tagVersionPrefix: 'v',
    json: false,
  },
  prefix: '',
  version: '1.0.0',
}
const mocks = {
  libnpmversion: noop,
  '../../lib/utils/output.js': (...msg) => {
    for (const m of msg)
      result.push(m)
  },
  '../../lib/utils/usage.js': () => 'usage instructions',
}

const Version = requireInject('../../lib/version.js', mocks)
const version = new Version(npm)

const _processVersions = process.versions
t.afterEach(cb => {
  npm.flatOptions.json = false
  npm.prefix = ''
  process.versions = _processVersions
  result = []
  cb()
})

t.test('no args', t => {
  const prefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'test-version-no-args',
      version: '3.2.1',
    }),
  })
  npm.prefix = prefix
  Object.defineProperty(process, 'versions', { value: { node: '1.0.0' } })

  version.exec([], err => {
    if (err)
      throw err

    t.deepEqual(
      result,
      [{
        'test-version-no-args': '3.2.1',
        node: '1.0.0',
        npm: '1.0.0',
      }],
      'should output expected values for various versions in npm'
    )

    t.end()
  })
})

t.test('too many args', t => {
  version.exec(['foo', 'bar'], err => {
    t.match(
      err,
      'usage instructions',
      'should throw usage instructions error'
    )

    t.end()
  })
})

t.test('completion', async t => {
  const testComp = async (argv, expect) => {
    const res = await version.completion({ conf: { argv: { remain: argv } } })
    t.strictSame(res, expect, argv.join(' '))
  }

  await testComp(['npm', 'version'], [
    'major',
    'minor',
    'patch',
    'premajor',
    'preminor',
    'prepatch',
    'prerelease',
    'from-git',
  ])
  await testComp(['npm', 'version', 'major'], [])

  t.end()
})

t.test('failure reading package.json', t => {
  const prefix = t.testdir({})
  npm.prefix = prefix

  version.exec([], err => {
    if (err)
      throw err

    t.deepEqual(
      result,
      [{
        npm: '1.0.0',
        node: '1.0.0',
      }],
      'should not have package name on returning object'
    )

    t.end()
  })
})

t.test('--json option', t => {
  const prefix = t.testdir({})
  npm.flatOptions.json = true
  npm.prefix = prefix
  Object.defineProperty(process, 'versions', { value: {} })

  version.exec([], err => {
    if (err)
      throw err
    t.deepEqual(
      result,
      ['{\n  "npm": "1.0.0"\n}'],
      'should return json stringified result'
    )
    t.end()
  })
})

t.test('with one arg', t => {
  const Version = requireInject('../../lib/version.js', {
    ...mocks,
    libnpmversion: (arg, opts) => {
      t.equal(arg, 'major', 'should forward expected value')
      t.deepEqual(
        opts,
        {
          tagVersionPrefix: 'v',
          json: false,
          path: '',
        },
        'should forward expected options'
      )
      return '4.0.0'
    },
  })
  const version = new Version(npm)

  version.exec(['major'], err => {
    if (err)
      throw err
    t.same(result, ['v4.0.0'], 'outputs the new version prefixed by the tagVersionPrefix')
    t.end()
  })
})
