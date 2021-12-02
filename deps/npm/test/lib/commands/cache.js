const t = require('tap')
const { fake: mockNpm } = require('../../fixtures/mock-npm.js')
const path = require('path')
const npa = require('npm-package-arg')

let outputOutput = []

let rimrafPath = ''
const rimraf = (path, cb) => {
  rimrafPath = path
  return cb()
}

let logOutput = []

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
    /* eslint-disable-next-line max-len */
    [cacacheEntries['make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted'].integrity]
  ].data = Buffer.from('<>>>}"')
  // nuke the version dist
  cacacheContent[
    /* eslint-disable-next-line max-len */
    [cacacheEntries['make-fetch-happen:request-cache:https://registry.npmjs.org/missing-dist'].integrity]
  ].data = Buffer.from(JSON.stringify({ versions: { '23.0.0': {} } }))
  // make the version a non-object
  cacacheContent[
    /* eslint-disable-next-line max-len */
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
  if (!publicURL) {
    url = `${registry}/aabbcc/${contentId}`
  }
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
      || cacacheContent[cacacheEntries[key].integrity] === undefined) {
      throw new Error()
    }
    return cacacheContent[cacacheEntries[key].integrity]
  },
  rm: {
    entry: (path, key) => {
      if (cacacheEntries[key] === undefined) {
        throw new Error()
      }
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

const Cache = t.mock('../../../lib/commands/cache.js', {
  cacache,
  pacote,
  rimraf,
  'proc-log': {
    silly: (...args) => {
      logOutput.push(['silly', ...args])
    },
    warn: (...args) => {
      logOutput.push(['warn', ...args])
    },
  },
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

t.test('cache no args', async t => {
  await t.rejects(
    cache.exec([]),
    { code: 'EUSAGE' },
    'should throw usage instructions'
  )
})

t.test('cache clean', async t => {
  await t.rejects(
    cache.exec(['clean']),
    'the npm cache self-heals',
    'should throw warning'
  )
})

t.test('cache clean (force)', async t => {
  npm.config.set('force', true)
  npm.flatOptions.force = true
  t.teardown(() => {
    rimrafPath = ''
    npm.config.force = false
    npm.flatOptions.force = false
  })

  await cache.exec(['clear'])
  t.equal(rimrafPath, path.join(npm.cache, '_cacache'))
})

t.test('cache add no arg', async t => {
  t.teardown(() => {
    logOutput = []
  })

  await t.rejects(
    cache.exec(['add']),
    {
      code: 'EUSAGE',
      message: 'Usage: First argument to `add` is required',
    },
    'throws usage error'
  )
  t.strictSame(logOutput, [
    ['silly', 'cache add', 'args', []],
  ], 'logs correctly')
})

t.test('cache add pkg only', async t => {
  t.teardown(() => {
    logOutput = []
    tarballStreamSpec = ''
    tarballStreamOpts = {}
  })

  await cache.exec(['add', 'mypkg'])
  t.strictSame(logOutput, [
    ['silly', 'cache add', 'args', ['mypkg']],
    ['silly', 'cache add', 'spec', 'mypkg'],
  ], 'logs correctly')
  t.equal(tarballStreamSpec, 'mypkg', 'passes the correct spec to pacote')
  t.same(tarballStreamOpts, npm.flatOptions, 'passes the correct options to pacote')
})

t.test('cache add multiple pkgs', async t => {
  t.teardown(() => {
    outputOutput = []
    tarballStreamSpec = ''
    tarballStreamOpts = {}
  })

  await cache.exec(['add', 'mypkg', 'anotherpkg'])
  t.strictSame(logOutput, [
    ['silly', 'cache add', 'args', ['mypkg', 'anotherpkg']],
    ['silly', 'cache add', 'spec', 'mypkg'],
    ['silly', 'cache add', 'spec', 'anotherpkg'],
  ], 'logs correctly')
  t.equal(tarballStreamSpec, 'anotherpkg', 'passes the correct spec to pacote')
  t.same(tarballStreamOpts, npm.flatOptions, 'passes the correct options to pacote')
})

t.test('cache ls', async t => {
  t.teardown(() => {
    outputOutput = []
    logOutput = []
  })
  setupCacacheFixture()
  await cache.exec(['ls'])
  t.strictSame(outputOutput, [
    /* eslint-disable-next-line max-len */
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@fritzy/staydown/-/@fritzy/staydown-3.1.1.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@fritzy%2fstaydown',
    /* eslint-disable-next-line max-len */
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@gar/npm-expansion/-/@gar/npm-expansion-2.1.0.tgz',
    /* eslint-disable-next-line max-len */
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@gar/npm-expansion/-/@gar/npm-expansion-3.0.0-beta.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@gar%2fnpm-expansion',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/ape-ecs',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/ape-ecs/-/ape-ecs-2.1.7.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted/-/corrupted-3.1.0.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/foo',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/foo/-/foo-1.2.3-beta.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-dist',
    /* eslint-disable-next-line max-len */
    'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-dist/-/missing-dist-23.0.0.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-version',
    /* eslint-disable-next-line max-len */
    'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-version/-/missing-version-16.2.0.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/npm',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/npm/-/npm-1.2.0.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.44.1.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.47.0.tgz',
    'make-fetch-happen:request-cache:https://somerepo.github.org/aabbcc/14',
    'make-fetch-happen:request-cache:https://somerepo.github.org/extemporaneously',
  ])
})

t.test('cache ls pkgs', async t => {
  t.teardown(() => {
    outputOutput = []
  })
  await cache.exec(['ls', 'webpack@>4.44.1', 'npm'])
  t.strictSame(outputOutput, [
    'make-fetch-happen:request-cache:https://registry.npmjs.org/npm',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/npm/-/npm-1.2.0.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.47.0.tgz',
  ])
})

t.test('cache ls special', async t => {
  t.teardown(() => {
    outputOutput = []
  })
  await cache.exec(['ls', 'foo@1.2.3-beta'])
  t.strictSame(outputOutput, [
    'make-fetch-happen:request-cache:https://registry.npmjs.org/foo',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/foo/-/foo-1.2.3-beta.tgz',
  ])
})

t.test('cache ls nonpublic registry', async t => {
  t.teardown(() => {
    outputOutput = []
  })
  await cache.exec(['ls', 'extemporaneously'])
  t.strictSame(outputOutput, [
    'make-fetch-happen:request-cache:https://somerepo.github.org/aabbcc/14',
    'make-fetch-happen:request-cache:https://somerepo.github.org/extemporaneously',
  ])
})

t.test('cache ls tagged', async t => {
  t.teardown(() => {
    outputOutput = []
  })
  await t.rejects(
    cache.exec(['ls', 'webpack@latest']),
    'tagged package',
    'should throw warning'
  )
})

t.test('cache ls scoped and scoped slash', async t => {
  t.teardown(() => {
    outputOutput = []
  })
  await cache.exec(['ls', '@fritzy/staydown', '@gar/npm-expansion'])
  t.strictSame(outputOutput, [
    /* eslint-disable-next-line max-len */
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@fritzy/staydown/-/@fritzy/staydown-3.1.1.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@fritzy%2fstaydown',
    /* eslint-disable-next-line max-len */
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@gar/npm-expansion/-/@gar/npm-expansion-2.1.0.tgz',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/@gar%2fnpm-expansion',
  ])
})

t.test('cache ls corrupted', async t => {
  t.teardown(() => {
    outputOutput = []
  })
  await cache.exec(['ls', 'corrupted'])
  t.strictSame(outputOutput, [
    'make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/corrupted/-/corrupted-3.1.0.tgz',
  ])
})

t.test('cache ls missing packument dist', async t => {
  t.teardown(() => {
    outputOutput = []
  })
  await cache.exec(['ls', 'missing-dist'])
  t.strictSame(outputOutput, [
    'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-dist',
    /* eslint-disable-next-line max-len */
    'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-dist/-/missing-dist-23.0.0.tgz',
  ])
})

t.test('cache ls missing packument version not an object', async t => {
  t.teardown(() => {
    outputOutput = []
  })
  await cache.exec(['ls', 'missing-version'])
  t.strictSame(outputOutput, [
    'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-version',
    /* eslint-disable-next-line max-len */
    'make-fetch-happen:request-cache:https://registry.npmjs.org/missing-version/-/missing-version-16.2.0.tgz',
  ])
})

t.test('cache rm', async t => {
  t.teardown(() => {
    outputOutput = []
  })
  await cache.exec(['rm',
    'make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.44.1.tgz'])
  t.strictSame(outputOutput, [
    /* eslint-disable-next-line max-len */
    'Deleted: make-fetch-happen:request-cache:https://registry.npmjs.org/webpack/-/webpack-4.44.1.tgz',
  ])
})

t.test('cache rm unfound', async t => {
  t.teardown(() => {
    outputOutput = []
    logOutput = []
  })
  await cache.exec(['rm', 'made-up-key'])
  t.strictSame(logOutput, [
    ['warn', 'Not Found: made-up-key'],
  ], 'logs correctly')
})

t.test('cache verify', async t => {
  t.teardown(() => {
    outputOutput = []
  })

  await cache.exec(['verify'])
  t.match(outputOutput, [
    `Cache verified and compressed (${path.join(npm.cache, '_cacache')})`,
    'Content verified: 1 (100 bytes)',
    'Index entries: 1',
    'Finished in 2s',
  ], 'prints correct output')
})

t.test('cache verify w/ extra output', async t => {
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

  await cache.exec(['check'])
  t.match(outputOutput, [
    `Cache verified and compressed (~${path.join('/fake/path', '_cacache')})`,
    'Content verified: 1 (100 bytes)',
    'Corrupted content removed: 1',
    'Content garbage-collected: 2 (200 bytes)',
    'Missing content: 3',
    'Index entries: 1',
    'Finished in 2s',
  ], 'prints correct output')
})

t.test('cache completion', async t => {
  const { completion } = cache

  const testComp = (argv, expect) => {
    return t.resolveMatch(completion({ conf: { argv: { remain: argv } } }), expect, argv.join(' '))
  }

  await Promise.all([
    testComp(['npm', 'cache'], ['add', 'clean', 'verify']),
    testComp(['npm', 'cache', 'add'], []),
    testComp(['npm', 'cache', 'clean'], []),
    testComp(['npm', 'cache', 'verify'], []),
  ])
})
