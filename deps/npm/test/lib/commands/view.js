const t = require('tap')
const { load: _loadMockNpm } = require('../../fixtures/mock-npm.js')

t.cleanSnapshot = str => str
  .replace(/(published ).*?( ago)/g, '$1{TIME}$2')

// run the same as tap does when running directly with node
process.stdout.columns = undefined

// 3 days. its never yesterday and never a week ago
const yesterday = new Date(Date.now() - 1000 * 60 * 60 * 24 * 3)

const packument = (nv, opts) => {
  if (!opts.fullMetadata) {
    throw new Error('must fetch fullMetadata')
  }

  if (!opts.preferOnline) {
    throw new Error('must fetch with preferOnline')
  }

  const mocks = {
    red: {
      _id: 'red@1.0.1',
      name: 'red',
      'dist-tags': {
        '1.0.1': {},
      },
      time: {
        unpublished: {
          time: '2012-12-20T00:00:00.000Z',
        },
      },
    },
    blue: {
      name: 'blue',
      'dist-tags': {
        latest: '1.0.0',
      },
      time: {
        '1.0.0': yesterday,
      },
      versions: {
        '1.0.0': {
          name: 'blue',
          version: '1.0.0',
          dist: {
            shasum: '123',
            tarball: 'http://hm.blue.com/1.0.0.tgz',
            integrity: '---',
            fileCount: 1,
            unpackedSize: 1,
          },
        },
        '1.0.1': {
          name: 'blue',
          version: '1.0.1',
          dist: {
            shasum: '124',
            tarball: 'http://hm.blue.com/1.0.1.tgz',
            integrity: '---',
            fileCount: 1,
            unpackedSize: 1000,
          },
        },
      },
    },
    cyan: {
      _npmUser: {
        name: 'claudia',
        email: 'claudia@cyan.com',
      },
      name: 'cyan',
      'dist-tags': {
        latest: '1.0.0',
      },
      versions: {
        '1.0.0': {
          version: '1.0.0',
          name: 'cyan',
          dist: {
            shasum: '123',
            tarball: 'http://hm.cyan.com/1.0.0.tgz',
            integrity: '---',
            fileCount: 1,
            unpackedSize: 1000000,
          },
        },
        '1.0.1': {},
      },
    },
    brown: {
      name: 'brown',
    },
    yellow: {
      _id: 'yellow',
      name: 'yellow',
      author: {
        name: 'foo',
        email: 'foo@yellow.com',
        twitter: 'foo',
      },
      readme: 'a very useful readme',
      versions: {
        '1.0.0': {
          version: '1.0.0',
          author: 'claudia',
          readme: 'a very useful readme',
          maintainers: [
            { name: 'claudia', email: 'c@yellow.com', twitter: 'cyellow' },
            { name: 'isaacs', email: 'i@yellow.com', twitter: 'iyellow' },
          ],
        },
        '1.0.1': {
          version: '1.0.1',
          author: 'claudia',
        },
        '1.0.2': {
          version: '1.0.2',
          author: 'claudia',
        },
      },
    },
    purple: {
      name: 'purple',
      versions: {
        '1.0.0': {
          foo: 1,
          maintainers: [
            { name: 'claudia' },
          ],
        },
        '1.0.1': {},
      },
    },
    green: {
      _id: 'green',
      name: 'green',
      'dist-tags': {
        latest: '1.0.0',
      },
      maintainers: [
        { name: 'claudia', email: 'c@yellow.com', twitter: 'cyellow' },
        { name: 'isaacs', email: 'i@yellow.com', twitter: 'iyellow' },
      ],
      keywords: ['colors', 'green', 'crayola'],
      versions: {
        '1.0.0': {
          _id: 'green',
          version: '1.0.0',
          description: 'green is a very important color',
          bugs: {
            url: 'http://bugs.green.com',
          },
          deprecated: true,
          repository: {
            url: 'http://repository.green.com',
          },
          license: { type: 'ACME' },
          bin: {
            green: 'bin/green.js',
          },
          dependencies: {
            red: '1.0.0',
            yellow: '1.0.0',
          },
          dist: {
            shasum: '123',
            tarball: 'http://hm.green.com/1.0.0.tgz',
            integrity: '---',
            fileCount: 1,
            unpackedSize: 1000000000,
          },
        },
        '1.0.1': {},
      },
    },
    black: {
      name: 'black',
      'dist-tags': {
        latest: '1.0.0',
      },
      versions: {
        '1.0.0': {
          version: '1.0.0',
          bugs: 'http://bugs.black.com',
          license: {},
          dependencies: (() => {
            const deps = {}
            for (let i = 0; i < 25; i++) {
              deps[i] = '1.0.0'
            }

            return deps
          })(),
          dist: {
            shasum: '123',
            tarball: 'http://hm.black.com/1.0.0.tgz',
            integrity: '---',
            fileCount: 1,
            unpackedSize: 1,
          },
        },
        '1.0.1': {},
      },
    },
    pink: {
      name: 'pink',
      'dist-tags': {
        latest: '1.0.0',
      },
      versions: {
        '1.0.0': {
          version: '1.0.0',
          maintainers: [
            { name: 'claudia', url: 'http://c.pink.com' },
            { name: 'isaacs', url: 'http://i.pink.com' },
          ],
          repository: 'http://repository.pink.com',
          license: {},
          dist: {
            shasum: '123',
            tarball: 'http://hm.pink.com/1.0.0.tgz',
            integrity: '---',
            fileCount: 1,
            unpackedSize: 1,
          },
        },
        '1.0.1': {},
      },
    },
    orange: {
      name: 'orange',
      'dist-tags': {
        latest: '1.0.0',
      },
      versions: {
        '1.0.0': {
          version: '1.0.0',
          homepage: 'http://hm.orange.com',
          license: {},
          dist: {
            shasum: '123',
            tarball: 'http://hm.orange.com/1.0.0.tgz',
            integrity: '---',
            fileCount: 1,
            unpackedSize: 1,
          },
        },
        '1.0.1': {},
      },
    },
  }
  if (nv.type === 'git') {
    return mocks[nv.hosted.project]
  }
  if (nv.raw === './blue') {
    return mocks.blue
  }
  return mocks[nv.name]
}

const loadMockNpm = async function (t, opts = {}) {
  const consoleLogs = []
  const mockNpm = await _loadMockNpm(t, {
    mocks: {
      pacote: {
        packument,
      },
    },
    globals: {
      'console.log': (...args) => {
        consoleLogs.push(args)
      },
    },
    ...opts,
  })
  return { ...mockNpm, consoleLogs }
}

t.test('package from git', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t, { config: { unicode: false } })
  await npm.exec('view', ['https://github.com/npm/green'])
  t.matchSnapshot(consoleLogs.join('\n'))
})

t.test('deprecated package with license, bugs, repository and other fields', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t, { config: { unicode: false } })
  await npm.exec('view', ['green@1.0.0'])
  t.matchSnapshot(consoleLogs.join('\n'))
})

t.test('deprecated package with unicode', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t, { config: { unicode: true } })
  await npm.exec('view', ['green@1.0.0'])
  t.matchSnapshot(consoleLogs.join('\n'))
})

t.test('package with more than 25 deps', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t, { config: { unicode: false } })
  await npm.exec('view', ['black@1.0.0'])
  t.matchSnapshot(consoleLogs.join('\n'))
})

t.test('package with maintainers info as object', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t, { config: { unicode: false } })
  await npm.exec('view', ['pink@1.0.0'])
  t.matchSnapshot(consoleLogs.join('\n'))
})

t.test('package with homepage', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t, { config: { unicode: false } })
  await npm.exec('view', ['orange@1.0.0'])
  t.matchSnapshot(consoleLogs.join('\n'))
})

t.test('package with no versions', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t, { config: { unicode: false } })
  await npm.exec('view', ['brown'])
  t.equal(consoleLogs.join('\n'), '', 'no info to display')
})

t.test('package with no repo or homepage', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t, { config: { unicode: false } })
  await npm.exec('view', ['blue@1.0.0'])
  t.matchSnapshot(consoleLogs.join('\n'))
})

t.test('package with semver range', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t, { config: { unicode: false } })
  await npm.exec('view', ['blue@^1.0.0'])
  t.matchSnapshot(consoleLogs.join('\n'))
})

t.test('package with no modified time', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t, { config: { unicode: false } })
  await npm.exec('view', ['cyan@1.0.0'])
  t.matchSnapshot(consoleLogs.join('\n'))
})

t.test('package with --json and semver range', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t, { config: { json: true } })
  await npm.exec('view', ['cyan@^1.0.0'])
  t.matchSnapshot(consoleLogs.join('\n'))
})

t.test('package with --json and no versions', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t, { config: { json: true } })
  await npm.exec('view', ['brown'])
  t.equal(consoleLogs.join('\n'), '', 'no info to display')
})

t.test('package in cwd', async t => {
  const prefixDir = {
    'package.json': JSON.stringify({
      name: 'blue',
      version: '1.0.0',
    }, null, 2),
  }

  t.test('specific version', async t => {
    const { npm, consoleLogs } = await loadMockNpm(t, { prefixDir })
    await npm.exec('view', ['.@1.0.0'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('non-specific version', async t => {
    const { npm, consoleLogs } = await loadMockNpm(t, { prefixDir })
    await npm.exec('view', ['.'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('directory', async t => {
    const { npm, consoleLogs } = await loadMockNpm(t, { prefixDir })
    await npm.exec('view', ['./blue'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })
})

t.test('specific field names', async t => {
  const { npm, consoleLogs } = await loadMockNpm(t)
  t.afterEach(() => {
    consoleLogs.length = 0
  })
  t.test('readme', async t => {
    await npm.exec('view', ['yellow@1.0.0', 'readme'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('several fields', async t => {
    await npm.exec('view', ['yellow@1.0.0', 'name', 'version', 'foo[bar]'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('several fields with several versions', async t => {
    await npm.exec('view', ['yellow@1.x.x', 'author'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('nested field with brackets', async t => {
    await npm.exec('view', ['orange@1.0.0', 'dist[shasum]'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('maintainers with email', async t => {
    await npm.exec('view', ['yellow@1.0.0', 'maintainers', 'name'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('maintainers with url', async t => {
    await npm.exec('view', ['pink@1.0.0', 'maintainers'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('unknown nested field ', async t => {
    await npm.exec('view', ['yellow@1.0.0', 'dist.foobar'])
    t.equal(consoleLogs.join('\n'), '', 'no info to display')
  })

  t.test('array field - 1 element', async t => {
    await npm.exec('view', ['purple@1.0.0', 'maintainers.name'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('array field - 2 elements', async t => {
    await npm.exec('view', ['yellow@1.x.x', 'maintainers.name'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })
})

t.test('throw error if global mode', async t => {
  const { npm } = await loadMockNpm(t, { config: { global: true } })
  await t.rejects(
    npm.exec('view', []),
    /Cannot use view command in global mode./
  )
})

t.test('throw ENOENT error if package.json missing', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('view', []),
    { code: 'ENOENT' }
  )
})

t.test('throw error if package.json has no name', async t => {
  const { npm } = await loadMockNpm(t, {
    prefixDir: {
      'package.json': '{}',
    },
  })
  await t.rejects(
    npm.exec('view', []),
    /Invalid package.json, no "name" field/
  )
})

t.test('throws when unpublished', async t => {
  const { npm } = await loadMockNpm(t)
  await t.rejects(
    npm.exec('view', ['red']),
    { code: 'E404', pkgid: 'red@1.0.1', message: 'Unpublished on 2012-12-20T00:00:00.000Z' }
  )
})

t.test('workspaces', async t => {
  const prefixDir = {
    'package.json': JSON.stringify({
      name: 'workspaces-test-package',
      version: '1.2.3',
      workspaces: ['test-workspace-a', 'test-workspace-b'],
    }),
    'test-workspace-a': {
      'package.json': JSON.stringify({
        name: 'green',
        version: '1.2.3',
      }),
    },
    'test-workspace-b': {
      'package.json': JSON.stringify({
        name: 'orange',
        version: '1.2.3',
      }),
    },
  }

  t.test('all workspaces', async t => {
    const { npm, consoleLogs } = await loadMockNpm(t, {
      prefixDir,
      config: { unicode: false, workspaces: true },
    })
    await npm.exec('view', [])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('one specific workspace', async t => {
    const { npm, consoleLogs } = await loadMockNpm(t, {
      prefixDir,
      config: { unicode: false, workspace: ['green'] },
    })
    await npm.exec('view', [])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('all workspaces --json', async t => {
    const { npm, consoleLogs } = await loadMockNpm(t, {
      prefixDir,
      config: { unicode: false, workspaces: true, json: true },
    })
    await npm.exec('view', [])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('all workspaces single field', async t => {
    const { npm, consoleLogs } = await loadMockNpm(t, {
      prefixDir,
      config: { unicode: false, workspaces: true },
    })
    await npm.exec('view', ['.', 'name'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('all workspaces nonexistent field', async t => {
    const { npm, consoleLogs } = await loadMockNpm(t, {
      prefixDir,
      config: { unicode: false, workspaces: true },
    })
    await npm.exec('view', ['.', 'foo'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('all workspaces nonexistent field --json', async t => {
    const { npm, consoleLogs } = await loadMockNpm(t, {
      prefixDir,
      config: { unicode: false, workspaces: true, json: true },
    })
    await npm.exec('view', ['.', 'foo'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('all workspaces single field --json', async t => {
    const { npm, consoleLogs } = await loadMockNpm(t, {
      prefixDir,
      config: { unicode: false, workspaces: true, json: true },
    })
    await npm.exec('view', ['.', 'name'])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('single workspace --json', async t => {
    const { npm, consoleLogs } = await loadMockNpm(t, {
      prefixDir,
      config: { unicode: false, workspace: ['green'], json: true },
    })
    await npm.exec('view', [])
    t.matchSnapshot(consoleLogs.join('\n'))
  })

  t.test('remote package name', async t => {
    const { npm, logs, consoleLogs } = await loadMockNpm(t, {
      prefixDir,
      config: { unicode: false, workspaces: true },
    })
    await npm.exec('view', ['pink'])
    t.matchSnapshot(consoleLogs.join('\n'))
    t.matchSnapshot(logs.warn, 'should have warning of ignoring workspaces')
  })
})

t.test('completion', async t => {
  const { npm } = await loadMockNpm(t)
  const view = await npm.cmd('view')
  const res = await view.completion({
    conf: { argv: { remain: ['npm', 'view', 'green@1.0.0'] } },
  })
  t.ok(res, 'returns back fields')
})

t.test('no package completion', async t => {
  const { npm } = await loadMockNpm(t)
  const view = await npm.cmd('view')
  const res = await view.completion({ conf: { argv: { remain: ['npm', 'view'] } } })
  t.notOk(res, 'there is no package completion')
  t.end()
})
