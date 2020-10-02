const t = require('tap')

const requireInject = require('require-inject')
const pacote = {
  manifest: async (spec, options) => {
    return spec === 'nodocs' ? {
      name: 'nodocs',
      version: '1.2.3'
    }
    : spec === 'docsurl' ? {
      name: 'docsurl',
      version: '1.2.3',
      homepage: 'https://bugzilla.localhost/docsurl'
    }
    : spec === 'repourl' ? {
      name: 'repourl',
      version: '1.2.3',
      repository: 'https://github.com/foo/repourl'
    }
    : spec === 'repoobj' ? {
      name: 'repoobj',
      version: '1.2.3',
      repository: { url: 'https://github.com/foo/repoobj' }
    }
    : spec === '.' ? {
      name: 'thispkg',
      version: '1.2.3',
      homepage: 'https://example.com'
    }
    : null
  }
}

// keep a tally of which urls got opened
const opened = {}
const openUrl = (url, errMsg, cb) => {
  opened[url] = opened[url] || 0
  opened[url]++
  process.nextTick(cb)
}

const docs = requireInject('../../lib/docs.js', {
  pacote,
  '../../lib/utils/open-url.js': openUrl
})

t.test('completion', t => {
  docs.completion({}, (er, res) => {
    t.equal(er, null)
    t.same(res, [])
    t.end()
  })
})

t.test('open docs urls', t => {
  const expect = {
    nodocs: 'https://www.npmjs.com/package/nodocs',
    docsurl: 'https://bugzilla.localhost/docsurl',
    repourl: 'https://github.com/foo/repourl#readme',
    repoobj: 'https://github.com/foo/repoobj#readme',
    '.': 'https://example.com'
  }
  const keys = Object.keys(expect)
  t.plan(keys.length)
  keys.forEach(pkg => {
    t.test(pkg, t => {
      docs([pkg], (er) => {
        if (er)
          throw er
        const url = expect[pkg]
        t.equal(opened[url], 1, url, {opened})
        t.end()
      })
    })
  })
})

t.test('open default package if none specified', t => {
  docs([], (er) => {
    if (er)
      throw er
    t.equal(opened['https://example.com'], 2, 'opened expected url', {opened})
    t.end()
  })
})
