const t = require('tap')
const requireInject = require('require-inject')

let STAT_ERROR = null
let STAT_CALLED = ''
const mockStat = (path, cb) => {
  STAT_CALLED = path
  cb(STAT_ERROR, {})
}

let SPAWN_ERROR = null
let SPAWN_EXIT_CODE = 0
let SPAWN_SHELL_EXEC = null
let SPAWN_SHELL_ARGS = null
const mockSpawn = (sh, shellArgs, opts) => {
  if (sh !== 'shell-command')
    throw new Error('got wrong shell command')

  if (SPAWN_ERROR)
    return Promise.reject(SPAWN_ERROR)

  SPAWN_SHELL_EXEC = sh
  SPAWN_SHELL_ARGS = shellArgs
  return Promise.resolve({ code: SPAWN_EXIT_CODE })
}

const output = []
let ERROR_HANDLER_CALLED = null
const getExplore = windows => requireInject('../../lib/explore.js', {
  '../../lib/utils/is-windows.js': windows,
  '../../lib/utils/escape-arg.js': requireInject('../../lib/utils/escape-arg.js', {
    '../../lib/utils/is-windows.js': windows,
  }),
  path: require('path')[windows ? 'win32' : 'posix'],
  '../../lib/utils/escape-exec-path.js': requireInject('../../lib/utils/escape-arg.js', {
    '../../lib/utils/is-windows.js': windows,
  }),
  '../../lib/utils/error-handler.js': er => {
    ERROR_HANDLER_CALLED = er
  },
  fs: {
    stat: mockStat,
  },
  '../../lib/npm.js': {
    dir: windows ? 'c:\\npm\\dir' : '/npm/dir',
    flatOptions: {
      shell: 'shell-command',
    },
  },
  '@npmcli/promise-spawn': mockSpawn,
  '../../lib/utils/output.js': out => {
    output.push(out)
  },
})

const windowsExplore = getExplore(true)
const posixExplore = getExplore(false)

t.test('basic interactive', t => {
  t.afterEach((cb) => {
    output.length = 0
    cb()
  })

  t.test('windows', t => windowsExplore(['pkg'], er => {
    if (er)
      throw er

    t.strictSame({
      ERROR_HANDLER_CALLED,
      STAT_CALLED,
      SPAWN_SHELL_EXEC,
      SPAWN_SHELL_ARGS,
    }, {
      ERROR_HANDLER_CALLED: null,
      STAT_CALLED: 'c:\\npm\\dir\\pkg',
      SPAWN_SHELL_EXEC: 'shell-command',
      SPAWN_SHELL_ARGS: [],
    })
    t.strictSame(output, [
      "\nExploring c:\\npm\\dir\\pkg\nType 'exit' or ^D when finished\n",
    ])
  }))

  t.test('posix', t => posixExplore(['pkg'], er => {
    if (er)
      throw er

    t.strictSame({
      ERROR_HANDLER_CALLED,
      STAT_CALLED,
      SPAWN_SHELL_EXEC,
      SPAWN_SHELL_ARGS,
    }, {
      ERROR_HANDLER_CALLED: null,
      STAT_CALLED: '/npm/dir/pkg',
      SPAWN_SHELL_EXEC: 'shell-command',
      SPAWN_SHELL_ARGS: [],
    })
    t.strictSame(output, [
      "\nExploring /npm/dir/pkg\nType 'exit' or ^D when finished\n",
    ])
  }))

  t.end()
})

t.test('interactive tracks exit code', t => {
  const { exitCode } = process
  t.beforeEach((cb) => {
    process.exitCode = exitCode
    SPAWN_EXIT_CODE = 99
    cb()
  })
  t.afterEach((cb) => {
    SPAWN_EXIT_CODE = 0
    output.length = 0
    process.exitCode = exitCode
    cb()
  })

  t.test('windows', t => windowsExplore(['pkg'], er => {
    if (er)
      throw er

    t.strictSame({
      ERROR_HANDLER_CALLED,
      STAT_CALLED,
      SPAWN_SHELL_EXEC,
      SPAWN_SHELL_ARGS,
    }, {
      ERROR_HANDLER_CALLED: null,
      STAT_CALLED: 'c:\\npm\\dir\\pkg',
      SPAWN_SHELL_EXEC: 'shell-command',
      SPAWN_SHELL_ARGS: [],
    })
    t.strictSame(output, [
      "\nExploring c:\\npm\\dir\\pkg\nType 'exit' or ^D when finished\n",
    ])
    t.equal(process.exitCode, 99)
  }))

  t.test('posix', t => posixExplore(['pkg'], er => {
    if (er)
      throw er

    t.strictSame({
      ERROR_HANDLER_CALLED,
      STAT_CALLED,
      SPAWN_SHELL_EXEC,
      SPAWN_SHELL_ARGS,
    }, {
      ERROR_HANDLER_CALLED: null,
      STAT_CALLED: '/npm/dir/pkg',
      SPAWN_SHELL_EXEC: 'shell-command',
      SPAWN_SHELL_ARGS: [],
    })
    t.strictSame(output, [
      "\nExploring /npm/dir/pkg\nType 'exit' or ^D when finished\n",
    ])
    t.equal(process.exitCode, 99)
  }))

  t.test('posix spawn fail', t => {
    t.teardown(() => {
      SPAWN_ERROR = null
    })
    SPAWN_ERROR = Object.assign(new Error('glorb'), {
      code: 33,
    })
    return posixExplore(['pkg'], er => {
      if (er)
        throw er

      t.strictSame(output, [
        "\nExploring /npm/dir/pkg\nType 'exit' or ^D when finished\n",
      ])
      t.equal(process.exitCode, 33)
    })
  })

  t.end()
})

t.test('basic non-interactive', t => {
  t.afterEach((cb) => {
    output.length = 0
    cb()
  })

  t.test('windows', t => windowsExplore(['pkg', 'ls'], er => {
    if (er)
      throw er

    t.strictSame({
      ERROR_HANDLER_CALLED,
      STAT_CALLED,
      SPAWN_SHELL_EXEC,
      SPAWN_SHELL_ARGS,
    }, {
      ERROR_HANDLER_CALLED: null,
      STAT_CALLED: 'c:\\npm\\dir\\pkg',
      SPAWN_SHELL_EXEC: 'shell-command',
      SPAWN_SHELL_ARGS: [
        '/d',
        '/s',
        '/c',
        '"ls"',
      ],
    })
    t.strictSame(output, [])
  }))

  t.test('posix', t => posixExplore(['pkg', 'ls'], er => {
    if (er)
      throw er

    t.strictSame({
      ERROR_HANDLER_CALLED,
      STAT_CALLED,
      SPAWN_SHELL_EXEC,
      SPAWN_SHELL_ARGS,
    }, {
      ERROR_HANDLER_CALLED: null,
      STAT_CALLED: '/npm/dir/pkg',
      SPAWN_SHELL_EXEC: 'shell-command',
      SPAWN_SHELL_ARGS: ['-c', 'ls'],
    })
    t.strictSame(output, [])
  }))

  t.end()
})

t.test('usage if no pkg provided', t => {
  t.teardown(() => {
    output.length = 0
    ERROR_HANDLER_CALLED = null
  })
  t.plan(1)
  posixExplore([], er => {
    if (er)
      throw er

    t.strictSame({
      ERROR_HANDLER_CALLED: null,
      STAT_CALLED,
      SPAWN_SHELL_EXEC,
      SPAWN_SHELL_ARGS,
    }, {
      ERROR_HANDLER_CALLED: null,
      STAT_CALLED: '/npm/dir/pkg',
      SPAWN_SHELL_EXEC: 'shell-command',
      SPAWN_SHELL_ARGS: ['-c', 'ls'],
    })
  }).catch(er => t.equal(er, 'npm explore <pkg> [ -- <command>]'))
})

t.test('pkg not installed', t => {
  STAT_ERROR = new Error('plurple')
  t.plan(1)

  posixExplore(['pkg', 'ls'], er => {
    if (er)
      throw er

    t.strictSame({
      ERROR_HANDLER_CALLED,
      STAT_CALLED,
      SPAWN_SHELL_EXEC,
      SPAWN_SHELL_ARGS,
    }, {
      ERROR_HANDLER_CALLED: null,
      STAT_CALLED: '/npm/dir/pkg',
      SPAWN_SHELL_EXEC: 'shell-command',
      SPAWN_SHELL_ARGS: ['-c', 'ls'],
    })
    t.strictSame(output, [])
  }).catch(er => {
    t.match(er, { message: `It doesn't look like pkg is installed.` })
  })
})
