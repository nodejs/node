const { test } = require('tap')
const requireInject = require('require-inject')
const fs = require('fs')
const path = require('path')

const completionScript = fs.readFileSync(path.resolve(__dirname, '../../lib/utils/completion.sh'), { encoding: 'utf8' }).replace(/^#!.*?\n/, '')

const output = []
const npmConfig = {}
let accessCompletionError = false

const npm = {
  config: {
    set: (key, value) => {
      npmConfig[key] = value
    },
    clear: () => {
      for (const key in npmConfig)
        delete npmConfig[key]
    },
  },
  commands: {
    completion: {
      completion: () => [['>>', '~/.bashrc']],
    },
    adduser: {},
    access: {
      completion: () => {
        if (accessCompletionError)
          throw new Error('access completion failed')

        return ['public', 'restricted']
      },
    },
    promise: {
      completion: () => Promise.resolve(['resolved_completion_promise']),
    },
    donothing: {
      completion: () => {
        return null
      },
    },
    driveaboat: {
      completion: () => {
        return ' fast'
      },
    },
  },
}

const cmdList = {
  aliases: {
    login: 'adduser',
  },
  cmdList: [
    'access',
    'adduser',
    'completion',
  ],
  plumbing: [],
}

const config = {
  types: {
    global: Boolean,
    browser: [null, Boolean, String],
    registry: [null, String],
  },
  shorthands: {
    reg: ['--registry'],
  },
}

const deref = (cmd) => {
  return cmd
}

const Completion = requireInject('../../lib/completion.js', {
  '../../lib/utils/cmd-list.js': cmdList,
  '../../lib/utils/config.js': config,
  '../../lib/utils/deref-command.js': deref,
  '../../lib/utils/is-windows-shell.js': false,
  '../../lib/utils/output.js': (line) => {
    output.push(line)
  },
})
const completion = new Completion(npm)

test('completion completion', async t => {
  const home = process.env.HOME
  t.teardown(() => {
    process.env.HOME = home
  })

  process.env.HOME = t.testdir({
    '.bashrc': '',
    '.zshrc': '',
  })

  const res = await completion.completion({ w: 2 })
  t.strictSame(res, [
    ['>>', '~/.zshrc'],
    ['>>', '~/.bashrc'],
  ], 'identifies both shells')
  t.end()
})

test('completion completion no known shells', async t => {
  const home = process.env.HOME
  t.teardown(() => {
    process.env.HOME = home
  })

  process.env.HOME = t.testdir()

  const res = await completion.completion({ w: 2 })
  t.strictSame(res, [], 'no responses')
  t.end()
})

test('completion completion wrong word count', async t => {
  const res = await completion.completion({ w: 3 })
  t.strictSame(res, undefined, 'no responses')
  t.end()
})

test('completion errors in windows without bash', t => {
  const Compl = requireInject('../../lib/completion.js', {
    '../../lib/utils/is-windows-shell.js': true,
  })

  const compl = new Compl()

  compl.exec({}, (err) => {
    t.match(err, {
      code: 'ENOTSUP',
      message: /completion supported only in MINGW/,
    }, 'returns the correct error')
    t.end()
  })
})

test('dump script when completion is not being attempted', t => {
  const _write = process.stdout.write
  const _on = process.stdout.on
  t.teardown(() => {
    process.stdout.write = _write
    process.stdout.on = _on
  })

  let errorHandler
  process.stdout.on = (event, handler) => {
    errorHandler = handler
    process.stdout.on = _on
  }

  let data
  process.stdout.write = (chunk, callback) => {
    data = chunk
    process.stdout.write = _write
    process.nextTick(() => {
      callback()
      errorHandler({ errno: 'EPIPE' })
    })
  }

  completion.exec({}, (err) => {
    if (err)
      throw err

    t.equal(data, completionScript, 'wrote the completion script')
    t.end()
  })
})

test('dump script exits correctly when EPIPE is emitted on stdout', t => {
  const _write = process.stdout.write
  const _on = process.stdout.on
  t.teardown(() => {
    process.stdout.write = _write
    process.stdout.on = _on
  })

  let errorHandler
  process.stdout.on = (event, handler) => {
    errorHandler = handler
    process.stdout.on = _on
  }

  let data
  process.stdout.write = (chunk, callback) => {
    data = chunk
    process.stdout.write = _write
    process.nextTick(() => {
      errorHandler({ errno: 'EPIPE' })
      callback()
    })
  }

  completion.exec({}, (err) => {
    if (err)
      throw err

    t.equal(data, completionScript, 'wrote the completion script')
    t.end()
  })
})

test('non EPIPE errors cause failures', t => {
  const _write = process.stdout.write
  const _on = process.stdout.on
  t.teardown(() => {
    process.stdout.write = _write
    process.stdout.on = _on
  })

  let errorHandler
  process.stdout.on = (event, handler) => {
    errorHandler = handler
    process.stdout.on = _on
  }

  let data
  process.stdout.write = (chunk, callback) => {
    data = chunk
    process.stdout.write = _write
    process.nextTick(() => {
      errorHandler({ errno: 'ESOMETHINGELSE' })
      callback()
    })
  }

  completion.exec({}, (err) => {
    t.equal(err.errno, 'ESOMETHINGELSE', 'propagated error')
    t.equal(data, completionScript, 'wrote the completion script')
    t.end()
  })
})

test('completion completes single command name', t => {
  process.env.COMP_CWORD = 1
  process.env.COMP_LINE = 'npm c'
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  completion.exec(['npm', 'c'], (err, res) => {
    if (err)
      throw err

    t.strictSame(output, ['completion'], 'correctly completed a command name')
    t.end()
  })
})

test('completion completes command names', t => {
  process.env.COMP_CWORD = 1
  process.env.COMP_LINE = 'npm a'
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  completion.exec(['npm', 'a'], (err, res) => {
    if (err)
      throw err

    t.strictSame(output, [['access', 'adduser'].join('\n')], 'correctly completed a command name')
    t.end()
  })
})

test('completion of invalid command name does nothing', t => {
  process.env.COMP_CWORD = 1
  process.env.COMP_LINE = 'npm compute'
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  completion.exec(['npm', 'compute'], (err, res) => {
    if (err)
      throw err

    t.strictSame(output, [], 'returns no results')
    t.end()
  })
})

test('handles async completion function', t => {
  process.env.COMP_CWORD = 2
  process.env.COMP_LINE = 'npm promise'
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  completion.exec(['npm', 'promise', ''], (err, res) => {
    if (err)
      throw err

    t.strictSame(npmConfig, {
      argv: {
        remain: ['npm', 'promise'],
        cooked: ['npm', 'promise'],
        original: ['npm', 'promise'],
      },
    }, 'applies command config appropriately')
    t.strictSame(output, ['resolved_completion_promise'], 'resolves async completion results')
    t.end()
  })
})

test('completion triggers command completions', t => {
  process.env.COMP_CWORD = 2
  process.env.COMP_LINE = 'npm access '
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  completion.exec(['npm', 'access', ''], (err, res) => {
    if (err)
      throw err

    t.strictSame(npmConfig, {
      argv: {
        remain: ['npm', 'access'],
        cooked: ['npm', 'access'],
        original: ['npm', 'access'],
      },
    }, 'applies command config appropriately')
    t.strictSame(output, [['public', 'restricted'].join('\n')], 'correctly completed a subcommand name')
    t.end()
  })
})

test('completion triggers filtered command completions', t => {
  process.env.COMP_CWORD = 2
  process.env.COMP_LINE = 'npm access p'
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  completion.exec(['npm', 'access', 'p'], (err, res) => {
    if (err)
      throw err

    t.strictSame(npmConfig, {
      argv: {
        remain: ['npm', 'access'],
        cooked: ['npm', 'access'],
        original: ['npm', 'access'],
      },
    }, 'applies command config appropriately')
    t.strictSame(output, ['public'], 'correctly completed a subcommand name')
    t.end()
  })
})

test('completions for commands that return nested arrays are joined', t => {
  process.env.COMP_CWORD = 2
  process.env.COMP_LINE = 'npm completion '
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  completion.exec(['npm', 'completion', ''], (err, res) => {
    if (err)
      throw err

    t.strictSame(npmConfig, {
      argv: {
        remain: ['npm', 'completion'],
        cooked: ['npm', 'completion'],
        original: ['npm', 'completion'],
      },
    }, 'applies command config appropriately')
    t.strictSame(output, ['>> ~/.bashrc'], 'joins nested arrays')
    t.end()
  })
})

test('completions for commands that return nothing work correctly', t => {
  process.env.COMP_CWORD = 2
  process.env.COMP_LINE = 'npm donothing '
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  completion.exec(['npm', 'donothing', ''], (err, res) => {
    if (err)
      throw err

    t.strictSame(npmConfig, {
      argv: {
        remain: ['npm', 'donothing'],
        cooked: ['npm', 'donothing'],
        original: ['npm', 'donothing'],
      },
    }, 'applies command config appropriately')
    t.strictSame(output, [], 'returns nothing')
    t.end()
  })
})

test('completions for commands that return a single item work correctly', t => {
  process.env.COMP_CWORD = 2
  process.env.COMP_LINE = 'npm driveaboat '
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  completion.exec(['npm', 'driveaboat', ''], (err, res) => {
    if (err)
      throw err

    t.strictSame(npmConfig, {
      argv: {
        remain: ['npm', 'driveaboat'],
        cooked: ['npm', 'driveaboat'],
        original: ['npm', 'driveaboat'],
      },
    }, 'applies command config appropriately')
    t.strictSame(output, ['\' fast\''], 'returns the correctly escaped string')
    t.end()
  })
})

test('command completion for commands with no completion return no results', t => {
  process.env.COMP_CWORD = 2
  process.env.COMP_LINE = 'npm adduser '
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  // quotes around adduser are to ensure coverage when unescaping commands
  completion.exec(['npm', '\'adduser\'', ''], (err, res) => {
    if (err)
      throw err

    t.strictSame(npmConfig, {
      argv: {
        remain: ['npm', 'adduser'],
        cooked: ['npm', 'adduser'],
        original: ['npm', 'adduser'],
      },
    }, 'applies command config appropriately')
    t.strictSame(output, [], 'correctly completed a subcommand name')
    t.end()
  })
})

test('command completion errors propagate', t => {
  process.env.COMP_CWORD = 2
  process.env.COMP_LINE = 'npm access '
  process.env.COMP_POINT = process.env.COMP_LINE.length
  accessCompletionError = true

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
    accessCompletionError = false
  })

  completion.exec(['npm', 'access', ''], (err, res) => {
    t.match(err, /access completion failed/, 'catches the appropriate error')
    t.strictSame(npmConfig, {
      argv: {
        remain: ['npm', 'access'],
        cooked: ['npm', 'access'],
        original: ['npm', 'access'],
      },
    }, 'applies command config appropriately')
    t.strictSame(output, [], 'returns no results')
    t.end()
  })
})

test('completion can complete flags', t => {
  process.env.COMP_CWORD = 2
  process.env.COMP_LINE = 'npm install --'
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  completion.exec(['npm', 'install', '--'], (err, res) => {
    if (err)
      throw err

    t.strictSame(npmConfig, {}, 'does not apply command config')
    t.strictSame(output, [['--global', '--browser', '--registry', '--reg', '--no-global', '--no-browser'].join('\n')], 'correctly completes flag names')
    t.end()
  })
})

test('double dashes escape from flag completion', t => {
  process.env.COMP_CWORD = 2
  process.env.COMP_LINE = 'npm -- install --'
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  completion.exec(['npm', '--', 'install', '--'], (err, res) => {
    if (err)
      throw err

    t.strictSame(npmConfig, {}, 'does not apply command config')
    t.strictSame(output, [['access', 'adduser', 'completion', 'login'].join('\n')], 'correctly completes flag names')
    t.end()
  })
})

test('completion cannot complete options that take a value in mid-command', t => {
  process.env.COMP_CWORD = 2
  process.env.COMP_LINE = 'npm --registry install'
  process.env.COMP_POINT = process.env.COMP_LINE.length

  t.teardown(() => {
    delete process.env.COMP_CWORD
    delete process.env.COMP_LINE
    delete process.env.COMP_POINT
    npm.config.clear()
    output.length = 0
  })

  completion.exec(['npm', '--registry', 'install'], (err, res) => {
    if (err)
      throw err

    t.strictSame(npmConfig, {}, 'does not apply command config')
    t.strictSame(output, [], 'does not try to complete option arguments in the middle of a command')
    t.end()
  })
})
