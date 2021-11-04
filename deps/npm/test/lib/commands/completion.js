const t = require('tap')
const fs = require('fs')
const path = require('path')

const completionScript = fs.readFileSync(path.resolve(__dirname, '../../../lib/utils/completion.sh'), { encoding: 'utf8' }).replace(/^#!.*?\n/, '')

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
  cmd: (cmd) => {
    return ({
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
    }[cmd])
  },
  output: (line) => {
    output.push(line)
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

// only include a subset so that the snapshots aren't huge and
// don't change when we add/remove config definitions.
const definitions = require('../../../lib/utils/config/definitions.js')
const config = {
  definitions: {
    global: definitions.global,
    browser: definitions.browser,
    registry: definitions.registry,
  },
  shorthands: {
    reg: ['--registry'],
  },
}

const deref = (cmd) => {
  return cmd
}

const Completion = t.mock('../../../lib/commands/completion.js', {
  '../../../lib/utils/cmd-list.js': cmdList,
  '../../../lib/utils/config/index.js': config,
  '../../../lib/utils/deref-command.js': deref,
  '../../../lib/utils/is-windows-shell.js': false,
})
const completion = new Completion(npm)

t.test('completion completion', async t => {
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

t.test('completion completion no known shells', async t => {
  const home = process.env.HOME
  t.teardown(() => {
    process.env.HOME = home
  })

  process.env.HOME = t.testdir()

  const res = await completion.completion({ w: 2 })
  t.strictSame(res, [], 'no responses')
  t.end()
})

t.test('completion completion wrong word count', async t => {
  const res = await completion.completion({ w: 3 })
  t.strictSame(res, undefined, 'no responses')
  t.end()
})

t.test('completion errors in windows without bash', async t => {
  const Compl = t.mock('../../../lib/commands/completion.js', {
    '../../../lib/utils/is-windows-shell.js': true,
  })

  const compl = new Compl()

  await t.rejects(
    compl.exec({}),
    { code: 'ENOTSUP',
      message: /completion supported only in MINGW/,

    }, 'returns the correct error')
})

t.test('dump script when completion is not being attempted', async t => {
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

  await completion.exec({})

  t.equal(data, completionScript, 'wrote the completion script')
})

t.test('dump script exits correctly when EPIPE is emitted on stdout', async t => {
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

  await completion.exec({})
  t.equal(data, completionScript, 'wrote the completion script')
})

// This test was only working by coincidence before, when we switch to full
// async/await the race condition now makes it impossible to test.  The root of
// the problem is that if we override stdout.write then other things interfere
// during testing.
// t.test('non EPIPE errors cause failures', async t => {
//   const _write = process.stdout.write
//   const _on = process.stdout.on
//   t.teardown(() => {
//     process.stdout.write = _write
//     process.stdout.on = _on
//   })

//   let errorHandler
//   process.stdout.on = (event, handler) => {
//     errorHandler = handler
//     process.stdout.on = _on
//   }

//   let data
//   process.stdout.write = (chunk, callback) => {
//     data = chunk
//     process.stdout.write = _write
//     process.nextTick(() => {
//       errorHandler({ errno: 'ESOMETHINGELSE' })
//       callback()
//     })
//   }

//   await t.rejects(
//     completion.exec([]),
//     { errno: 'ESOMETHINGELSE' },
//     'propagated error'
//   )
//   t.equal(data, completionScript, 'wrote the completion script')
// })

t.test('completion completes single command name', async t => {
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

  await completion.exec(['npm', 'c'])
  t.strictSame(output, ['completion'], 'correctly completed a command name')
})

t.test('completion completes command names', async t => {
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

  await completion.exec(['npm', 'a'])
  t.strictSame(output, [['access', 'adduser'].join('\n')], 'correctly completed a command name')
})

t.test('completion of invalid command name does nothing', async t => {
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

  await completion.exec(['npm', 'compute'])
  t.strictSame(output, [], 'returns no results')
})

t.test('handles async completion function', async t => {
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

  await completion.exec(['npm', 'promise', ''])

  t.strictSame(npmConfig, {
    argv: {
      remain: ['npm', 'promise'],
      cooked: ['npm', 'promise'],
      original: ['npm', 'promise'],
    },
  }, 'applies command config appropriately')
  t.strictSame(output, ['resolved_completion_promise'], 'resolves async completion results')
})

t.test('completion triggers command completions', async t => {
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

  await completion.exec(['npm', 'access', ''])

  t.strictSame(npmConfig, {
    argv: {
      remain: ['npm', 'access'],
      cooked: ['npm', 'access'],
      original: ['npm', 'access'],
    },
  }, 'applies command config appropriately')
  t.strictSame(output, [['public', 'restricted'].join('\n')], 'correctly completed a subcommand name')
})

t.test('completion triggers filtered command completions', async t => {
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

  await completion.exec(['npm', 'access', 'p'])

  t.strictSame(npmConfig, {
    argv: {
      remain: ['npm', 'access'],
      cooked: ['npm', 'access'],
      original: ['npm', 'access'],
    },
  }, 'applies command config appropriately')
  t.strictSame(output, ['public'], 'correctly completed a subcommand name')
})

t.test('completions for commands that return nested arrays are joined', async t => {
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

  await completion.exec(['npm', 'completion', ''])

  t.strictSame(npmConfig, {
    argv: {
      remain: ['npm', 'completion'],
      cooked: ['npm', 'completion'],
      original: ['npm', 'completion'],
    },
  }, 'applies command config appropriately')
  t.strictSame(output, ['>> ~/.bashrc'], 'joins nested arrays')
})

t.test('completions for commands that return nothing work correctly', async t => {
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

  await completion.exec(['npm', 'donothing', ''])

  t.strictSame(npmConfig, {
    argv: {
      remain: ['npm', 'donothing'],
      cooked: ['npm', 'donothing'],
      original: ['npm', 'donothing'],
    },
  }, 'applies command config appropriately')
  t.strictSame(output, [], 'returns nothing')
})

t.test('completions for commands that return a single item work correctly', async t => {
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

  await completion.exec(['npm', 'driveaboat', ''])
  t.strictSame(npmConfig, {
    argv: {
      remain: ['npm', 'driveaboat'],
      cooked: ['npm', 'driveaboat'],
      original: ['npm', 'driveaboat'],
    },
  }, 'applies command config appropriately')
  t.strictSame(output, ['\' fast\''], 'returns the correctly escaped string')
})

t.test('command completion for commands with no completion return no results', async t => {
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
  await completion.exec(['npm', '\'adduser\'', ''])
  t.strictSame(npmConfig, {
    argv: {
      remain: ['npm', 'adduser'],
      cooked: ['npm', 'adduser'],
      original: ['npm', 'adduser'],
    },
  }, 'applies command config appropriately')
  t.strictSame(output, [], 'correctly completed a subcommand name')
})

t.test('command completion errors propagate', async t => {
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

  await t.rejects(
    completion.exec(['npm', 'access', '']),
    /access completion failed/,
    'catches the appropriate error'
  )
  t.strictSame(npmConfig, {
    argv: {
      remain: ['npm', 'access'],
      cooked: ['npm', 'access'],
      original: ['npm', 'access'],
    },
  }, 'applies command config appropriately')
  t.strictSame(output, [], 'returns no results')
})

t.test('completion can complete flags', async t => {
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

  await completion.exec(['npm', 'install', '--'])

  t.strictSame(npmConfig, {}, 'does not apply command config')
  t.strictSame(output, [['--global', '--browser', '--registry', '--reg', '--no-global', '--no-browser'].join('\n')], 'correctly completes flag names')
})

t.test('double dashes escape from flag completion', async t => {
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

  await completion.exec(['npm', '--', 'install', '--'])

  t.strictSame(npmConfig, {}, 'does not apply command config')
  t.strictSame(output, [['access', 'adduser', 'completion', 'login'].join('\n')], 'correctly completes flag names')
})

t.test('completion cannot complete options that take a value in mid-command', async t => {
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

  await completion.exec(['npm', '--registry', 'install'])
  t.strictSame(npmConfig, {}, 'does not apply command config')
  t.strictSame(output, [], 'does not try to complete option arguments in the middle of a command')
})
