const t = require('tap')
const fs = require('fs')
const path = require('path')

const completionScript = fs
  .readFileSync(path.resolve(__dirname, '../../../lib/utils/completion.sh'), { encoding: 'utf8' })
  .replace(/^#!.*?\n/, '')

const { real: mockNpm } = require('../../fixtures/mock-npm')

const { Npm, outputs } = mockNpm(t, {
  '../../lib/utils/is-windows-shell.js': false,
})
const npm = new Npm()

t.test('completion', async t => {
  const completion = await npm.cmd('completion')
  t.test('completion completion', async t => {
    const home = process.env.HOME
    t.teardown(() => {
      process.env.HOME = home
    })

    process.env.HOME = t.testdir({
      '.bashrc': '',
      '.zshrc': '',
    })

    await completion.completion({ w: 2 })
    t.matchSnapshot(outputs, 'both shells')
  })

  t.test('completion completion no known shells', async t => {
    const home = process.env.HOME
    t.teardown(() => {
      process.env.HOME = home
    })

    process.env.HOME = t.testdir()

    await completion.completion({ w: 2 })
    t.matchSnapshot(outputs, 'no responses')
  })

  t.test('completion completion wrong word count', async t => {
    await completion.completion({ w: 3 })
    t.matchSnapshot(outputs, 'no responses')
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

  t.test('single command name', async t => {
    process.env.COMP_CWORD = 1
    process.env.COMP_LINE = 'npm conf'
    process.env.COMP_POINT = process.env.COMP_LINE.length

    t.teardown(() => {
      delete process.env.COMP_CWORD
      delete process.env.COMP_LINE
      delete process.env.COMP_POINT
    })

    await completion.exec(['npm', 'conf'])
    t.matchSnapshot(outputs, 'single command name')
  })

  t.test('multiple command names', async t => {
    process.env.COMP_CWORD = 1
    process.env.COMP_LINE = 'npm a'
    process.env.COMP_POINT = process.env.COMP_LINE.length

    t.teardown(() => {
      delete process.env.COMP_CWORD
      delete process.env.COMP_LINE
      delete process.env.COMP_POINT
    })

    await completion.exec(['npm', 'a'])
    t.matchSnapshot(outputs, 'multiple command names')
  })

  t.test('completion of invalid command name does nothing', async t => {
    process.env.COMP_CWORD = 1
    process.env.COMP_LINE = 'npm compute'
    process.env.COMP_POINT = process.env.COMP_LINE.length

    t.teardown(() => {
      delete process.env.COMP_CWORD
      delete process.env.COMP_LINE
      delete process.env.COMP_POINT
    })

    await completion.exec(['npm', 'compute'])
    t.matchSnapshot(outputs, 'no results')
  })

  t.test('subcommand completion', async t => {
    process.env.COMP_CWORD = 2
    process.env.COMP_LINE = 'npm access '
    process.env.COMP_POINT = process.env.COMP_LINE.length

    t.teardown(() => {
      delete process.env.COMP_CWORD
      delete process.env.COMP_LINE
      delete process.env.COMP_POINT
    })

    await completion.exec(['npm', 'access', ''])
    t.matchSnapshot(outputs, 'subcommands')
  })

  t.test('filtered subcommands', async t => {
    process.env.COMP_CWORD = 2
    process.env.COMP_LINE = 'npm access p'
    process.env.COMP_POINT = process.env.COMP_LINE.length

    t.teardown(() => {
      delete process.env.COMP_CWORD
      delete process.env.COMP_LINE
      delete process.env.COMP_POINT
    })

    await completion.exec(['npm', 'access', 'p'])
    t.matchSnapshot(outputs, 'filtered subcommands')
  })

  t.test('commands with no completion', async t => {
    process.env.COMP_CWORD = 2
    process.env.COMP_LINE = 'npm adduser '
    process.env.COMP_POINT = process.env.COMP_LINE.length

    t.teardown(() => {
      delete process.env.COMP_CWORD
      delete process.env.COMP_LINE
      delete process.env.COMP_POINT
    })

    // quotes around adduser are to ensure coverage when unescaping commands
    await completion.exec(['npm', "'adduser'", ''])
    t.matchSnapshot(outputs, 'no results')
  })

  t.test('flags', async t => {
    process.env.COMP_CWORD = 2
    process.env.COMP_LINE = 'npm install --v'
    process.env.COMP_POINT = process.env.COMP_LINE.length

    t.teardown(() => {
      delete process.env.COMP_CWORD
      delete process.env.COMP_LINE
      delete process.env.COMP_POINT
    })

    await completion.exec(['npm', 'install', '--v'])

    t.matchSnapshot(outputs, 'flags')
  })

  t.test('--no- flags', async t => {
    process.env.COMP_CWORD = 2
    process.env.COMP_LINE = 'npm install --no-v'
    process.env.COMP_POINT = process.env.COMP_LINE.length

    t.teardown(() => {
      delete process.env.COMP_CWORD
      delete process.env.COMP_LINE
      delete process.env.COMP_POINT
    })

    await completion.exec(['npm', 'install', '--no-v'])

    t.matchSnapshot(outputs, 'flags')
  })

  t.test('double dashes escape from flag completion', async t => {
    process.env.COMP_CWORD = 2
    process.env.COMP_LINE = 'npm -- install --'
    process.env.COMP_POINT = process.env.COMP_LINE.length

    t.teardown(() => {
      delete process.env.COMP_CWORD
      delete process.env.COMP_LINE
      delete process.env.COMP_POINT
    })

    await completion.exec(['npm', '--', 'install', '--'])

    t.matchSnapshot(outputs, 'full command list')
  })

  t.test('completion cannot complete options that take a value in mid-command', async t => {
    process.env.COMP_CWORD = 2
    process.env.COMP_LINE = 'npm --registry install'
    process.env.COMP_POINT = process.env.COMP_LINE.length

    t.teardown(() => {
      delete process.env.COMP_CWORD
      delete process.env.COMP_LINE
      delete process.env.COMP_POINT
    })

    await completion.exec(['npm', '--registry', 'install'])
    t.matchSnapshot(outputs, 'does not try to complete option arguments in the middle of a command')
  })
})

t.test('windows without bash', async t => {
  const { Npm, outputs } = mockNpm(t, {
    '../../lib/utils/is-windows-shell.js': true,
  })
  const npm = new Npm()
  const completion = await npm.cmd('completion')
  await t.rejects(
    completion.exec({}),
    { code: 'ENOTSUP', message: /completion supported only in MINGW/ },
    'returns the correct error'
  )
  t.matchSnapshot(outputs, 'no output')
})
