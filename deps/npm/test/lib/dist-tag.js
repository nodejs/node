const requireInject = require('require-inject')
const { test } = require('tap')

let prefix
let result = ''
let log = ''

// these declared opts are used in ./utils/read-local-package.js
const _flatOptions = {
  global: false,
  get prefix () {
    return prefix
  },
}

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

const distTag = requireInject('../../lib/dist-tag.js', {
  npmlog: {
    error: logger,
    info: logger,
    verbose: logger,
    warn: logger,
  },
  get 'npm-registry-fetch' () {
    return npmRegistryFetchMock
  },
  '../../lib/npm.js': {
    flatOptions: _flatOptions,
    config: {
      get (key) {
        return _flatOptions[key]
      },
    },
  },
  '../../lib/utils/output.js': msg => {
    result = msg
  },
})

test('ls in current package', (t) => {
  prefix = t.testdir({
    'package.json': JSON.stringify({
      name: '@scoped/pkg',
    }),
  })
  distTag(['ls'], (err) => {
    t.ifError(err, 'npm dist-tags ls')
    t.matchSnapshot(
      result,
      'should list available tags for current package'
    )
    result = ''
    log = ''
    t.end()
  })
})

test('no args in current package', (t) => {
  prefix = t.testdir({
    'package.json': JSON.stringify({
      name: '@scoped/pkg',
    }),
  })
  distTag([], (err) => {
    t.ifError(err, 'npm dist-tags ls')
    t.matchSnapshot(
      result,
      'should default to listing available tags for current package'
    )
    result = ''
    log = ''
    t.end()
  })
})

test('borked cmd usage', (t) => {
  prefix = t.testdir({})
  distTag(['borked', '@scoped/pkg'], (err) => {
    t.matchSnapshot(err, 'should show usage error')
    result = ''
    log = ''
    t.end()
  })
})

test('ls on named package', (t) => {
  prefix = t.testdir({})
  distTag(['ls', '@scoped/another'], (err) => {
    t.ifError(err, 'npm dist-tags ls')
    t.matchSnapshot(
      result,
      'should list tags for the specified package'
    )
    result = ''
    log = ''
    t.end()
  })
})

test('ls on missing package', (t) => {
  prefix = t.testdir({})
  distTag(['ls', 'foo'], (err) => {
    t.matchSnapshot(
      log,
      'should log no dist-tag found msg'
    )
    t.matchSnapshot(
      err,
      'should throw error message'
    )
    result = ''
    log = ''
    t.end()
  })
})

test('ls on missing name in current package', (t) => {
  prefix = t.testdir({
    'package.json': JSON.stringify({
      version: '1.0.0',
    }),
  })
  distTag(['ls'], (err) => {
    t.matchSnapshot(
      err,
      'should throw usage error message'
    )
    result = ''
    log = ''
    t.end()
  })
})

test('only named package arg', (t) => {
  prefix = t.testdir({})
  distTag(['@scoped/another'], (err) => {
    t.ifError(err, 'npm dist-tags ls')
    t.matchSnapshot(
      result,
      'should default to listing tags for the specified package'
    )
    result = ''
    log = ''
    t.end()
  })
})

test('add new tag', (t) => {
  const _nrf = npmRegistryFetchMock
  npmRegistryFetchMock = async (url, opts) => {
    t.equal(opts.method, 'PUT', 'should trigger request to add new tag')
    t.equal(opts.body, '7.7.7', 'should point to expected version')
  }
  prefix = t.testdir({})
  distTag(['add', '@scoped/another@7.7.7', 'c'], (err) => {
    t.ifError(err, 'npm dist-tags add')
    t.matchSnapshot(
      result,
      'should return success msg'
    )
    result = ''
    log = ''
    npmRegistryFetchMock = _nrf
    t.end()
  })
})

test('add using valid semver range as name', (t) => {
  prefix = t.testdir({})
  distTag(['add', '@scoped/another@7.7.7', '1.0.0'], (err) => {
    t.match(
      err,
      /Error: Tag name must not be a valid SemVer range: 1.0.0/,
      'should exit with semver range error'
    )
    t.matchSnapshot(
      log,
      'should return success msg'
    )
    result = ''
    log = ''
    t.end()
  })
})

test('add missing args', (t) => {
  prefix = t.testdir({})
  distTag(['add', '@scoped/another@7.7.7'], (err) => {
    t.matchSnapshot(err, 'should exit usage error message')
    result = ''
    log = ''
    t.end()
  })
})

test('add missing pkg name', (t) => {
  prefix = t.testdir({})
  distTag(['add', null], (err) => {
    t.matchSnapshot(err, 'should exit usage error message')
    result = ''
    log = ''
    t.end()
  })
})

test('set existing version', (t) => {
  prefix = t.testdir({})
  distTag(['set', '@scoped/another@0.6.0', 'b'], (err) => {
    t.ifError(err, 'npm dist-tags set')
    t.matchSnapshot(
      log,
      'should log warn msg'
    )
    log = ''
    t.end()
  })
})

test('remove existing tag', (t) => {
  const _nrf = npmRegistryFetchMock
  npmRegistryFetchMock = async (url, opts) => {
    t.equal(opts.method, 'DELETE', 'should trigger request to remove tag')
  }
  prefix = t.testdir({})
  distTag(['rm', '@scoped/another', 'c'], (err) => {
    t.ifError(err, 'npm dist-tags rm')
    t.matchSnapshot(log, 'should log remove info')
    t.matchSnapshot(result, 'should return success msg')
    result = ''
    log = ''
    npmRegistryFetchMock = _nrf
    t.end()
  })
})

test('remove non-existing tag', (t) => {
  prefix = t.testdir({})
  distTag(['rm', '@scoped/another', 'nonexistent'], (err) => {
    t.match(
      err,
      /Error: nonexistent is not a dist-tag on @scoped\/another/,
      'should exit with error'
    )
    t.matchSnapshot(log, 'should log error msg')
    result = ''
    log = ''
    t.end()
  })
})

test('remove missing pkg name', (t) => {
  prefix = t.testdir({})
  distTag(['rm', null], (err) => {
    t.matchSnapshot(err, 'should exit usage error message')
    result = ''
    log = ''
    t.end()
  })
})

test('completion', t => {
  const { completion } = distTag
  t.plan(3)

  completion({
    conf: {
      argv: {
        remain: ['npm', 'dist-tag'],
      },
    },
  }, (err, res) => {
    t.ifError(err, 'npm dist-tags completion')

    t.strictSame(
      res,
      [
        'add',
        'rm',
        'ls',
      ],
      'should list npm dist-tag commands for completion'
    )
  })

  completion({
    conf: {
      argv: {
        remain: ['npm', 'dist-tag', 'foobar'],
      },
    },
  }, (err) => {
    t.notOk(err, 'should ignore any unkown name')
  })
})
