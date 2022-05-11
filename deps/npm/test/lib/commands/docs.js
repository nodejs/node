const t = require('tap')
const { fake: mockNpm } = require('../../fixtures/mock-npm.js')
const { join, sep } = require('path')

const pkgDirs = t.testdir({
  'package.json': JSON.stringify({
    name: 'thispkg',
    version: '1.2.3',
    homepage: 'https://example.com',
  }),
  nodocs: {
    'package.json': JSON.stringify({
      name: 'nodocs',
      version: '1.2.3',
    }),
  },
  docsurl: {
    'package.json': JSON.stringify({
      name: 'docsurl',
      version: '1.2.3',
      homepage: 'https://bugzilla.localhost/docsurl',
    }),
  },
  repourl: {
    'package.json': JSON.stringify({
      name: 'repourl',
      version: '1.2.3',
      repository: 'https://github.com/foo/repourl',
    }),
  },
  repoobj: {
    'package.json': JSON.stringify({
      name: 'repoobj',
      version: '1.2.3',
      repository: { url: 'https://github.com/foo/repoobj' },
    }),
  },
  repourlobj: {
    'package.json': JSON.stringify({
      name: 'repourlobj',
      version: '1.2.3',
      repository: { url: { works: false } },
    }),
  },
  workspaces: {
    'package.json': JSON.stringify({
      name: 'workspaces-test',
      version: '1.2.3-test',
      workspaces: ['workspace-a', 'workspace-b', 'workspace-c'],
    }),
    'workspace-a': {
      'package.json': JSON.stringify({
        name: 'workspace-a',
        version: '1.2.3-a',
        homepage: 'http://docs.workspace-a/',
      }),
    },
    'workspace-b': {
      'package.json': JSON.stringify({
        name: 'workspace-b',
        version: '1.2.3-n',
        repository: 'https://github.com/npm/workspace-b',
      }),
    },
    'workspace-c': JSON.stringify({
      'package.json': {
        name: 'workspace-n',
        version: '1.2.3-n',
      },
    }),
  },
})

// keep a tally of which urls got opened
let opened = {}
const openUrl = async (npm, url, errMsg) => {
  opened[url] = opened[url] || 0
  opened[url]++
}

const Docs = t.mock('../../../lib/commands/docs.js', {
  '../../../lib/utils/open-url.js': openUrl,
})
const flatOptions = {}
const npm = mockNpm({ flatOptions })
const docs = new Docs(npm)

t.afterEach(() => opened = {})

t.test('open docs urls', t => {
  npm.localPrefix = pkgDirs
  const expect = {
    nodocs: 'https://www.npmjs.com/package/nodocs',
    docsurl: 'https://bugzilla.localhost/docsurl',
    repourl: 'https://github.com/foo/repourl#readme',
    repoobj: 'https://github.com/foo/repoobj#readme',
    repourlobj: 'https://www.npmjs.com/package/repourlobj',
    '.': 'https://example.com',
  }
  const keys = Object.keys(expect)
  t.plan(keys.length)
  keys.forEach(pkg => {
    t.test(pkg, async t => {
      await docs.exec([['.', pkg].join(sep)])
      const url = expect[pkg]
      t.match({
        [url]: 1,
      }, opened, `opened ${url}`, { opened })
    })
  })
})

t.test('open default package if none specified', async t => {
  await docs.exec([])
  t.equal(opened['https://example.com'], 1, 'opened expected url', { opened })
})

t.test('workspaces', (t) => {
  npm.localPrefix = join(pkgDirs, 'workspaces')
  t.test('all workspaces', async t => {
    await docs.execWorkspaces([], [])
    t.match({
      'http://docs.workspace-a/': 1,
      'https://github.com/npm/workspace-b#readme': 1,
    }, opened, 'opened two valid docs urls')
  })

  t.test('one workspace', async t => {
    await docs.execWorkspaces([], ['workspace-a'])
    t.match({
      'http://docs.workspace-a/': 1,
    }, opened, 'opened one requested docs urls')
  })

  t.test('invalid workspace', async t => {
    await t.rejects(
      docs.execWorkspaces([], ['workspace-x']),
      /No workspaces found/
    )
    await t.rejects(
      docs.execWorkspaces([], ['workspace-x']),
      /workspace-x/
    )
    t.match({}, opened, 'opened no docs urls')
  })
  t.end()
})
