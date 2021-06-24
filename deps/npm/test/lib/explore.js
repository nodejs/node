const t = require('tap')

let RPJ_ERROR = null
let RPJ_CALLED = ''
const mockRPJ = async path => {
  if (RPJ_ERROR) {
    try {
      throw RPJ_ERROR
    } finally {
      RPJ_ERROR = null
    }
  }
  RPJ_CALLED = path
  return {some: 'package'}
}

let RUN_SCRIPT_ERROR = null
let RUN_SCRIPT_EXIT_CODE = 0
let RUN_SCRIPT_SIGNAL = null
let RUN_SCRIPT_EXEC = null
const mockRunScript = ({ pkg, banner, path, event, stdio }) => {
  if (event !== '_explore')
    throw new Error('got wrong event name')

  RUN_SCRIPT_EXEC = pkg.scripts._explore

  if (RUN_SCRIPT_ERROR) {
    try {
      return Promise.reject(RUN_SCRIPT_ERROR)
    } finally {
      RUN_SCRIPT_ERROR = null
    }
  }

  if (RUN_SCRIPT_EXIT_CODE || RUN_SCRIPT_SIGNAL) {
    return Promise.reject(Object.assign(new Error('command failed'), {
      code: RUN_SCRIPT_EXIT_CODE,
      signal: RUN_SCRIPT_SIGNAL,
    }))
  }

  return Promise.resolve({ code: 0, signal: null })
}

const output = []
const logs = []
const getExplore = (windows) => {
  const Explore = t.mock('../../lib/explore.js', {
    '../../lib/utils/is-windows.js': windows,
    path: require('path')[windows ? 'win32' : 'posix'],
    'read-package-json-fast': mockRPJ,
    '@npmcli/run-script': mockRunScript,
  })
  const npm = {
    dir: windows ? 'c:\\npm\\dir' : '/npm/dir',
    log: {
      error: (...msg) => logs.push(msg),
      disableProgress: () => {},
      enableProgress: () => {},
    },
    flatOptions: {
      shell: 'shell-command',
    },
    output: out => {
      output.push(out)
    },
  }
  return new Explore(npm)
}

const windowsExplore = getExplore(true)
const posixExplore = getExplore(false)

t.test('basic interactive', t => {
  t.afterEach(() => output.length = 0)

  t.test('windows', t => windowsExplore.exec(['pkg'], er => {
    if (er)
      throw er

    t.strictSame({
      RPJ_CALLED,
      RUN_SCRIPT_EXEC,
    }, {
      RPJ_CALLED: 'c:\\npm\\dir\\pkg\\package.json',
      RUN_SCRIPT_EXEC: 'shell-command',
    })
    t.strictSame(output, [
      "\nExploring c:\\npm\\dir\\pkg\nType 'exit' or ^D when finished\n",
    ])
    t.end()
  }))

  t.test('posix', t => posixExplore.exec(['pkg'], er => {
    if (er)
      throw er

    t.strictSame({
      RPJ_CALLED,
      RUN_SCRIPT_EXEC,
    }, {
      RPJ_CALLED: '/npm/dir/pkg/package.json',
      RUN_SCRIPT_EXEC: 'shell-command',
    })
    t.strictSame(output, [
      "\nExploring /npm/dir/pkg\nType 'exit' or ^D when finished\n",
    ])
    t.end()
  }))

  t.end()
})

t.test('interactive tracks exit code', t => {
  const { exitCode } = process
  t.beforeEach(() => {
    process.exitCode = exitCode
    RUN_SCRIPT_EXIT_CODE = 99
  })
  t.afterEach(() => {
    RUN_SCRIPT_EXIT_CODE = 0
    output.length = 0
    process.exitCode = exitCode
  })

  t.test('windows', t => windowsExplore.exec(['pkg'], er => {
    if (er)
      throw er

    t.strictSame({
      RPJ_CALLED,
      RUN_SCRIPT_EXEC,
    }, {
      RPJ_CALLED: 'c:\\npm\\dir\\pkg\\package.json',
      RUN_SCRIPT_EXEC: 'shell-command',
    })
    t.strictSame(output, [
      "\nExploring c:\\npm\\dir\\pkg\nType 'exit' or ^D when finished\n",
    ])
    t.equal(process.exitCode, 99)
    t.end()
  }))

  t.test('posix', t => posixExplore.exec(['pkg'], er => {
    if (er)
      throw er

    t.strictSame({
      RPJ_CALLED,
      RUN_SCRIPT_EXEC,
    }, {
      RPJ_CALLED: '/npm/dir/pkg/package.json',
      RUN_SCRIPT_EXEC: 'shell-command',
    })
    t.strictSame(output, [
      "\nExploring /npm/dir/pkg\nType 'exit' or ^D when finished\n",
    ])
    t.equal(process.exitCode, 99)
    t.end()
  }))

  t.test('posix spawn fail', t => {
    RUN_SCRIPT_ERROR = Object.assign(new Error('glorb'), {
      code: 33,
    })
    posixExplore.exec(['pkg'], er => {
      t.match(er, { message: 'glorb', code: 33 })
      t.strictSame(output, [
        "\nExploring /npm/dir/pkg\nType 'exit' or ^D when finished\n",
      ])
      t.equal(process.exitCode, 33)
      t.end()
    })
  })

  t.test('posix spawn fail, 0 exit code', t => {
    RUN_SCRIPT_ERROR = Object.assign(new Error('glorb'), {
      code: 0,
    })
    posixExplore.exec(['pkg'], er => {
      t.match(er, { message: 'glorb', code: 0 })
      t.strictSame(output, [
        "\nExploring /npm/dir/pkg\nType 'exit' or ^D when finished\n",
      ])
      t.equal(process.exitCode, 1)
      t.end()
    })
  })

  t.test('posix spawn fail, no exit code', t => {
    RUN_SCRIPT_ERROR = Object.assign(new Error('command failed'), {
      code: 'EPROBLEM',
    })
    posixExplore.exec(['pkg'], er => {
      t.match(er, { message: 'command failed', code: 'EPROBLEM' })
      t.strictSame(output, [
        "\nExploring /npm/dir/pkg\nType 'exit' or ^D when finished\n",
      ])
      t.equal(process.exitCode, 1)
      t.end()
    })
  })

  t.end()
})

t.test('basic non-interactive', t => {
  t.afterEach(() => output.length = 0)

  t.test('windows', t => windowsExplore.exec(['pkg', 'ls'], er => {
    if (er)
      throw er

    t.strictSame({
      RPJ_CALLED,
      RUN_SCRIPT_EXEC,
    }, {
      RPJ_CALLED: 'c:\\npm\\dir\\pkg\\package.json',
      RUN_SCRIPT_EXEC: 'ls',
    })
    t.strictSame(output, [])
    t.end()
  }))

  t.test('posix', t => posixExplore.exec(['pkg', 'ls'], er => {
    if (er)
      throw er

    t.strictSame({
      RPJ_CALLED,
      RUN_SCRIPT_EXEC,
    }, {
      RPJ_CALLED: '/npm/dir/pkg/package.json',
      RUN_SCRIPT_EXEC: 'ls',
    })
    t.strictSame(output, [])
    t.end()
  }))

  t.end()
})

t.test('signal fails non-interactive', t => {
  const { exitCode } = process
  t.afterEach(() => {
    output.length = 0
    logs.length = 0
  })

  t.beforeEach(() => {
    RUN_SCRIPT_SIGNAL = 'SIGPROBLEM'
    RUN_SCRIPT_EXIT_CODE = null
    process.exitCode = exitCode
  })
  t.afterEach(() => process.exitCode = exitCode)

  t.test('windows', t => windowsExplore.exec(['pkg', 'ls'], er => {
    t.match(er, {
      message: 'command failed',
      signal: 'SIGPROBLEM',
    })

    t.strictSame({
      RPJ_CALLED,
      RUN_SCRIPT_EXEC,
    }, {
      RPJ_CALLED: 'c:\\npm\\dir\\pkg\\package.json',
      RUN_SCRIPT_EXEC: 'ls',
    })
    t.strictSame(output, [])
    t.end()
  }))

  t.test('posix', t => posixExplore.exec(['pkg', 'ls'], er => {
    t.match(er, {
      message: 'command failed',
      signal: 'SIGPROBLEM',
    })

    t.strictSame({
      RPJ_CALLED,
      RUN_SCRIPT_EXEC,
    }, {
      RPJ_CALLED: '/npm/dir/pkg/package.json',
      RUN_SCRIPT_EXEC: 'ls',
    })
    t.strictSame(output, [])
    t.end()
  }))

  t.end()
})

t.test('usage if no pkg provided', t => {
  t.teardown(() => {
    output.length = 0
  })
  const noPkg = [
    [],
    ['foo/../..'],
    ['asdf/..'],
    ['.'],
    ['..'],
    ['../..'],
  ]
  t.plan(noPkg.length)
  for (const args of noPkg) {
    t.test(JSON.stringify(args), t => {
      posixExplore.exec(args, er => {
        t.match(er, 'Usage:')
        t.strictSame({
          RPJ_CALLED,
          RUN_SCRIPT_EXEC,
        }, {
          RPJ_CALLED: '/npm/dir/pkg/package.json',
          RUN_SCRIPT_EXEC: 'ls',
        })
        t.end()
      })
    })
  }
})

t.test('pkg not installed', t => {
  RPJ_ERROR = new Error('plurple')

  posixExplore.exec(['pkg', 'ls'], er => {
    t.strictSame({
      RPJ_CALLED,
      RUN_SCRIPT_EXEC,
    }, {
      RPJ_CALLED: '/npm/dir/pkg/package.json',
      RUN_SCRIPT_EXEC: 'ls',
    })
    t.strictSame(output, [])
    t.match(er, { message: 'plurple' })
    t.match(logs, [['explore', `It doesn't look like pkg is installed.`]])
    t.end()
    logs.length = 0
  })
})
