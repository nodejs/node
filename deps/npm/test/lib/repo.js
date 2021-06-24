const t = require('tap')
const { fake: mockNpm } = require('../fixtures/mock-npm.js')
const { join, sep } = require('path')

const pkgDirs = t.testdir({
  'package.json': JSON.stringify({
    name: 'thispkg',
    version: '1.2.3',
    repository: 'https://example.com/thispkg.git',
  }),
  norepo: {
    'package.json': JSON.stringify({
      name: 'norepo',
      version: '1.2.3',
    }),
  },
  'repoobbj-nourl': {
    'package.json': JSON.stringify({
      name: 'repoobj-nourl',
      repository: { no: 'url' },
    }),
  },
  hostedgit: {
    'package.json': JSON.stringify({
      repository: 'git://github.com/foo/hostedgit',
    }),
  },
  hostedgitat: {
    'package.json': JSON.stringify({
      repository: 'git@github.com:foo/hostedgitat',
    }),
  },
  hostedssh: {
    'package.json': JSON.stringify({
      repository: 'ssh://git@github.com/foo/hostedssh',
    }),
  },
  hostedgitssh: {
    'package.json': JSON.stringify({
      repository: 'git+ssh://git@github.com/foo/hostedgitssh',
    }),
  },
  hostedgithttp: {
    'package.json': JSON.stringify({
      repository: 'git+http://github.com/foo/hostedgithttp',
    }),
  },
  hostedgithttps: {
    'package.json': JSON.stringify({
      repository: 'git+https://github.com/foo/hostedgithttps',
    }),
  },
  hostedgitobj: {
    'package.json': JSON.stringify({
      repository: { url: 'git://github.com/foo/hostedgitobj' },
    }),
  },
  hostedgitatobj: {
    'package.json': JSON.stringify({
      repository: { url: 'git@github.com:foo/hostedgitatobj' },
    }),
  },
  hostedsshobj: {
    'package.json': JSON.stringify({
      repository: { url: 'ssh://git@github.com/foo/hostedsshobj' },
    }),
  },
  hostedgitsshobj: {
    'package.json': JSON.stringify({
      repository: { url: 'git+ssh://git@github.com/foo/hostedgitsshobj' },
    }),
  },
  hostedgithttpobj: {
    'package.json': JSON.stringify({
      repository: { url: 'git+http://github.com/foo/hostedgithttpobj' },
    }),
  },
  hostedgithttpsobj: {
    'package.json': JSON.stringify({
      repository: { url: 'git+https://github.com/foo/hostedgithttpsobj' },
    }),
  },
  unhostedgit: {
    'package.json': JSON.stringify({
      repository: 'git://gothib.com/foo/unhostedgit',
    }),
  },
  unhostedgitat: {
    'package.json': JSON.stringify({
      repository: 'git@gothib.com:foo/unhostedgitat',
    }),
  },
  unhostedssh: {
    'package.json': JSON.stringify({
      repository: 'ssh://git@gothib.com/foo/unhostedssh',
    }),
  },
  unhostedgitssh: {
    'package.json': JSON.stringify({
      repository: 'git+ssh://git@gothib.com/foo/unhostedgitssh',
    }),
  },
  unhostedgithttp: {
    'package.json': JSON.stringify({
      repository: 'git+http://gothib.com/foo/unhostedgithttp',
    }),
  },
  unhostedgithttps: {
    'package.json': JSON.stringify({
      repository: 'git+https://gothib.com/foo/unhostedgithttps',
    }),
  },
  unhostedgitobj: {
    'package.json': JSON.stringify({
      repository: { url: 'git://gothib.com/foo/unhostedgitobj' },
    }),
  },
  unhostedgitatobj: {
    'package.json': JSON.stringify({
      repository: { url: 'git@gothib.com:foo/unhostedgitatobj' },
    }),
  },
  unhostedsshobj: {
    'package.json': JSON.stringify({
      repository: { url: 'ssh://git@gothib.com/foo/unhostedsshobj' },
    }),
  },
  unhostedgitsshobj: {
    'package.json': JSON.stringify({
      repository: { url: 'git+ssh://git@gothib.com/foo/unhostedgitsshobj' },
    }),
  },
  unhostedgithttpobj: {
    'package.json': JSON.stringify({
      repository: { url: 'git+http://gothib.com/foo/unhostedgithttpobj' },
    }),
  },
  unhostedgithttpsobj: {
    'package.json': JSON.stringify({
      repository: { url: 'git+https://gothib.com/foo/unhostedgithttpsobj' },
    }),
  },
  directory: {
    'package.json': JSON.stringify({
      repository: {
        type: 'git',
        url: 'git+https://github.com/foo/test-repo-with-directory.git',
        directory: 'some/directory',
      },
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
        repository: 'http://repo.workspace-a/',
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

const Repo = t.mock('../../lib/repo.js', {
  '../../lib/utils/open-url.js': openUrl,
})
const flatOptions = {}
const npm = mockNpm({ flatOptions })
const repo = new Repo(npm)

t.afterEach(() => opened = {})

t.test('open repo urls', t => {
  // XXX It is very odd that `where` is how pacote knows to look anywhere other
  // than the cwd. I would think npm.localPrefix would factor in somehow
  flatOptions.where = pkgDirs
  const expect = {
    hostedgit: 'https://github.com/foo/hostedgit',
    hostedgitat: 'https://github.com/foo/hostedgitat',
    hostedssh: 'https://github.com/foo/hostedssh',
    hostedgitssh: 'https://github.com/foo/hostedgitssh',
    hostedgithttp: 'https://github.com/foo/hostedgithttp',
    hostedgithttps: 'https://github.com/foo/hostedgithttps',
    hostedgitobj: 'https://github.com/foo/hostedgitobj',
    hostedgitatobj: 'https://github.com/foo/hostedgitatobj',
    hostedsshobj: 'https://github.com/foo/hostedsshobj',
    hostedgitsshobj: 'https://github.com/foo/hostedgitsshobj',
    hostedgithttpobj: 'https://github.com/foo/hostedgithttpobj',
    hostedgithttpsobj: 'https://github.com/foo/hostedgithttpsobj',
    unhostedgit: 'https://gothib.com/foo/unhostedgit',
    unhostedssh: 'https://gothib.com/foo/unhostedssh',
    unhostedgitssh: 'https://gothib.com/foo/unhostedgitssh',
    unhostedgithttp: 'http://gothib.com/foo/unhostedgithttp',
    unhostedgithttps: 'https://gothib.com/foo/unhostedgithttps',
    unhostedgitobj: 'https://gothib.com/foo/unhostedgitobj',
    unhostedsshobj: 'https://gothib.com/foo/unhostedsshobj',
    unhostedgitsshobj: 'https://gothib.com/foo/unhostedgitsshobj',
    unhostedgithttpobj: 'http://gothib.com/foo/unhostedgithttpobj',
    unhostedgithttpsobj: 'https://gothib.com/foo/unhostedgithttpsobj',
    directory: 'https://github.com/foo/test-repo-with-directory/tree/master/some/directory',
    '.': 'https://example.com/thispkg',
  }
  const keys = Object.keys(expect)
  t.plan(keys.length)
  keys.forEach(pkg => {
    t.test(pkg, t => {
      repo.exec([['.', pkg].join(sep)], (err) => {
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

t.test('fail if cannot figure out repo url', t => {
  flatOptions.where = pkgDirs
  const cases = [
    'norepo',
    'repoobbj-nourl',
    'unhostedgitat',
    'unhostedgitatobj',
  ]

  t.plan(cases.length)

  cases.forEach(pkg => {
    t.test(pkg, t => {
      repo.exec([['.', pkg].join(sep)], (err) => {
        t.match(err, { pkgid: pkg })
        t.end()
      })
    })
  })
})

t.test('open default package if none specified', t => {
  flatOptions.where = pkgDirs
  repo.exec([], (er) => {
    if (er)
      throw er
    t.equal(opened['https://example.com/thispkg'], 1, 'opened expected url', {opened})
    t.end()
  })
})

t.test('workspaces', t => {
  flatOptions.where = undefined
  npm.localPrefix = join(pkgDirs, 'workspaces')

  t.test('all workspaces', (t) => {
    repo.execWorkspaces([], [], (err) => {
      t.notOk(err)
      t.match({
        'https://repo.workspace-a/': 1, // Gets translated to https!
        'https://github.com/npm/workspace-b': 1,
      }, opened, 'opened two valid repo urls')
      t.end()
    })
  })

  t.test('one workspace', (t) => {
    repo.execWorkspaces([], ['workspace-a'], (err) => {
      t.notOk(err)
      t.match({
        'https://repo.workspace-a/': 1,
      }, opened, 'opened one requested repo urls')
      t.end()
    })
  })

  t.test('invalid workspace', (t) => {
    repo.execWorkspaces([], ['workspace-x'], (err) => {
      t.match(err, /No workspaces found/)
      t.match(err, /workspace-x/)
      t.match({}, opened, 'opened no repo urls')
      t.end()
    })
  })
  t.end()
})
