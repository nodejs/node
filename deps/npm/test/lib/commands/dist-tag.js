const t = require('tap')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

let result = ''
let log = ''

t.afterEach(() => {
  result = ''
  log = ''
})

const routeMap = {
  '/-/package/@scoped%2fpkg/dist-tags': {
    latest: '1.0.0',
    a: '0.0.1',
    b: '0.5.0',
  },
  '/-/package/@scoped%2fanother/dist-tags': {
    latest: '2.0.0',
    a: '0.0.2',
    b: '0.6.0',
  },
  '/-/package/@scoped%2fanother/dist-tags/c': {
    latest: '7.7.7',
    a: '0.0.2',
    b: '0.6.0',
    c: '7.7.7',
  },
  '/-/package/workspace-a/dist-tags': {
    latest: '1.0.0',
    'latest-a': '1.0.0',
  },
  '/-/package/workspace-b/dist-tags': {
    latest: '2.0.0',
    'latest-b': '2.0.0',
  },
  '/-/package/workspace-c/dist-tags': {
    latest: '3.0.0',
    'latest-c': '3.0.0',
  },
}

// XXX overriding this does not appear to do anything, adding t.plan to things
// that use it fails the test
let npmRegistryFetchMock = (url, opts) => {
  if (url === '/-/package/foo/dist-tags') {
    throw new Error('no package found')
  }

  return routeMap[url]
}

npmRegistryFetchMock.json = async (url, opts) => {
  return routeMap[url]
}

const logger = (...msgs) => {
  for (const msg of [...msgs]) {
    log += msg + ' '
  }

  log += '\n'
}

const DistTag = t.mock('../../../lib/commands/dist-tag.js', {
  'proc-log': {
    error: logger,
    info: logger,
    verbose: logger,
    warn: logger,
  },
  get 'npm-registry-fetch' () {
    return npmRegistryFetchMock
  },
})

const config = {}
const npm = mockNpm({
  config,
  output: msg => {
    result = result ? [result, msg].join('\n') : msg
  },
})
const distTag = new DistTag(npm)

t.test('ls in current package', async t => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: '@scoped/pkg',
    }),
  })
  await distTag.exec(['ls'])
  t.matchSnapshot(
    result,
    'should list available tags for current package'
  )
})

t.test('ls global', async t => {
  t.teardown(() => {
    config.global = false
  })
  config.global = true
  await t.rejects(
    distTag.exec(['ls']),
    distTag.usage,
    'should throw basic usage'
  )
})

t.test('no args in current package', async t => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: '@scoped/pkg',
    }),
  })
  await distTag.exec([])
  t.matchSnapshot(
    result,
    'should default to listing available tags for current package'
  )
})

t.test('borked cmd usage', async t => {
  npm.prefix = t.testdir({})
  await t.rejects(
    distTag.exec(['borked', '@scoped/pkg']),
    distTag.usage,
    'should show usage error'
  )
})

t.test('ls on named package', async t => {
  npm.prefix = t.testdir({})
  await distTag.exec(['ls', '@scoped/another'])
  t.matchSnapshot(
    result,
    'should list tags for the specified package'
  )
})

t.test('ls on missing package', async t => {
  npm.prefix = t.testdir({})
  await t.rejects(
    distTag.exec(['ls', 'foo']),
    distTag.usage
  )
  t.matchSnapshot(
    log,
    'should log no dist-tag found msg'
  )
})

t.test('ls on missing name in current package', async t => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      version: '1.0.0',
    }),
  })
  await t.rejects(
    distTag.exec(['ls']),
    distTag.usage,
    'should throw usage error message'
  )
})

t.test('only named package arg', async t => {
  npm.prefix = t.testdir({})
  await distTag.exec(['@scoped/another'])
  t.matchSnapshot(
    result,
    'should default to listing tags for the specified package'
  )
})

t.test('workspaces', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'root',
      version: '1.0.0',
      workspaces: ['workspace-a', 'workspace-b', 'workspace-c'],
    }),
    'workspace-a': {
      'package.json': JSON.stringify({
        name: 'workspace-a',
        version: '1.0.0',
      }),
    },
    'workspace-b': {
      'package.json': JSON.stringify({
        name: 'workspace-b',
        version: '1.0.0',
      }),
    },
    'workspace-c': {
      'package.json': JSON.stringify({
        name: 'workspace-c',
        version: '1.0.0',
      }),
    },
  })

  t.test('no args', async t => {
    await distTag.execWorkspaces([], [])
    t.matchSnapshot(result, 'printed the expected output')
  })

  t.test('no args, one workspace', async t => {
    await distTag.execWorkspaces([], ['workspace-a'])
    t.matchSnapshot(result, 'printed the expected output')
  })

  t.test('one arg -- .', async t => {
    await distTag.execWorkspaces(['.'], [])
    t.matchSnapshot(result, 'printed the expected output')
  })

  t.test('one arg -- .@1, ignores version spec', async t => {
    await distTag.execWorkspaces(['.@'], [])
    t.matchSnapshot(result, 'printed the expected output')
  })

  t.test('one arg -- list', async t => {
    await distTag.execWorkspaces(['list'], [])
    t.matchSnapshot(result, 'printed the expected output')
  })

  t.test('two args -- list, .', async t => {
    await distTag.execWorkspaces(['list', '.'], [])
    t.matchSnapshot(result, 'printed the expected output')
  })

  t.test('two args -- list, .@1, ignores version spec', async t => {
    await distTag.execWorkspaces(['list', '.@'], [])
    t.matchSnapshot(result, 'printed the expected output')
  })

  t.test('two args -- list, @scoped/pkg, logs a warning and ignores workspaces', async t => {
    await distTag.execWorkspaces(['list', '@scoped/pkg'], [])
    t.match(log, 'Ignoring workspaces for specified package', 'logs a warning')
    t.matchSnapshot(result, 'printed the expected output')
  })

  t.test('no args, one failing workspace sets exitCode to 1', async t => {
    npm.localPrefix = t.testdir({
      'package.json': JSON.stringify({
        name: 'root',
        version: '1.0.0',
        workspaces: ['workspace-a', 'workspace-b', 'workspace-c', 'workspace-d'],
      }),
      'workspace-a': {
        'package.json': JSON.stringify({
          name: 'workspace-a',
          version: '1.0.0',
        }),
      },
      'workspace-b': {
        'package.json': JSON.stringify({
          name: 'workspace-b',
          version: '1.0.0',
        }),
      },
      'workspace-c': {
        'package.json': JSON.stringify({
          name: 'workspace-c',
          version: '1.0.0',
        }),
      },
      'workspace-d': {
        'package.json': JSON.stringify({
          name: 'workspace-d',
          version: '1.0.0',
        }),
      },
    })

    await distTag.execWorkspaces([], [])
    t.equal(process.exitCode, 1, 'set the error status')
    process.exitCode = 0
    t.match(log, 'dist-tag ls Couldn\'t get dist-tag data for workspace-d@latest', 'logs the error')
    t.matchSnapshot(result, 'printed the expected output')
  })

  t.end()
})

t.test('add new tag', async t => {
  const _nrf = npmRegistryFetchMock
  t.teardown(() => {
    npmRegistryFetchMock = _nrf
  })

  npmRegistryFetchMock = async (url, opts) => {
    t.equal(opts.method, 'PUT', 'should trigger request to add new tag')
    t.equal(opts.body, '7.7.7', 'should point to expected version')
  }
  npm.prefix = t.testdir({})
  await distTag.exec(['add', '@scoped/another@7.7.7', 'c'])
  t.matchSnapshot(
    result,
    'should return success msg'
  )
})

t.test('add using valid semver range as name', async t => {
  npm.prefix = t.testdir({})
  await t.rejects(
    distTag.exec(['add', '@scoped/another@7.7.7', '1.0.0']),
    /Tag name must not be a valid SemVer range: 1.0.0/,
    'should exit with semver range error'
  )
  t.matchSnapshot(
    log,
    'should return success msg'
  )
})

t.test('add missing args', async t => {
  npm.prefix = t.testdir({})
  config.tag = ''
  t.teardown(() => {
    delete config.tag
  })
  await t.rejects(
    distTag.exec(['add', '@scoped/another@7.7.7']),
    distTag.usage,
    'should exit usage error message'
  )
})

t.test('add missing pkg name', async t => {
  npm.prefix = t.testdir({})
  await t.rejects(
    distTag.exec(['add', null]),
    distTag.usage,
    'should exit usage error message'
  )
})

t.test('set existing version', async t => {
  npm.prefix = t.testdir({})
  await distTag.exec(['set', '@scoped/another@0.6.0', 'b'])
  t.matchSnapshot(
    log,
    'should log warn msg'
  )
})

t.test('remove existing tag', async t => {
  const _nrf = npmRegistryFetchMock
  t.teardown(() => {
    npmRegistryFetchMock = _nrf
  })

  npmRegistryFetchMock = async (url, opts) => {
    t.equal(opts.method, 'DELETE', 'should trigger request to remove tag')
  }
  npm.prefix = t.testdir({})
  await distTag.exec(['rm', '@scoped/another', 'c'])
  t.matchSnapshot(log, 'should log remove info')
  t.matchSnapshot(result, 'should return success msg')
})

t.test('remove non-existing tag', async t => {
  npm.prefix = t.testdir({})
  await t.rejects(
    distTag.exec(['rm', '@scoped/another', 'nonexistent']),
    /nonexistent is not a dist-tag on @scoped\/another/,
    'should exit with error'
  )
  t.matchSnapshot(log, 'should log error msg')
})

t.test('remove missing pkg name', async t => {
  npm.prefix = t.testdir({})
  await t.rejects(
    distTag.exec(['rm', null]),
    distTag.usage,
    'should exit usage error message'
  )
})

t.test('completion', async t => {
  const { completion } = distTag
  t.plan(2)

  const match = completion({ conf: { argv: { remain: ['npm', 'dist-tag'] } } })
  t.resolveMatch(match, ['add', 'rm', 'ls'],
    'should list npm dist-tag commands for completion')

  const noMatch = completion({ conf: { argv: { remain: ['npm', 'dist-tag', 'foobar'] } } })
  t.resolveMatch(noMatch, [])
  t.end()
})
