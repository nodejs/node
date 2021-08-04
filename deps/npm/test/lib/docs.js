const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm.js')
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

const Docs = t.mock('../../lib/docs.js', {
  '../../lib/utils/open-url.js': openUrl,
})
const flatOptions = {}
const npm = mockNpm({ flatOptions })
const docs = new Docs(npm)

t.afterEach(() => opened = {})

t.test('open docs urls', t => {
  // XXX It is very odd that `where` is how pacote knows to look anywhere other
  // than the cwd. I would think npm.localPrefix would factor in somehow
  flatOptions.where = pkgDirs
  const expect = {
    nodocs: 'https://www.npmjs.com/package/nodocs',
    docsurl: 'https://bugzilla.localhost/docsurl',
    repourl: 'https://github.com/foo/repourl#readme',
    repoobj: 'https://github.com/foo/repoobj#readme',
    '.': 'https://example.com',
  }
  const keys = Object.keys(expect)
  t.plan(keys.length)
  keys.forEach(pkg => {
    t.test(pkg, t => {
      docs.exec([['.', pkg].join(sep)], (err) => {
        if (err)
          throw err
        const url = expect[pkg]
        t.match({
          [url]: 1,
        }, opened, `opened ${url}`, {opened})
        t.end()
      })
    })
  })
})

t.test('open default package if none specified', t => {
  docs.exec([], (er) => {
    if (er)
      throw er
    t.equal(opened['https://example.com'], 1, 'opened expected url', {opened})
    t.end()
  })
})

t.test('workspaces', (t) => {
  flatOptions.where = undefined
  npm.localPrefix = join(pkgDirs, 'workspaces')
  t.test('all workspaces', (t) => {
    docs.execWorkspaces([], [], (err) => {
      t.notOk(err)
      t.match({
        'http://docs.workspace-a/': 1,
        'https://github.com/npm/workspace-b#readme': 1,
      }, opened, 'opened two valid docs urls')
      t.end()
    })
  })

  t.test('one workspace', (t) => {
    docs.execWorkspaces([], ['workspace-a'], (err) => {
      t.notOk(err)
      t.match({
        'http://docs.workspace-a/': 1,
      }, opened, 'opened one requested docs urls')
      t.end()
    })
  })

  t.test('invalid workspace', (t) => {
    docs.execWorkspaces([], ['workspace-x'], (err) => {
      t.match(err, /No workspaces found/)
      t.match(err, /workspace-x/)
      t.match({}, opened, 'opened no docs urls')
      t.end()
    })
  })
  t.end()
})
