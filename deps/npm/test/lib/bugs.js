const t = require('tap')

const pacote = {
  manifest: async (spec, options) => {
    return spec === 'nobugs' ? {
      name: 'nobugs',
      version: '1.2.3',
    }
      : spec === 'bugsurl' ? {
        name: 'bugsurl',
        version: '1.2.3',
        bugs: 'https://bugzilla.localhost/bugsurl',
      }
      : spec === 'bugsobj' ? {
        name: 'bugsobj',
        version: '1.2.3',
        bugs: { url: 'https://bugzilla.localhost/bugsobj' },
      }
      : spec === 'bugsobj-nourl' ? {
        name: 'bugsobj-nourl',
        version: '1.2.3',
        bugs: { no: 'url here' },
      }
      : spec === 'repourl' ? {
        name: 'repourl',
        version: '1.2.3',
        repository: 'https://github.com/foo/repourl',
      }
      : spec === 'repoobj' ? {
        name: 'repoobj',
        version: '1.2.3',
        repository: { url: 'https://github.com/foo/repoobj' },
      }
      : spec === 'mailtest' ? {
        name: 'mailtest',
        version: '3.7.4',
        bugs: { email: 'hello@example.com' },
      }
      : spec === 'secondmailtest' ? {
        name: 'secondmailtest',
        version: '0.1.1',
        bugs: { email: 'ABC432abc@a.b.example.net' },
      }
      : spec === '.' ? {
        name: 'thispkg',
        version: '1.2.3',
        bugs: 'https://example.com',
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

const Bugs = t.mock('../../lib/bugs.js', {
  pacote,
  '../../lib/utils/open-url.js': openUrl,
})

const bugs = new Bugs({ flatOptions: {} })

t.test('usage', (t) => {
  t.match(bugs.usage, 'bugs', 'usage has command name in it')
  t.end()
})

t.test('open bugs urls & emails', t => {
  const expect = {
    nobugs: 'https://www.npmjs.com/package/nobugs',
    'bugsobj-nourl': 'https://www.npmjs.com/package/bugsobj-nourl',
    bugsurl: 'https://bugzilla.localhost/bugsurl',
    bugsobj: 'https://bugzilla.localhost/bugsobj',
    repourl: 'https://github.com/foo/repourl/issues',
    repoobj: 'https://github.com/foo/repoobj/issues',
    mailtest: 'mailto:hello@example.com',
    secondmailtest: 'mailto:ABC432abc@a.b.example.net',
    '.': 'https://example.com',
  }
  const keys = Object.keys(expect)
  t.plan(keys.length)
  keys.forEach(pkg => {
    t.test(pkg, t => {
      bugs.exec([pkg], (er) => {
        if (er)
          throw er
        t.equal(opened[expect[pkg]], 1, 'opened expected url', {opened})
        t.end()
      })
    })
  })
})

t.test('open default package if none specified', t => {
  bugs.exec([], (er) => {
    if (er)
      throw er
    t.equal(opened['https://example.com'], 2, 'opened expected url', {opened})
    t.end()
  })
})
