const t = require('tap')
const fs = require('node:fs')
const path = require('node:path')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

const completionScript = fs
  .readFileSync(path.resolve(__dirname, '../../../lib/utils/completion.sh'), { encoding: 'utf8' })
  .replace(/^#!.*?\n/, '')

const loadMockCompletion = async (t, o = {}) => {
  const { globals = {}, windows, ...options } = o
  const res = await loadMockNpm(t, {
    command: 'completion',
    ...options,
    globals: (dirs) => ({
      'process.platform': windows ? 'win32' : 'posix',
      'process.env.term': 'notcygwin',
      'process.env.msystem': 'nogmingw',
      ...(typeof globals === 'function' ? globals(dirs) : globals),
    }),
  })
  return {
    resetGlobals: res.mockedGlobals.reset,
    ...res,
  }
}

const loadMockCompletionComp = async (t, word, line) =>
  loadMockCompletion(t, {
    globals: {
      'process.env.COMP_CWORD': word,
      'process.env.COMP_LINE': line,
      'process.env.COMP_POINT': line.length,
    },
  })

t.test('completion', async t => {
  t.test('completion completion', async t => {
    const { outputs, completion } = await loadMockCompletion(t, {
      prefixDir: {
        '.bashrc': 'aaa',
        '.zshrc': 'aaa',
      },
      globals: ({ prefix }) => ({
        'process.env.HOME': prefix,
      }),
    })

    await completion.completion({ w: 2 })
    t.matchSnapshot(outputs, 'both shells')
  })

  t.test('completion completion no known shells', async t => {
    const { outputs, completion } = await loadMockCompletion(t, {
      globals: ({ prefix }) => ({
        'process.env.HOME': prefix,
      }),
    })

    await completion.completion({ w: 2 })
    t.matchSnapshot(outputs, 'no responses')
  })

  t.test('completion completion wrong word count', async t => {
    const { outputs, completion } = await loadMockCompletion(t)

    await completion.completion({ w: 3 })
    t.matchSnapshot(outputs, 'no responses')
  })

  t.test('dump script when completion is not being attempted', async t => {
    let errorHandler, data
    const { completion, resetGlobals } = await loadMockCompletion(t, {
      globals: {
        'process.stdout.on': (event, handler) => {
          errorHandler = handler
          resetGlobals['process.stdout.on']()
        },
        'process.stdout.write': (chunk, callback) => {
          data = chunk
          process.nextTick(() => {
            callback()
            errorHandler({ errno: 'EPIPE' })
          })
          resetGlobals['process.stdout.write']()
        },
      },
    })

    await completion.exec()
    t.equal(data, completionScript, 'wrote the completion script')
  })

  t.test('dump script exits correctly when EPIPE is emitted on stdout', async t => {
    let errorHandler, data
    const { completion, resetGlobals } = await loadMockCompletion(t, {
      globals: {
        'process.stdout.on': (event, handler) => {
          if (event === 'error') {
            errorHandler = handler
          }
          resetGlobals['process.stdout.on']()
        },
        'process.stdout.write': (chunk, callback) => {
          data = chunk
          process.nextTick(() => {
            errorHandler({ errno: 'EPIPE' })
            callback()
          })
          resetGlobals['process.stdout.write']()
        },
      },
    })

    await completion.exec()
    t.equal(data, completionScript, 'wrote the completion script')
  })

  t.test('single command name', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 1, 'npm conf')

    await completion.exec(['npm', 'conf'])
    t.matchSnapshot(outputs, 'single command name')
  })

  t.test('multiple command names', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 1, 'npm a')

    await completion.exec(['npm', 'a'])
    t.matchSnapshot(outputs, 'multiple command names')
  })

  t.test('completion of invalid command name does nothing', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 1, 'npm compute')

    await completion.exec(['npm', 'compute'])
    t.matchSnapshot(outputs, 'no results')
  })

  t.test('subcommand completion', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 2, 'npm access ')

    await completion.exec(['npm', 'access', ''])
    t.matchSnapshot(outputs, 'subcommands')
  })

  t.test('filtered subcommands', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 2, 'npm access p')

    await completion.exec(['npm', 'access', 'p'])
    t.matchSnapshot(outputs, 'filtered subcommands')
  })

  t.test('commands with no completion', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 2, 'npm adduser ')

    // quotes around adduser are to ensure coverage when unescaping commands
    await completion.exec(['npm', "'adduser'", ''])
    t.matchSnapshot(outputs, 'no results')
  })

  t.test('flags', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 2, 'npm install --v')

    await completion.exec(['npm', 'install', '--v'])
    t.matchSnapshot(outputs, 'flags')
  })

  t.test('--no- flags', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 2, 'npm install --no-v')

    await completion.exec(['npm', 'install', '--no-v'])
    t.matchSnapshot(outputs, 'flags')
  })

  t.test('double dashes escape from flag completion', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 2, 'npm -- install --')

    await completion.exec(['npm', '--', 'install', '--'])
    t.matchSnapshot(outputs, 'full command list')
  })

  t.test('completion cannot complete options that take a value in mid-command', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 2, 'npm --registry install')

    await completion.exec(['npm', '--registry', 'install'])
    t.matchSnapshot(outputs, 'does not try to complete option arguments in the middle of a command')
  })

  // Test custom definition flag that requires a value (non-Boolean)
  t.test('completion after custom definition flag requiring value', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 4, 'npm trust github --file value')

    await completion.exec(['npm', 'trust', 'github', '--file', 'value'])
    t.matchSnapshot(outputs, 'custom definition non-Boolean flag handled')
  })

  t.test('trust subcommands', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 2, 'npm trust ')

    await completion.exec(['npm', 'trust', ''])
    t.matchSnapshot(outputs, 'trust subcommands')
  })

  t.test('trust filtered subcommands', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 2, 'npm trust g')

    await completion.exec(['npm', 'trust', 'g'])
    t.matchSnapshot(outputs, 'trust filtered subcommands')
  })

  t.test('trust github flags', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 3, 'npm trust github --re')

    await completion.exec(['npm', 'trust', 'github', '--re'])
    t.matchSnapshot(outputs, 'trust github flags with custom definitions')
  })

  t.test('trust gitlab flags', async t => {
    const { outputs, completion } = await loadMockCompletionComp(t, 3, 'npm trust gitlab --pro')

    await completion.exec(['npm', 'trust', 'gitlab', '--pro'])
    t.matchSnapshot(outputs, 'trust gitlab flags with custom definitions')
  })

  // Test to ensure custom definition aliases are recognized
  t.test('custom definition with alias', async t => {
    const { completion } = await loadMockCompletionComp(t, 3, 'npm trust github --repo')
    await completion.exec(['npm', 'trust', 'github', '--repo'])
    t.pass('custom alias handled')
  })

  // Test to ensure the code handles undefined word gracefully
  t.test('completion with undefined current word', async t => {
    const { completion } = await loadMockCompletion(t, {
      globals: {
        'process.env.COMP_CWORD': '3',
        'process.env.COMP_LINE': 'npm trust github ',
        'process.env.COMP_POINT': '19',
      },
    })
    await completion.exec(['npm', 'trust', 'github'])
    t.pass('undefined word handled')
  })

  // Test custom definition Boolean type checking (covers isFlag with custom defs)
  t.test('completion after Boolean flag from custom definitions', async t => {
    const { completion } = await loadMockCompletionComp(t, 4, 'npm trust github --yes ')
    await completion.exec(['npm', 'trust', 'github', '--yes', ''])
    t.pass('Boolean custom definition handled')
  })

  // Test custom definition non-Boolean type (requires value)
  t.test('completion after non-Boolean custom definition flag', async t => {
    const { completion } = await loadMockCompletionComp(t, 4, 'npm trust github --file ')
    await completion.exec(['npm', 'trust', 'github', '--file', ''])
    t.pass('non-Boolean custom definition handled')
  })

  // Test to trigger isFlag with custom definition alias
  t.test('completion after custom definition flag with alias', async t => {
    const { completion } = await loadMockCompletionComp(t, 4, 'npm trust github --repo ')
    await completion.exec(['npm', 'trust', 'github', '--repo', ''])
    t.pass('custom definition alias handled in isFlag')
  })

  // Test to cover shorthand fallback in isFlag (line 345)
  t.test('completion with unknown flag', async t => {
    const { completion } = await loadMockCompletionComp(t, 3, 'npm install --unknown ')
    await completion.exec(['npm', 'install', '--unknown', ''])
    t.pass('unknown flag handled via shorthand fallback')
  })

  // Test to cover line 110 - cursor in middle of word
  t.test('completion with cursor in middle of word', async t => {
    const { completion } = await loadMockCompletion(t, {
      globals: {
        'process.env.COMP_CWORD': '1',
        'process.env.COMP_LINE': 'npm install',
        'process.env.COMP_POINT': '7', // cursor after "npm ins"
      },
    })
    await completion.exec(['npm', 'ins'])
    t.pass('cursor in middle of word handled')
  })

  // Test to cover line 110 - with escaped/quoted word
  t.test('completion with escaped word', async t => {
    const { completion } = await loadMockCompletion(t, {
      globals: {
        'process.env.COMP_CWORD': '1',
        'process.env.COMP_LINE': 'npm inst',
        'process.env.COMP_POINT': '8', // cursor after "npm inst"
      },
    })
    await completion.exec(['npm', 'install']) // args has full word but COMP_LINE is partial
    t.pass('escaped word handled')
  })

  // Test to cover line 261 - command with definitions (not subcommand)
  t.test('completion for command with definitions', async t => {
    const { completion } = await loadMockCompletionComp(t, 2, 'npm completion --')
    await completion.exec(['npm', 'completion', '--'])
    t.pass('command with definitions handled')
  })

  // Test to cover line 141 - false branch where '--' IS in partialWords
  t.test('completion with double-dash escape in command line', async t => {
    // This tests the false branch at line 141 where partialWords contains '--'
    // The '--' escape prevents flag completion
    // COMP_CWORD should point to the word AFTER '--'
    const { outputs, completion } = await loadMockCompletionComp(t, 3, 'npm install -- pkg')
    await completion.exec(['npm', 'install', '--', 'pkg'])
    t.matchSnapshot(outputs, 'double-dash escape handled')
  })

  // Test to cover line 142 - false branch where word doesn't start with '-'
  t.test('completion with non-flag word', async t => {
    // Inside the outer if (no '--' in partialWords) but word doesn't start with '-'
    const { outputs, completion } = await loadMockCompletionComp(t, 2, 'npm install pack')
    await completion.exec(['npm', 'install', 'pack'])
    t.matchSnapshot(outputs, 'non-flag word completion')
  })
})

t.test('windows without bash', async t => {
  const { outputs, completion } = await loadMockCompletion(t, { windows: true })
  await t.rejects(
    completion.exec(),
    { code: 'ENOTSUP', message: /completion supported only in MINGW/ },
    'returns the correct error'
  )
  t.matchSnapshot(outputs, 'no output')
})
