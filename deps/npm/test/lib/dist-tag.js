const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm')

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

let npmRegistryFetchMock = (url, opts) => {
  if (url === '/-/package/foo/dist-tags')
    throw new Error('no package found')

  return routeMap[url]
}

npmRegistryFetchMock.json = async (url, opts) => routeMap[url]

const logger = (...msgs) => {
  for (const msg of [...msgs])
    log += msg + ' '

  log += '\n'
}

const DistTag = t.mock('../../lib/dist-tag.js', {
  npmlog: {
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

t.test('ls in current package', (t) => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: '@scoped/pkg',
    }),
  })
  distTag.exec(['ls'], (err) => {
    t.error(err, 'npm dist-tags ls')
    t.matchSnapshot(
      result,
      'should list available tags for current package'
    )
    t.end()
  })
})

t.test('ls global', (t) => {
  t.teardown(() => {
    config.global = false
  })
  config.global = true
  distTag.exec(['ls'], (err) => {
    t.matchSnapshot(
      err,
      'should throw basic usage'
    )
    t.end()
  })
})

t.test('no args in current package', (t) => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      name: '@scoped/pkg',
    }),
  })
  distTag.exec([], (err) => {
    t.error(err, 'npm dist-tags ls')
    t.matchSnapshot(
      result,
      'should default to listing available tags for current package'
    )
    t.end()
  })
})

t.test('borked cmd usage', (t) => {
  npm.prefix = t.testdir({})
  distTag.exec(['borked', '@scoped/pkg'], (err) => {
    t.matchSnapshot(err, 'should show usage error')
    t.end()
  })
})

t.test('ls on named package', (t) => {
  npm.prefix = t.testdir({})
  distTag.exec(['ls', '@scoped/another'], (err) => {
    t.error(err, 'npm dist-tags ls')
    t.matchSnapshot(
      result,
      'should list tags for the specified package'
    )
    t.end()
  })
})

t.test('ls on missing package', (t) => {
  npm.prefix = t.testdir({})
  distTag.exec(['ls', 'foo'], (err) => {
    t.matchSnapshot(
      log,
      'should log no dist-tag found msg'
    )
    t.matchSnapshot(
      err,
      'should throw error message'
    )
    t.end()
  })
})

t.test('ls on missing name in current package', (t) => {
  npm.prefix = t.testdir({
    'package.json': JSON.stringify({
      version: '1.0.0',
    }),
  })
  distTag.exec(['ls'], (err) => {
    t.matchSnapshot(
      err,
      'should throw usage error message'
    )
    t.end()
  })
})

t.test('only named package arg', (t) => {
  npm.prefix = t.testdir({})
  distTag.exec(['@scoped/another'], (err) => {
    t.error(err, 'npm dist-tags ls')
    t.matchSnapshot(
      result,
      'should default to listing tags for the specified package'
    )
    t.end()
  })
})

t.test('workspaces', (t) => {
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

  t.test('no args', t => {
    distTag.execWorkspaces([], [], (err) => {
      t.error(err)
      t.matchSnapshot(result, 'printed the expected output')
      t.end()
    })
  })

  t.test('no args, one workspace', t => {
    distTag.execWorkspaces([], ['workspace-a'], (err) => {
      t.error(err)
      t.matchSnapshot(result, 'printed the expected output')
      t.end()
    })
  })

  t.test('one arg -- .', t => {
    distTag.execWorkspaces(['.'], [], (err) => {
      t.error(err)
      t.matchSnapshot(result, 'printed the expected output')
      t.end()
    })
  })

  t.test('one arg -- .@1, ignores version spec', t => {
    distTag.execWorkspaces(['.@'], [], (err) => {
      t.error(err)
      t.matchSnapshot(result, 'printed the expected output')
      t.end()
    })
  })

  t.test('one arg -- list', t => {
    distTag.execWorkspaces(['list'], [], (err) => {
      t.error(err)
      t.matchSnapshot(result, 'printed the expected output')
      t.end()
    })
  })

  t.test('two args -- list, .', t => {
    distTag.execWorkspaces(['list', '.'], [], (err) => {
      t.error(err)
      t.matchSnapshot(result, 'printed the expected output')
      t.end()
    })
  })

  t.test('two args -- list, .@1, ignores version spec', t => {
    distTag.execWorkspaces(['list', '.@'], [], (err) => {
      t.error(err)
      t.matchSnapshot(result, 'printed the expected output')
      t.end()
    })
  })

  t.test('two args -- list, @scoped/pkg, logs a warning and ignores workspaces', t => {
    distTag.execWorkspaces(['list', '@scoped/pkg'], [], (err) => {
      t.error(err)
      t.match(log, 'Ignoring workspaces for specified package', 'logs a warning')
      t.matchSnapshot(result, 'printed the expected output')
      t.end()
    })
  })

  t.test('no args, one failing workspace sets exitCode to 1', t => {
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

    distTag.execWorkspaces([], [], (err) => {
      t.error(err)
      t.equal(process.exitCode, 1, 'set the error status')
      process.exitCode = 0
      t.match(log, 'dist-tag ls Couldn\'t get dist-tag data for workspace-d@latest', 'logs the error')
      t.matchSnapshot(result, 'printed the expected output')
      t.end()
    })
  })

  t.end()
})

t.test('add new tag', (t) => {
  const _nrf = npmRegistryFetchMock
  t.teardown(() => {
    npmRegistryFetchMock = _nrf
  })

  npmRegistryFetchMock = async (url, opts) => {
    t.equal(opts.method, 'PUT', 'should trigger request to add new tag')
    t.equal(opts.body, '7.7.7', 'should point to expected version')
  }
  npm.prefix = t.testdir({})
  distTag.exec(['add', '@scoped/another@7.7.7', 'c'], (err) => {
    t.error(err, 'npm dist-tags add')
    t.matchSnapshot(
      result,
      'should return success msg'
    )
    t.end()
  })
})

t.test('add using valid semver range as name', (t) => {
  npm.prefix = t.testdir({})
  distTag.exec(['add', '@scoped/another@7.7.7', '1.0.0'], (err) => {
    t.match(
      err,
      /Error: Tag name must not be a valid SemVer range: 1.0.0/,
      'should exit with semver range error'
    )
    t.matchSnapshot(
      log,
      'should return success msg'
    )
    t.end()
  })
})

t.test('add missing args', (t) => {
  npm.prefix = t.testdir({})
  config.tag = ''
  t.teardown(() => {
    delete config.tag
  })
  distTag.exec(['add', '@scoped/another@7.7.7'], (err) => {
    t.matchSnapshot(err, 'should exit usage error message')
    t.end()
  })
})

t.test('add missing pkg name', (t) => {
  npm.prefix = t.testdir({})
  distTag.exec(['add', null], (err) => {
    t.matchSnapshot(err, 'should exit usage error message')
    t.end()
  })
})

t.test('set existing version', (t) => {
  npm.prefix = t.testdir({})
  distTag.exec(['set', '@scoped/another@0.6.0', 'b'], (err) => {
    t.error(err, 'npm dist-tags set')
    t.matchSnapshot(
      log,
      'should log warn msg'
    )
    t.end()
  })
})

t.test('remove existing tag', (t) => {
  const _nrf = npmRegistryFetchMock
  t.teardown(() => {
    npmRegistryFetchMock = _nrf
  })

  npmRegistryFetchMock = async (url, opts) => {
    t.equal(opts.method, 'DELETE', 'should trigger request to remove tag')
  }
  npm.prefix = t.testdir({})
  distTag.exec(['rm', '@scoped/another', 'c'], (err) => {
    t.error(err, 'npm dist-tags rm')
    t.matchSnapshot(log, 'should log remove info')
    t.matchSnapshot(result, 'should return success msg')
    t.end()
  })
})

t.test('remove non-existing tag', (t) => {
  npm.prefix = t.testdir({})
  distTag.exec(['rm', '@scoped/another', 'nonexistent'], (err) => {
    t.match(
      err,
      /Error: nonexistent is not a dist-tag on @scoped\/another/,
      'should exit with error'
    )
    t.matchSnapshot(log, 'should log error msg')
    t.end()
  })
})

t.test('remove missing pkg name', (t) => {
  npm.prefix = t.testdir({})
  distTag.exec(['rm', null], (err) => {
    t.matchSnapshot(err, 'should exit usage error message')
    t.end()
  })
})

t.test('completion', t => {
  const { completion } = distTag
  t.plan(2)

  const match = completion({ conf: { argv: { remain: ['npm', 'dist-tag'] } } })
  t.resolveMatch(match, ['add', 'rm', 'ls'],
    'should list npm dist-tag commands for completion')

  const noMatch = completion({ conf: { argv: { remain: ['npm', 'dist-tag', 'foobar'] } } })
  t.resolveMatch(noMatch, [])
  t.end()
})
