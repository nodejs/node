const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm.js')
const path = require('path')

const usageUtil = () => 'usage instructions'

let outputOutput = []

let rimrafPath = ''
const rimraf = (path, cb) => {
  rimrafPath = path
  return cb()
}

let logOutput = []
const npmlog = {
  silly: (...args) => {
    logOutput.push(['silly', ...args])
  },
}

let tarballStreamSpec = ''
let tarballStreamOpts = {}
const pacote = {
  tarball: {
    stream: (spec, handler, opts) => {
      tarballStreamSpec = spec
      tarballStreamOpts = opts
      return handler({
        resume: () => {},
        promise: () => Promise.resolve(),
      })
    },
  },
}

const cacacheVerifyStats = {
  keptSize: 100,
  verifiedContent: 1,
  totalEntries: 1,
  runTime: { total: 2000 },
}
const cacache = {
  verify: (path) => {
    return cacacheVerifyStats
  },
}

const Cache = t.mock('../../lib/cache.js', {
  cacache,
  npmlog,
  pacote,
  rimraf,
  '../../lib/utils/usage.js': usageUtil,
})

const npm = mockNpm({
  cache: '/fake/path',
  flatOptions: { force: false },
  config: { force: false },
  output: (msg) => {
    outputOutput.push(msg)
  },
})
const cache = new Cache(npm)

t.test('cache no args', t => {
  cache.exec([], err => {
    t.match(err.message, 'usage instructions', 'should throw usage instructions')
    t.end()
  })
})

t.test('cache clean', t => {
  cache.exec(['clean'], err => {
    t.match(err.message, 'the npm cache self-heals', 'should throw warning')
    t.end()
  })
})

t.test('cache clean (force)', t => {
  npm.config.set('force', true)
  npm.flatOptions.force = true
  t.teardown(() => {
    rimrafPath = ''
    npm.config.force = false
    npm.flatOptions.force = false
  })

  cache.exec(['clear'], err => {
    t.error(err)
    t.equal(rimrafPath, path.join(npm.cache, '_cacache'))
    t.end()
  })
})

t.test('cache clean with arg', t => {
  cache.exec(['rm', 'pkg'], err => {
    t.match(err.message, 'does not accept arguments', 'should throw error')
    t.end()
  })
})

t.test('cache add no arg', t => {
  t.teardown(() => {
    logOutput = []
  })

  cache.exec(['add'], err => {
    t.strictSame(logOutput, [
      ['silly', 'cache add', 'args', []],
    ], 'logs correctly')
    t.equal(err.code, 'EUSAGE', 'throws usage error')
    t.end()
  })
})

t.test('cache add pkg only', t => {
  t.teardown(() => {
    logOutput = []
    tarballStreamSpec = ''
    tarballStreamOpts = {}
  })

  cache.exec(['add', 'mypkg'], err => {
    t.error(err)
    t.strictSame(logOutput, [
      ['silly', 'cache add', 'args', ['mypkg']],
      ['silly', 'cache add', 'spec', 'mypkg'],
    ], 'logs correctly')
    t.equal(tarballStreamSpec, 'mypkg', 'passes the correct spec to pacote')
    t.same(tarballStreamOpts, npm.flatOptions, 'passes the correct options to pacote')
    t.end()
  })
})

t.test('cache add multiple pkgs', t => {
  t.teardown(() => {
    logOutput = []
    tarballStreamSpec = ''
    tarballStreamOpts = {}
  })

  cache.exec(['add', 'mypkg', 'anotherpkg'], err => {
    t.error(err)
    t.strictSame(logOutput, [
      ['silly', 'cache add', 'args', ['mypkg', 'anotherpkg']],
      ['silly', 'cache add', 'spec', 'mypkg'],
      ['silly', 'cache add', 'spec', 'anotherpkg'],
    ], 'logs correctly')
    t.equal(tarballStreamSpec, 'anotherpkg', 'passes the correct spec to pacote')
    t.same(tarballStreamOpts, npm.flatOptions, 'passes the correct options to pacote')
    t.end()
  })
})

t.test('cache verify', t => {
  t.teardown(() => {
    outputOutput = []
  })

  cache.exec(['verify'], err => {
    t.error(err)
    t.match(outputOutput, [
      `Cache verified and compressed (${path.join(npm.cache, '_cacache')})`,
      'Content verified: 1 (100 bytes)',
      'Index entries: 1',
      'Finished in 2s',
    ], 'prints correct output')
    t.end()
  })
})

t.test('cache verify w/ extra output', t => {
  npm.cache = `${process.env.HOME}/fake/path`
  cacacheVerifyStats.badContentCount = 1
  cacacheVerifyStats.reclaimedCount = 2
  cacacheVerifyStats.reclaimedSize = 200
  cacacheVerifyStats.missingContent = 3
  t.teardown(() => {
    npm.cache = '/fake/path'
    outputOutput = []
    delete cacacheVerifyStats.badContentCount
    delete cacacheVerifyStats.reclaimedCount
    delete cacacheVerifyStats.reclaimedSize
    delete cacacheVerifyStats.missingContent
  })

  cache.exec(['check'], err => {
    t.error(err)
    t.match(outputOutput, [
      `Cache verified and compressed (~${path.join('/fake/path', '_cacache')})`,
      'Content verified: 1 (100 bytes)',
      'Corrupted content removed: 1',
      'Content garbage-collected: 2 (200 bytes)',
      'Missing content: 3',
      'Index entries: 1',
      'Finished in 2s',
    ], 'prints correct output')
    t.end()
  })
})

t.test('cache completion', t => {
  const { completion } = cache

  const testComp = (argv, expect) => {
    t.resolveMatch(completion({ conf: { argv: { remain: argv } } }), expect, argv.join(' '))
  }

  testComp(['npm', 'cache'], ['add', 'clean', 'verify'])
  testComp(['npm', 'cache', 'add'], [])
  testComp(['npm', 'cache', 'clean'], [])
  testComp(['npm', 'cache', 'verify'], [])

  t.end()
})
