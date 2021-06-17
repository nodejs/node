const t = require('tap')
const path = require('path')

// make a bunch of stuff consistent for snapshots

process.getuid = () => 69
process.getgid = () => 420

Object.defineProperty(process, 'arch', {
  value: 'x64',
  configurable: true,
})

const { resolve } = require('path')
const npm = require('../../../lib/npm.js')
const CACHE = '/some/cache/dir'
npm.config = {
  loaded: false,
  localPrefix: '/some/prefix/dir',
  get: key => {
    if (key === 'cache')
      return CACHE
    else if (key === 'node-version')
      return '99.99.99'
    else if (key === 'global')
      return false
    else
      throw new Error('unexpected config lookup: ' + key)
  },
}

npm.version = '123.69.420-npm'
Object.defineProperty(process, 'version', {
  value: '123.69.420-node',
  configurable: true,
})

const npmlog = require('npmlog')
const verboseLogs = []
npmlog.verbose = (...message) => {
  verboseLogs.push(message)
}

const EXPLAIN_CALLED = []
const mocks = {
  '../../../lib/utils/explain-eresolve.js': {
    report: (...args) => {
      EXPLAIN_CALLED.push(args)
      return 'explanation'
    },
  },
  // XXX ???
  get '../../../lib/utils/is-windows.js' () {
    return process.platform === 'win32'
  },
}
let errorMessage = t.mock('../../../lib/utils/error-message.js', { ...mocks })

const beWindows = () => {
  Object.defineProperty(process, 'platform', {
    value: 'win32',
    configurable: true,
  })
  errorMessage = t.mock('../../../lib/utils/error-message.js', { ...mocks })
}

const bePosix = () => {
  Object.defineProperty(process, 'platform', {
    value: 'posix',
    configurable: true,
  })
  errorMessage = t.mock('../../../lib/utils/error-message.js', { ...mocks })
}

t.test('just simple messages', t => {
  npm.command = 'audit'
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
  ]
  t.plan(codes.length)
  codes.forEach(code => {
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

t.test('replace message/stack sensistive info', t => {
  npm.command = 'audit'
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
  t.end()
})

t.test('bad engine with config loaded', t => {
  npm.config.loaded = true
  t.teardown(() => {
    npm.config.loaded = false
  })
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
  t.end()
})

t.test('enoent without a file', t => {
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
  t.end()
})

t.test('enolock without a command', t => {
  npm.command = null
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
  t.end()
})

t.test('default message', t => {
  t.matchSnapshot(errorMessage(new Error('error object'), npm))
  t.matchSnapshot(errorMessage('error string'), npm)
  t.matchSnapshot(errorMessage(Object.assign(new Error('cmd err'), {
    cmd: 'some command',
    signal: 'SIGYOLO',
    args: ['a', 'r', 'g', 's'],
    stdout: 'stdout',
    stderr: 'stderr',
  }), npm))
  t.end()
})

t.test('eacces/eperm', t => {
  const runTest = (windows, loaded, cachePath, cacheDest) => t => {
    if (windows)
      beWindows()
    else
      bePosix()

    npm.config.loaded = loaded
    const path = `${cachePath ? CACHE : '/not/cache/dir'}/path`
    const dest = `${cacheDest ? CACHE : '/not/cache/dir'}/dest`
    const er = Object.assign(new Error('whoopsie'), {
      code: 'EACCES',
      path,
      dest,
      stack: 'dummy stack trace',
    })
    verboseLogs.length = 0
    t.matchSnapshot(errorMessage(er, npm))
    t.matchSnapshot(verboseLogs)
    t.end()
    verboseLogs.length = 0
  }

  for (const windows of [true, false]) {
    for (const loaded of [true, false]) {
      for (const cachePath of [true, false]) {
        for (const cacheDest of [true, false]) {
          const m = JSON.stringify({windows, loaded, cachePath, cacheDest})
          t.test(m, runTest(windows, loaded, cachePath, cacheDest))
        }
      }
    }
  }
  t.end()
})

t.test('json parse', t => {
  t.test('merge conflict in package.json', t => {
    const dir = t.testdir({
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
    })
    const { prefix } = npm
    const { argv } = process
    t.teardown(() => {
      Object.defineProperty(npm, 'prefix', {
        value: prefix,
        configurable: true,
      })
      process.argv = argv
    })
    Object.defineProperty(npm, 'prefix', { value: dir, configurable: true })
    process.argv = ['arg', 'v']
    t.matchSnapshot(errorMessage(Object.assign(new Error('conflicted'), {
      code: 'EJSONPARSE',
      file: resolve(dir, 'package.json'),
    }), npm))
    t.end()
  })

  t.test('just regular bad json in package.json', t => {
    const dir = t.testdir({
      'package.json': 'not even slightly json',
    })
    const { prefix } = npm
    const { argv } = process
    t.teardown(() => {
      Object.defineProperty(npm, 'prefix', {
        value: prefix,
        configurable: true,
      })
      process.argv = argv
    })
    Object.defineProperty(npm, 'prefix', { value: dir, configurable: true })
    process.argv = ['arg', 'v']
    t.matchSnapshot(errorMessage(Object.assign(new Error('not json'), {
      code: 'EJSONPARSE',
      file: resolve(dir, 'package.json'),
    }), npm))
    t.end()
  })

  t.test('json somewhere else', t => {
    const dir = t.testdir({
      'blerg.json': 'not even slightly json',
    })
    const { argv } = process
    t.teardown(() => {
      process.argv = argv
    })
    process.argv = ['arg', 'v']
    t.matchSnapshot(errorMessage(Object.assign(new Error('not json'), {
      code: 'EJSONPARSE',
      file: `${dir}/blerg.json`,
    }), npm))
    t.end()
  })

  t.end()
})

t.test('eotp/e401', t => {
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

  t.end()
})

t.test('404', t => {
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
  t.end()
})

t.test('bad platform', t => {
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
        cpu: ['x420', 'x69'],
      },
      code: 'EBADPLATFORM',
    })
    t.matchSnapshot(errorMessage(er, npm))
    t.end()
  })

  t.end()
})

t.test('explain ERESOLVE errors', t => {
  const er = Object.assign(new Error('could not resolve'), {
    code: 'ERESOLVE',
  })
  t.matchSnapshot(errorMessage(er, npm))
  t.match(EXPLAIN_CALLED, [[
    er,
    undefined,
    path.resolve(npm.cache, 'eresolve-report.txt'),
  ]])
  t.end()
})
