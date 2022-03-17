const t = require('tap')

t.cleanSnapshot = str => str
  .replace(/(published ).*?( ago)/g, '$1{TIME}$2')

// run the same as tap does when running directly with node
process.stdout.columns = undefined

const { fake: mockNpm } = require('../../fixtures/mock-npm')

let logs
const cleanLogs = () => {
  logs = ''
  const fn = (...args) => {
    logs += '\n'
    args.map(el => logs += el)
  }
  console.log = fn
}

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
            unpackedSize: 1,
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
            unpackedSize: 1,
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
            unpackedSize: 1,
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

t.beforeEach(cleanLogs)

t.test('should log package info', async t => {
  const View = t.mock('../../../lib/commands/view.js', {
    pacote: {
      packument,
    },
  })
  const npm = mockNpm({
    config: { unicode: false },
  })
  const view = new View(npm)

  const ViewJson = t.mock('../../../lib/commands/view.js', {
    pacote: {
      packument,
    },
  })
  const jsonNpm = mockNpm({
    config: {
      json: true,
      tag: 'latest',
    },
  })
  const viewJson = new ViewJson(jsonNpm)

  const ViewUnicode = t.mock('../../../lib/commands/view.js', {
    pacote: {
      packument,
    },
  })
  const unicodeNpm = mockNpm({
    config: { unicode: true },
  })
  const viewUnicode = new ViewUnicode(unicodeNpm)

  t.test('package from git', async t => {
    await view.exec(['https://github.com/npm/green'])
    t.matchSnapshot(logs)
  })

  t.test('package with license, bugs, repository and other fields', async t => {
    await view.exec(['green@1.0.0'])
    t.matchSnapshot(logs)
  })

  t.test('package with more than 25 deps', async t => {
    await view.exec(['black@1.0.0'])
    t.matchSnapshot(logs)
  })

  t.test('package with maintainers info as object', async t => {
    await view.exec(['pink@1.0.0'])
    t.matchSnapshot(logs)
  })

  t.test('package with homepage', async t => {
    await view.exec(['orange@1.0.0'])
    t.matchSnapshot(logs)
  })

  t.test('package with no versions', async t => {
    await view.exec(['brown'])
    t.equal(logs, '', 'no info to display')
  })

  t.test('package with no repo or homepage', async t => {
    await view.exec(['blue@1.0.0'])
    t.matchSnapshot(logs)
  })

  t.test('package with semver range', async t => {
    await view.exec(['blue@^1.0.0'])
    t.matchSnapshot(logs)
  })

  t.test('package with no modified time', async t => {
    await viewUnicode.exec(['cyan@1.0.0'])
    t.matchSnapshot(logs)
  })

  t.test('package with --json and semver range', async t => {
    await viewJson.exec(['cyan@^1.0.0'])
    t.matchSnapshot(logs)
  })

  t.test('package with --json and no versions', async t => {
    await viewJson.exec(['brown'])
    t.equal(logs, '', 'no info to display')
  })
})

t.test('should log info of package in current working dir', async t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'blue',
      version: '1.0.0',
    }, null, 2),
  })

  const View = t.mock('../../../lib/commands/view.js', {
    pacote: {
      packument,
    },
  })
  const npm = mockNpm({
    prefix: testDir,
    config: {
      tag: '1.0.0',
    },
  })
  const view = new View(npm)

  t.test('specific version', async t => {
    await view.exec(['.@1.0.0'])
    t.matchSnapshot(logs)
  })

  t.test('non-specific version', async t => {
    await view.exec(['.'])
    t.matchSnapshot(logs)
  })

  t.test('directory', async t => {
    await view.exec(['./blue'])
    t.matchSnapshot(logs)
  })
})

t.test('should log info by field name', async t => {
  const ViewJson = t.mock('../../../lib/commands/view.js', {
    pacote: {
      packument,
    },
  })
  const jsonNpm = mockNpm({
    config: {
      tag: 'latest',
      json: true,
    },
  })

  const viewJson = new ViewJson(jsonNpm)

  const View = t.mock('../../../lib/commands/view.js', {
    pacote: {
      packument,
    },
  })
  const npm = mockNpm()
  const view = new View(npm)

  t.test('readme', async t => {
    await view.exec(['yellow@1.0.0', 'readme'])
    t.matchSnapshot(logs)
  })

  t.test('several fields', async t => {
    await viewJson.exec(['yellow@1.0.0', 'name', 'version', 'foo[bar]'])
    t.matchSnapshot(logs)
  })

  t.test('several fields with several versions', async t => {
    await view.exec(['yellow@1.x.x', 'author'])
    t.matchSnapshot(logs)
  })

  t.test('nested field with brackets', async t => {
    await viewJson.exec(['orange@1.0.0', 'dist[shasum]'])
    t.matchSnapshot(logs)
  })

  t.test('maintainers with email', async t => {
    await viewJson.exec(['yellow@1.0.0', 'maintainers', 'name'])
    t.matchSnapshot(logs)
  })

  t.test('maintainers with url', async t => {
    await viewJson.exec(['pink@1.0.0', 'maintainers'])
    t.matchSnapshot(logs)
  })

  t.test('unknown nested field ', async t => {
    await view.exec(['yellow@1.0.0', 'dist.foobar'])
    t.equal(logs, '', 'no info to display')
  })

  t.test('array field - 1 element', async t => {
    await view.exec(['purple@1.0.0', 'maintainers.name'])
    t.matchSnapshot(logs)
  })

  t.test('array field - 2 elements', async t => {
    await view.exec(['yellow@1.x.x', 'maintainers.name'])
    t.matchSnapshot(logs)
  })
})

t.test('throw error if global mode', async t => {
  const View = t.mock('../../../lib/commands/view.js')
  const npm = mockNpm({
    config: {
      global: true,
      tag: 'latest',
    },
  })
  const view = new View(npm)
  await t.rejects(
    view.exec([]),
    /Cannot use view command in global mode./
  )
})

t.test('throw ENOENT error if package.json missing', async t => {
  const testDir = t.testdir({})

  const View = t.mock('../../../lib/commands/view.js')
  const npm = mockNpm({
    prefix: testDir,
  })
  const view = new View(npm)
  await t.rejects(
    view.exec([]),
    { code: 'ENOENT' }
  )
})

t.test('throw EJSONPARSE error if package.json not json', async t => {
  const testDir = t.testdir({
    'package.json': 'not json, nope, not even a little bit!',
  })

  const View = t.mock('../../../lib/commands/view.js')
  const npm = mockNpm({
    prefix: testDir,
  })
  const view = new View(npm)
  await t.rejects(
    view.exec([]),
    { code: 'EJSONPARSE' }
  )
})

t.test('throw error if package.json has no name', async t => {
  const testDir = t.testdir({
    'package.json': '{}',
  })

  const View = t.mock('../../../lib/commands/view.js')
  const npm = mockNpm({
    prefix: testDir,
  })
  const view = new View(npm)
  await t.rejects(
    view.exec([]),
    /Invalid package.json, no "name" field/
  )
})

t.test('throws when unpublished', async t => {
  const View = t.mock('../../../lib/commands/view.js', {
    pacote: {
      packument,
    },
  })
  const npm = mockNpm({
    config: {
      tag: '1.0.1',
    },
  })
  const view = new View(npm)
  await t.rejects(
    view.exec(['red']),
    { code: 'E404', pkgid: 'red@1.0.1', message: 'Unpublished on 2012-12-20T00:00:00.000Z' }
  )
})

t.test('workspaces', async t => {
  t.beforeEach(() => {
    warnMsg = undefined
    config.json = false
  })
  const testDir = t.testdir({
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
  })
  const View = t.mock('../../../lib/commands/view.js', {
    pacote: {
      packument,
    },
    'proc-log': {
      warn: (msg) => {
        warnMsg = msg
      },
      silly: () => {},
    },
  })
  const config = {
    unicode: false,
    tag: 'latest',
  }
  let warnMsg
  const npm = mockNpm({
    config,
    localPrefix: testDir,
  })
  const view = new View(npm)

  t.test('all workspaces', async t => {
    await view.execWorkspaces([], [])
    t.matchSnapshot(logs)
  })

  t.test('one specific workspace', async t => {
    await view.execWorkspaces([], ['green'])
    t.matchSnapshot(logs)
  })

  t.test('all workspaces --json', async t => {
    config.json = true
    await view.execWorkspaces([], [])
    t.matchSnapshot(logs)
  })

  t.test('all workspaces single field', async t => {
    await view.execWorkspaces(['.', 'name'], [])
    t.matchSnapshot(logs)
  })

  t.test('all workspaces nonexistent field', async t => {
    await view.execWorkspaces(['.', 'foo'], [])
    t.matchSnapshot(logs)
  })

  t.test('all workspaces nonexistent field --json', async t => {
    config.json = true
    await view.execWorkspaces(['.', 'foo'], [])
    t.matchSnapshot(logs)
  })

  t.test('all workspaces single field --json', async t => {
    config.json = true
    await view.execWorkspaces(['.', 'name'], [])
    t.matchSnapshot(logs)
  })

  t.test('single workspace --json', async t => {
    config.json = true
    await view.execWorkspaces([], ['green'])
    t.matchSnapshot(logs)
  })

  t.test('remote package name', async t => {
    await view.execWorkspaces(['pink'], [])
    t.matchSnapshot(warnMsg)
    t.matchSnapshot(logs)
  })
})

t.test('completion', async t => {
  const View = t.mock('../../../lib/commands/view.js', {
    pacote: {
      packument,
    },
  })
  const npm = mockNpm({
    config: {
      tag: '1.0.1',
    },
  })
  const view = new View(npm)
  const res = await view.completion({
    conf: { argv: { remain: ['npm', 'view', 'green@1.0.0'] } },
  })
  t.ok(res, 'returns back fields')
})

t.test('no registry completion', async t => {
  const View = t.mock('../../../lib/commands/view.js')
  const npm = mockNpm({
    config: {
      tag: '1.0.1',
    },
  })
  const view = new View(npm)
  const res = await view.completion({ conf: { argv: { remain: ['npm', 'view'] } } })
  t.notOk(res, 'there is no package completion')
  t.end()
})
