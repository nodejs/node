const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm.js')
const path = require('path')
const npa = require('npm-package-arg')

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

let cacacheEntries = {}
let cacacheContent = {}

const setupCacacheFixture = () => {
  cacacheEntries = {}
  cacacheContent = {}
  const pkgs = [
    ['webpack@4.44.1', 'https://registry.npmjs.org', true],
    ['npm@1.2.0', 'https://registry.npmjs.org', true],
    ['webpack@4.47.0', 'https://registry.npmjs.org', true],
    ['foo@1.2.3-beta', 'https://registry.npmjs.org', true],
    ['ape-ecs@2.1.7', 'https://registry.npmjs.org', true],
    ['@fritzy/staydown@3.1.1', 'https://registry.npmjs.org', true],
    ['@gar/npm-expansion@2.1.0', 'https://registry.npmjs.org', true],
    ['@gar/npm-expansion@3.0.0-beta', 'https://registry.npmjs.org', true],
    ['extemporaneously@44.2.2', 'https://somerepo.github.org', false],
    ['corrupted@3.1.0', 'https://registry.npmjs.org', true],
    ['missing-dist@23.0.0', 'https://registry.npmjs.org', true],
    ['missing-version@16.2.0', 'https://registry.npmjs.org', true],
  ]
  pkgs.forEach(pkg => addCacachePkg(...pkg))
  // corrupt the packument
  cacacheContent[
    [cacacheEntries['make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted'].integrity]
  ].data = Buffer.from('<>>>}"')
  // nuke the version dist
  cacacheContent[
    [cacacheEntries['make-fetch-happen:request-cache:https://registry.npmjs.org/missing-dist'].integrity]
  ].data = Buffer.from(JSON.stringify({ versions: { '23.0.0': {} } }))
  // make the version a non-object
  cacacheContent[
    [cacacheEntries['make-fetch-happen:request-cache:https://registry.npmjs.org/missing-version'].integrity]
  ].data = Buffer.from(JSON.stringify({ versions: 'hello' }))
}

const packuments = {}

let contentId = 0
const cacacheVerifyStats = {
  keptSize: 100,
  verifiedContent: 1,
  totalEntries: 1,
  runTime: { total: 2000 },
}

const addCacacheKey = (key, content) => {
  contentId++
  cacacheEntries[key] = { integrity: `${contentId}` }
  cacacheContent[`${contentId}`] = {}
}
const addCacachePkg = (spec, registry, publicURL) => {
  const parts = npa(spec)
  const ver = parts.rawSpec || '1.0.0'
  let url = `${registry}/${parts.name}/-/${parts.name}-${ver}.tgz`
  if (!publicURL)
    url = `${registry}/aabbcc/${contentId}`
  const key = `make-fetch-happen:request-cache:${url}`
  const pkey = `make-fetch-happen:request-cache:${registry}/${parts.escapedName}`
  if (!packuments[parts.escapedName]) {
    packuments[parts.escapedName] = {
      versions: {},
    }
    addCacacheKey(pkey)
  }
  packuments[parts.escapedName].versions[ver] = {
    dist: {
      tarball: url,
    },
  }
  addCacacheKey(key)
  cacacheContent[cacacheEntries[pkey].integrity] = {
    data: Buffer.from(JSON.stringify(packuments[parts.escapedName])),
  }
}

const cacache = {
  verify: (path) => {
    return cacacheVerifyStats
  },
  get: (path, key) => {
    if (cacacheEntries[key] === undefined
      || cacacheContent[cacacheEntries[key].integrity] === undefined)
      throw new Error()
    return cacacheContent[cacacheEntries[key].integrity]
  },
  rm: {
    entry: (path, key) => {
      if (cacacheEntries[key] === undefined)
        throw new Error()
      delete cacacheEntries[key]
    },
    content: (path, sha) => {
      delete cacacheContent[sha]
    },
  },
  ls: (path) => {
    return cacacheEntries
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
  log: {
    warn: (...args) => {
      logOutput.push(['warn', ...args])
    },
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
    outputOutput = []
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

t.test('cache ls', t => {
  t.teardown(() => {
    outputOutput = []
    logOutput = []
  })
  setupCacacheFixture()
  cache.exec(['ls'], err => {
    t.error(err)
    t.strictSame(outputOutput, [
      'make-fetch-happen:request-cache:https://registry.npmjs.org/@fritzy/staydown/-/@fritzy/staydown-3.1.1.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/@fritzy%2fstaydown',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/@gar/npm-expansion/-/@gar/npm-expansion-2.1.0.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/@gar/npm-expansion/-/@gar/npm-expansion-3.0.0-beta.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/@gar%2fnpm-expansion',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/ape-ecs',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/ape-ecs/-/ape-ecs-2.1.7.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted/-/corrupted-3.1.0.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/foo',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/foo/-/foo-1.2.3-beta.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-dist',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-dist/-/missing-dist-23.0.0.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-version',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-version/-/missing-version-16.2.0.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/npm',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/npm/-/npm-1.2.0.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.44.1.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.47.0.tgz',
      'make-fetch-happen:request-cache:https://somerepo.github.org/aabbcc/14',
      'make-fetch-happen:request-cache:https://somerepo.github.org/extemporaneously',
    ])
    t.end()
  })
})

t.test('cache ls pkgs', t => {
  t.teardown(() => {
    outputOutput = []
  })
  cache.exec(['ls', 'webpack@>4.44.1', 'npm'], err => {
    t.error(err)
    t.strictSame(outputOutput, [
      'make-fetch-happen:request-cache:https://registry.npmjs.org/npm',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/npm/-/npm-1.2.0.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.47.0.tgz',
    ])
    t.end()
  })
})

t.test('cache ls special', t => {
  t.teardown(() => {
    outputOutput = []
  })
  cache.exec(['ls', 'foo@1.2.3-beta'], err => {
    t.error(err)
    t.strictSame(outputOutput, [
      'make-fetch-happen:request-cache:https://registry.npmjs.org/foo',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/foo/-/foo-1.2.3-beta.tgz',
    ])
    t.end()
  })
})

t.test('cache ls nonpublic registry', t => {
  t.teardown(() => {
    outputOutput = []
  })
  cache.exec(['ls', 'extemporaneously'], err => {
    t.error(err)
    t.strictSame(outputOutput, [
      'make-fetch-happen:request-cache:https://somerepo.github.org/aabbcc/14',
      'make-fetch-happen:request-cache:https://somerepo.github.org/extemporaneously',
    ])
    t.end()
  })
})

t.test('cache ls tagged', t => {
  t.teardown(() => {
    outputOutput = []
  })
  cache.exec(['ls', 'webpack@latest'], err => {
    t.match(err.message, 'tagged package', 'should throw warning')
    t.end()
  })
})

t.test('cache ls scoped and scoped slash', t => {
  t.teardown(() => {
    outputOutput = []
  })
  cache.exec(['ls', '@fritzy/staydown', '@gar/npm-expansion'], err => {
    t.error(err)
    t.strictSame(outputOutput, [
      'make-fetch-happen:request-cache:https://registry.npmjs.org/@fritzy/staydown/-/@fritzy/staydown-3.1.1.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/@fritzy%2fstaydown',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/@gar/npm-expansion/-/@gar/npm-expansion-2.1.0.tgz',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/@gar%2fnpm-expansion',
    ])
    t.end()
  })
})

t.test('cache ls corrupted', t => {
  t.teardown(() => {
    outputOutput = []
  })
  cache.exec(['ls', 'corrupted'], err => {
    t.error(err)
    t.strictSame(outputOutput, [
      'make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted/-/corrupted-3.1.0.tgz',
    ])
    t.end()
  })
})

t.test('cache ls missing packument dist', t => {
  t.teardown(() => {
    outputOutput = []
  })
  cache.exec(['ls', 'missing-dist'], err => {
    t.error(err)
    t.strictSame(outputOutput, [
      'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-dist',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-dist/-/missing-dist-23.0.0.tgz',
    ])
    t.end()
  })
})

t.test('cache ls missing packument version not an object', t => {
  t.teardown(() => {
    outputOutput = []
  })
  cache.exec(['ls', 'missing-version'], err => {
    t.error(err)
    t.strictSame(outputOutput, [
      'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-version',
      'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-version/-/missing-version-16.2.0.tgz',
    ])
    t.end()
  })
})

t.test('cache rm', t => {
  t.teardown(() => {
    outputOutput = []
  })
  cache.exec(['rm',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.44.1.tgz'], err => {
    t.error(err)
    t.strictSame(outputOutput, [
      'Deleted: make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.44.1.tgz',
    ])
    t.end()
  })
})

t.test('cache rm unfound', t => {
  t.teardown(() => {
    outputOutput = []
    logOutput = []
  })
  cache.exec(['rm', 'made-up-key'], err => {
    t.error(err)
    t.strictSame(logOutput, [
      ['warn', 'Not Found: made-up-key'],
    ], 'logs correctly')
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
