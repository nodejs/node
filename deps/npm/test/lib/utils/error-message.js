const t = require('tap')
const path = require('path')
const { load: _loadMockNpm } = require('../../fixtures/mock-npm.js')
const mockGlobals = require('../../fixtures/mock-globals.js')
const { cleanCwd, cleanDate } = require('../../fixtures/clean-snapshot.js')

t.cleanSnapshot = p => cleanDate(cleanCwd(p))

mockGlobals(t, {
  process: {
    getuid: () => 867,
    getgid: () => 5309,
    arch: 'x64',
    version: '123.456.789-node',
    platform: 'posix',
  },
})

const loadMockNpm = async (t, { load, command, prefixDir, config } = {}) => {
  const { npm, ...rest } = await _loadMockNpm(t, {
    load,
    prefixDir,
    config,
    mocks: {
      '../../package.json': {
        version: '123.456.789-npm',
      },
    },
  })
  if (command !== undefined) {
    npm.command = command
  }
  return {
    npm,
    ...rest,
  }
}

const errorMessage = (er, { mocks, logMocks, npm } = {}) =>
  t.mock('../../../lib/utils/error-message.js', { ...mocks, ...logMocks })(er, npm)

t.test('just simple messages', async t => {
  const npm = await loadMockNpm(t, {
    command: 'audit',
    config: {
      'node-version': '99.99.99',
    },
  })
  const codes = [
    'ENOAUDIT',
    'ENOLOCK',
    'ECONNREFUSED',
    'ENOGIT',
    'EPUBLISHCONFLICT',
    'EISGIT',
    'EEXIST',
    'ENEEDAUTH',
    'ECONNRESET',
    'ENOTFOUND',
    'ETIMEDOUT',
    'EAI_FAIL',
    'EBADENGINE',
    'ENOSPC',
    'EROFS',
    'ENOENT',
    'EMISSINGARG',
    'EUNKNOWNTYPE',
    'EINVALIDTYPE',
    'ETOOMANYARGS',
    'ETARGET',
    'E403',
    'ERR_SOCKET_TIMEOUT',
  ]
  t.plan(codes.length)
  codes.forEach(async code => {
    const path = '/some/path'
    const pkgid = 'some@package'
    const file = '/some/file'
    const stack = 'dummy stack trace'
    const er = Object.assign(new Error('foo'), {
      code,
      path,
      pkgid,
      file,
      stack,
    })
    t.matchSnapshot(errorMessage(er, npm))
  })
})

t.test('replace message/stack sensistive info', async t => {
  const npm = await loadMockNpm(t, { command: 'audit' })
  const path = '/some/path'
  const pkgid = 'some@package'
  const file = '/some/file'
  const stack = 'dummy stack trace at https://user:pass@registry.npmjs.org/'
  const message = 'Error at registry: https://user:pass@registry.npmjs.org/'
  const er = Object.assign(new Error(message), {
    code: 'ENOAUDIT',
    path,
    pkgid,
    file,
    stack,
  })
  t.matchSnapshot(errorMessage(er, npm))
})

t.test('bad engine without config loaded', async t => {
  const npm = await loadMockNpm(t, { load: false })
  const path = '/some/path'
  const pkgid = 'some@package'
  const file = '/some/file'
  const stack = 'dummy stack trace'
  const er = Object.assign(new Error('foo'), {
    code: 'EBADENGINE',
    path,
    pkgid,
    file,
    stack,
  })
  t.matchSnapshot(errorMessage(er, npm))
})

t.test('enoent without a file', async t => {
  const npm = await loadMockNpm(t)
  const path = '/some/path'
  const pkgid = 'some@package'
  const stack = 'dummy stack trace'
  const er = Object.assign(new Error('foo'), {
    code: 'ENOENT',
    path,
    pkgid,
    stack,
  })
  t.matchSnapshot(errorMessage(er, npm))
})

t.test('enolock without a command', async t => {
  const npm = await loadMockNpm(t, { command: null })
  const path = '/some/path'
  const pkgid = 'some@package'
  const file = '/some/file'
  const stack = 'dummy stack trace'
  const er = Object.assign(new Error('foo'), {
    code: 'ENOLOCK',
    path,
    pkgid,
    file,
    stack,
  })
  t.matchSnapshot(errorMessage(er, npm))
})

t.test('default message', async t => {
  const npm = await loadMockNpm(t)
  t.matchSnapshot(errorMessage(new Error('error object'), npm))
  t.matchSnapshot(errorMessage('error string', npm))
  t.matchSnapshot(errorMessage(Object.assign(new Error('cmd err'), {
    cmd: 'some command',
    signal: 'SIGYOLO',
    args: ['a', 'r', 'g', 's'],
    stdout: 'stdout',
    stderr: 'stderr',
  }), npm))
})

t.test('args are cleaned', async t => {
  const npm = await loadMockNpm(t)
  t.matchSnapshot(errorMessage(Object.assign(new Error('cmd err'), {
    cmd: 'some command',
    signal: 'SIGYOLO',
    args: ['a', 'r', 'g', 's', 'https://evil:password@npmjs.org'],
    stdout: 'stdout',
    stderr: 'stderr',
  }), npm))
})

t.test('eacces/eperm', async t => {
  const runTest = (windows, loaded, cachePath, cacheDest) => async t => {
    if (windows) {
      mockGlobals(t, { 'process.platform': 'win32' })
    }
    const npm = await loadMockNpm(t, { windows, load: loaded })
    const path = `${cachePath ? npm.cache : '/not/cache/dir'}/path`
    const dest = `${cacheDest ? npm.cache : '/not/cache/dir'}/dest`
    const er = Object.assign(new Error('whoopsie'), {
      code: 'EACCES',
      path,
      dest,
      stack: 'dummy stack trace',
    })

    t.matchSnapshot(errorMessage(er, npm))
    t.matchSnapshot(npm.logs.verbose)
  }

  for (const windows of [true, false]) {
    for (const loaded of [true, false]) {
      for (const cachePath of [true, false]) {
        for (const cacheDest of [true, false]) {
          const m = JSON.stringify({ windows, loaded, cachePath, cacheDest })
          t.test(m, runTest(windows, loaded, cachePath, cacheDest))
        }
      }
    }
  }
})

t.test('json parse', t => {
  mockGlobals(t, { 'process.argv': ['arg', 'v'] })

  t.test('merge conflict in package.json', async t => {
    const prefixDir = {
      'package.json': `
{
  "array": [
<<<<<<< HEAD
    100,
    {
      "foo": "baz"
    },
||||||| merged common ancestors
    1,
=======
    111,
    1,
    2,
    3,
    {
      "foo": "bar"
    },
>>>>>>> a
    1
  ],
  "a": {
    "b": {
<<<<<<< HEAD
      "c": {
        "x": "bbbb"
      }
||||||| merged common ancestors
      "c": {
        "x": "aaaa"
      }
=======
      "c": "xxxx"
>>>>>>> a
    }
  }
}
`,
    }
    const npm = await loadMockNpm(t, { prefixDir })
    t.matchSnapshot(errorMessage(Object.assign(new Error('conflicted'), {
      code: 'EJSONPARSE',
      path: path.resolve(npm.prefix, 'package.json'),
    }), npm))
    t.end()
  })

  t.test('just regular bad json in package.json', async t => {
    const prefixDir = {
      'package.json': 'not even slightly json',
    }
    const npm = await loadMockNpm(t, { prefixDir })
    t.matchSnapshot(errorMessage(Object.assign(new Error('not json'), {
      code: 'EJSONPARSE',
      path: path.resolve(npm.prefix, 'package.json'),
    }), npm))
    t.end()
  })

  t.test('json somewhere else', async t => {
    const prefixDir = {
      'blerg.json': 'not even slightly json',
    }
    const npm = await loadMockNpm(t, { prefixDir })
    t.matchSnapshot(errorMessage(Object.assign(new Error('not json'), {
      code: 'EJSONPARSE',
      path: path.resolve(npm.prefix, 'blerg.json'),
    }), npm))
    t.end()
  })

  t.end()
})

t.test('eotp/e401', async t => {
  const npm = await loadMockNpm(t)

  t.test('401, no auth headers', t => {
    t.matchSnapshot(errorMessage(Object.assign(new Error('nope'), {
      code: 'E401',
    }), npm))
    t.end()
  })

  t.test('401, no message', t => {
    t.matchSnapshot(errorMessage({
      code: 'E401',
    }, npm))
    t.end()
  })

  t.test('one-time pass challenge code', t => {
    t.matchSnapshot(errorMessage(Object.assign(new Error('nope'), {
      code: 'EOTP',
    }), npm))
    t.end()
  })

  t.test('one-time pass challenge message', t => {
    const message = 'one-time pass'
    t.matchSnapshot(errorMessage(Object.assign(new Error(message), {
      code: 'E401',
    }), npm))
    t.end()
  })

  t.test('www-authenticate challenges', t => {
    const auths = [
      'Bearer realm=do, charset="UTF-8", challenge="yourself"',
      'Basic realm=by, charset="UTF-8", challenge="your friends"',
      'PickACardAnyCard realm=friday, charset="UTF-8"',
      'WashYourHands, charset="UTF-8"',
    ]
    t.plan(auths.length)
    for (const auth of auths) {
      t.test(auth, t => {
        const er = Object.assign(new Error('challenge!'), {
          headers: {
            'www-authenticate': [auth],
          },
          code: 'E401',
        })
        t.matchSnapshot(errorMessage(er, npm))
        t.end()
      })
    }
  })
})

t.test('404', async t => {
  const npm = await loadMockNpm(t)

  t.test('no package id', t => {
    const er = Object.assign(new Error('404 not found'), { code: 'E404' })
    t.matchSnapshot(errorMessage(er, npm))
    t.end()
  })
  t.test('you should publish it', t => {
    const er = Object.assign(new Error('404 not found'), {
      pkgid: 'yolo',
      code: 'E404',
    })
    t.matchSnapshot(errorMessage(er, npm))
    t.end()
  })
  t.test('name with warning', t => {
    const er = Object.assign(new Error('404 not found'), {
      pkgid: new Array(215).fill('x').join(''),
      code: 'E404',
    })
    t.matchSnapshot(errorMessage(er, npm))
    t.end()
  })
  t.test('name with error', t => {
    const er = Object.assign(new Error('404 not found'), {
      pkgid: 'node_modules',
      code: 'E404',
    })
    t.matchSnapshot(errorMessage(er, npm))
    t.end()
  })
  t.test('cleans sensitive info from package id', t => {
    const er = Object.assign(new Error('404 not found'), {
      pkgid: 'http://evil:password@npmjs.org/not-found',
      code: 'E404',
    })
    t.matchSnapshot(errorMessage(er, npm))
    t.end()
  })
})

t.test('bad platform', async t => {
  const npm = await loadMockNpm(t)

  t.test('string os/arch', t => {
    const er = Object.assign(new Error('a bad plat'), {
      pkgid: 'lodash@1.0.0',
      current: {
        os: 'posix',
        cpu: 'x64',
      },
      required: {
        os: '!yours',
        cpu: 'x420',
      },
      code: 'EBADPLATFORM',
    })
    t.matchSnapshot(errorMessage(er, npm))
    t.end()
  })
  t.test('array os/arch', t => {
    const er = Object.assign(new Error('a bad plat'), {
      pkgid: 'lodash@1.0.0',
      current: {
        os: 'posix',
        cpu: 'x64',
      },
      required: {
        os: ['!yours', 'mine'],
        cpu: ['x867', 'x5309'],
      },
      code: 'EBADPLATFORM',
    })
    t.matchSnapshot(errorMessage(er, npm))
    t.end()
  })
})

t.test('explain ERESOLVE errors', async t => {
  const npm = await loadMockNpm(t)
  const EXPLAIN_CALLED = []

  const er = Object.assign(new Error('could not resolve'), {
    code: 'ERESOLVE',
  })

  t.matchSnapshot(errorMessage(er, {
    ...npm,
    mocks: {
      '../../../lib/utils/explain-eresolve.js': {
        report: (...args) => {
          EXPLAIN_CALLED.push(args)
          return 'explanation'
        },
      },
    },
  }))
  t.match(EXPLAIN_CALLED, [[
    er,
    false,
    path.resolve(npm.cache, 'eresolve-report.txt'),
  ]])
})
