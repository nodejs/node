const t = require('tap')

const requireInject = require('require-inject')
const pacote = {
  manifest: async (spec, options) => {
    return spec === 'norepo' ? {
      name: 'norepo',
      version: '1.2.3',
    }

      : spec === 'repoobbj-nourl' ? {
        name: 'repoobj-nourl',
        repository: { no: 'url' },
      }

      : spec === 'hostedgit' ? {
        repository: 'git://github.com/foo/hostedgit',
      }
      : spec === 'hostedgitat' ? {
        repository: 'git@github.com:foo/hostedgitat',
      }
      : spec === 'hostedssh' ? {
        repository: 'ssh://git@github.com/foo/hostedssh',
      }
      : spec === 'hostedgitssh' ? {
        repository: 'git+ssh://git@github.com/foo/hostedgitssh',
      }
      : spec === 'hostedgithttp' ? {
        repository: 'git+http://github.com/foo/hostedgithttp',
      }
      : spec === 'hostedgithttps' ? {
        repository: 'git+https://github.com/foo/hostedgithttps',
      }

      : spec === 'hostedgitobj' ? {
        repository: { url: 'git://github.com/foo/hostedgitobj' },
      }
      : spec === 'hostedgitatobj' ? {
        repository: { url: 'git@github.com:foo/hostedgitatobj' },
      }
      : spec === 'hostedsshobj' ? {
        repository: { url: 'ssh://git@github.com/foo/hostedsshobj' },
      }
      : spec === 'hostedgitsshobj' ? {
        repository: { url: 'git+ssh://git@github.com/foo/hostedgitsshobj' },
      }
      : spec === 'hostedgithttpobj' ? {
        repository: { url: 'git+http://github.com/foo/hostedgithttpobj' },
      }
      : spec === 'hostedgithttpsobj' ? {
        repository: { url: 'git+https://github.com/foo/hostedgithttpsobj' },
      }

      : spec === 'unhostedgit' ? {
        repository: 'git://gothib.com/foo/unhostedgit',
      }
      : spec === 'unhostedgitat' ? {
        repository: 'git@gothib.com:foo/unhostedgitat',
      }
      : spec === 'unhostedssh' ? {
        repository: 'ssh://git@gothib.com/foo/unhostedssh',
      }
      : spec === 'unhostedgitssh' ? {
        repository: 'git+ssh://git@gothib.com/foo/unhostedgitssh',
      }
      : spec === 'unhostedgithttp' ? {
        repository: 'git+http://gothib.com/foo/unhostedgithttp',
      }
      : spec === 'unhostedgithttps' ? {
        repository: 'git+https://gothib.com/foo/unhostedgithttps',
      }

      : spec === 'unhostedgitobj' ? {
        repository: { url: 'git://gothib.com/foo/unhostedgitobj' },
      }
      : spec === 'unhostedgitatobj' ? {
        repository: { url: 'git@gothib.com:foo/unhostedgitatobj' },
      }
      : spec === 'unhostedsshobj' ? {
        repository: { url: 'ssh://git@gothib.com/foo/unhostedsshobj' },
      }
      : spec === 'unhostedgitsshobj' ? {
        repository: { url: 'git+ssh://git@gothib.com/foo/unhostedgitsshobj' },
      }
      : spec === 'unhostedgithttpobj' ? {
        repository: { url: 'git+http://gothib.com/foo/unhostedgithttpobj' },
      }
      : spec === 'unhostedgithttpsobj' ? {
        repository: { url: 'git+https://gothib.com/foo/unhostedgithttpsobj' },
      }

      : spec === 'directory' ? {
        repository: {
          type: 'git',
          url: 'git+https://github.com/foo/test-repo-with-directory.git',
          directory: 'some/directory',
        },
      }

      : spec === '.' ? {
        name: 'thispkg',
        version: '1.2.3',
        repository: 'https://example.com/thispkg.git',
      }
      : null
  },
}

// keep a tally of which urls got opened
const opened = {}
const openUrl = async (npm, url, errMsg) => {
  opened[url] = opened[url] || 0
  opened[url]++
}

const Repo = requireInject('../../lib/repo.js', {
  pacote,
  '../../lib/utils/open-url.js': openUrl,
})
const repo = new Repo({ flatOptions: {} })

t.test('open repo urls', t => {
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
      repo.exec([pkg], (er) => {
        if (er)
          throw er
        const url = expect[pkg]
        t.equal(opened[url], 1, url, {opened})
        t.end()
      })
    })
  })
})

t.test('fail if cannot figure out repo url', t => {
  const cases = [
    'norepo',
    'repoobbj-nourl',
    'unhostedgitat',
    'unhostedgitatobj',
  ]

  t.plan(cases.length)

  cases.forEach(pkg => {
    t.test(pkg, t => {
      repo.exec([pkg], er => {
        t.match(er, { pkgid: pkg })
        t.end()
      })
    })
  })
})

t.test('open default package if none specified', t => {
  repo.exec([], (er) => {
    if (er)
      throw er
    t.equal(opened['https://example.com/thispkg'], 2, 'opened expected url', {opened})
    t.end()
  })
})
