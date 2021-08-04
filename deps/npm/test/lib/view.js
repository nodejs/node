const t = require('tap')

t.cleanSnapshot = str => str.replace(/published .*? ago/g, 'published {TIME} ago')

// run the same as tap does when running directly with node
process.stdout.columns = undefined

const { fake: mockNpm } = require('../fixtures/mock-npm')

let logs
const cleanLogs = () => {
  logs = ''
  const fn = (...args) => {
    logs += '\n'
    args.map(el => logs += el)
  }
  console.log = fn
}

const packument = (nv, opts) => {
  if (!opts.fullMetadata)
    throw new Error('must fetch fullMetadata')

  if (!opts.preferOnline)
    throw new Error('must fetch with preferOnline')

  const mocks = {
    red: {
      name: 'red',
      'dist-tags': {
        '1.0.1': {},
      },
      time: {
        unpublished: new Date(),
      },
    },
    blue: {
      name: 'blue',
      'dist-tags': {
        latest: '1.0.0',
      },
      time: {
        '1.0.0': '2019-08-06T16:21:09.842Z',
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
        '1.0.1': {},
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
            for (let i = 0; i < 25; i++)
              deps[i] = '1.0.0'

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
  if (nv.type === 'git')
    return mocks[nv.hosted.project]
  return mocks[nv.name]
}

t.beforeEach(cleanLogs)

t.test('should log package info', t => {
  const View = t.mock('../../lib/view.js', {
    pacote: {
      packument,
    },
  })
  const npm = mockNpm({
    config: { unicode: false },
  })
  const view = new View(npm)

  const ViewJson = t.mock('../../lib/view.js', {
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

  const ViewUnicode = t.mock('../../lib/view.js', {
    pacote: {
      packument,
    },
  })
  const unicodeNpm = mockNpm({
    config: { unicode: true },
  })
  const viewUnicode = new ViewUnicode(unicodeNpm)

  t.test('package from git', t => {
    view.exec(['https://github.com/npm/green'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('package with license, bugs, repository and other fields', t => {
    view.exec(['green@1.0.0'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('package with more than 25 deps', t => {
    view.exec(['black@1.0.0'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('package with maintainers info as object', t => {
    view.exec(['pink@1.0.0'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('package with homepage', t => {
    view.exec(['orange@1.0.0'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('package with no versions', t => {
    view.exec(['brown'], () => {
      t.equal(logs, '', 'no info to display')
      t.end()
    })
  })

  t.test('package with no repo or homepage', t => {
    view.exec(['blue@1.0.0'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('package with no modified time', t => {
    viewUnicode.exec(['cyan@1.0.0'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('package with --json and semver range', t => {
    viewJson.exec(['cyan@^1.0.0'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('package with --json and no versions', t => {
    viewJson.exec(['brown'], () => {
      t.equal(logs, '', 'no info to display')
      t.end()
    })
  })

  t.end()
})

t.test('should log info of package in current working dir', t => {
  const testDir = t.testdir({
    'package.json': JSON.stringify({
      name: 'blue',
      version: '1.0.0',
    }, null, 2),
  })

  const View = t.mock('../../lib/view.js', {
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

  t.test('specific version', t => {
    view.exec(['.@1.0.0'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('non-specific version', t => {
    view.exec(['.'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.end()
})

t.test('should log info by field name', t => {
  const ViewJson = t.mock('../../lib/view.js', {
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

  const View = t.mock('../../lib/view.js', {
    pacote: {
      packument,
    },
  })
  const npm = mockNpm()
  const view = new View(npm)

  t.test('readme', t => {
    view.exec(['yellow@1.0.0', 'readme'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('several fields', t => {
    viewJson.exec(['yellow@1.0.0', 'name', 'version', 'foo[bar]'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('several fields with several versions', t => {
    view.exec(['yellow@1.x.x', 'author'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('nested field with brackets', t => {
    viewJson.exec(['orange@1.0.0', 'dist[shasum]'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('maintainers with email', t => {
    viewJson.exec(['yellow@1.0.0', 'maintainers', 'name'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('maintainers with url', t => {
    viewJson.exec(['pink@1.0.0', 'maintainers'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('unknown nested field ', t => {
    view.exec(['yellow@1.0.0', 'dist.foobar'], () => {
      t.equal(logs, '', 'no info to display')
      t.end()
    })
  })

  t.test('array field - 1 element', t => {
    view.exec(['purple@1.0.0', 'maintainers.name'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('array field - 2 elements', t => {
    view.exec(['yellow@1.x.x', 'maintainers.name'], () => {
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.end()
})

t.test('throw error if global mode', (t) => {
  const View = t.mock('../../lib/view.js')
  const npm = mockNpm({
    config: {
      global: true,
      tag: 'latest',
    },
  })
  const view = new View(npm)
  view.exec([], (err) => {
    t.equal(err.message, 'Cannot use view command in global mode.')
    t.end()
  })
})

t.test('throw ENOENT error if package.json misisng', (t) => {
  const testDir = t.testdir({})

  const View = t.mock('../../lib/view.js')
  const npm = mockNpm({
    prefix: testDir,
  })
  const view = new View(npm)
  view.exec([], (err) => {
    t.match(err, { code: 'ENOENT' })
    t.end()
  })
})

t.test('throw EJSONPARSE error if package.json not json', (t) => {
  const testDir = t.testdir({
    'package.json': 'not json, nope, not even a little bit!',
  })

  const View = t.mock('../../lib/view.js')
  const npm = mockNpm({
    prefix: testDir,
  })
  const view = new View(npm)
  view.exec([], (err) => {
    t.match(err, { code: 'EJSONPARSE' })
    t.end()
  })
})

t.test('throw error if package.json has no name', (t) => {
  const testDir = t.testdir({
    'package.json': '{}',
  })

  const View = t.mock('../../lib/view.js')
  const npm = mockNpm({
    prefix: testDir,
  })
  const view = new View(npm)
  view.exec([], (err) => {
    t.equal(err.message, 'Invalid package.json, no "name" field')
    t.end()
  })
})

t.test('throws when unpublished', (t) => {
  const View = t.mock('../../lib/view.js', {
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
  view.exec(['red'], (err) => {
    t.equal(err.code, 'E404')
    t.end()
  })
})

t.test('workspaces', t => {
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
  const View = t.mock('../../lib/view.js', {
    pacote: {
      packument,
    },
  })
  const config = {
    unicode: false,
    tag: 'latest',
  }
  let warnMsg
  const npm = mockNpm({
    log: {
      warn: (msg) => {
        warnMsg = msg
      },
    },
    config,
    localPrefix: testDir,
  })
  const view = new View(npm)

  t.test('all workspaces', t => {
    view.execWorkspaces([], [], (err) => {
      t.error(err)
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('one specific workspace', t => {
    view.execWorkspaces([], ['green'], (err) => {
      t.error(err)
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('all workspaces --json', t => {
    config.json = true
    view.execWorkspaces([], [], (err) => {
      t.error(err)
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('all workspaces single field', t => {
    view.execWorkspaces(['.', 'name'], [], (err) => {
      t.error(err)
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('all workspaces nonexistent field', t => {
    view.execWorkspaces(['.', 'foo'], [], (err) => {
      t.error(err)
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('all workspaces nonexistent field --json', t => {
    config.json = true
    view.execWorkspaces(['.', 'foo'], [], (err) => {
      t.error(err)
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('all workspaces single field --json', t => {
    config.json = true
    view.execWorkspaces(['.', 'name'], [], (err) => {
      t.error(err)
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('single workspace --json', t => {
    config.json = true
    view.execWorkspaces([], ['green'], (err) => {
      t.error(err)
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.test('remote package name', t => {
    view.execWorkspaces(['pink'], [], (err) => {
      t.error(err)
      t.matchSnapshot(warnMsg)
      t.matchSnapshot(logs)
      t.end()
    })
  })

  t.end()
})

t.test('completion', async t => {
  const View = t.mock('../../lib/view.js', {
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
  t.end()
})

t.test('no registry completion', async t => {
  const View = t.mock('../../lib/view.js')
  const npm = mockNpm({
    config: {
      tag: '1.0.1',
    },
  })
  const view = new View(npm)
  const res = await view.completion({conf: { argv: { remain: ['npm', 'view'] } } })
  t.notOk(res, 'there is no package completion')
  t.end()
})
