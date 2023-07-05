const t = require('tap')
const mockNpm = require('../../fixtures/mock-npm')
const { cleanCwd } = require('../../fixtures/clean-snapshot')

const mockExplore = async (t, exec, {
  PJ_ERROR = null,
  RUN_SCRIPT_ERROR = null,
  RUN_SCRIPT_EXIT_CODE = 0,
  RUN_SCRIPT_SIGNAL = null,
} = {}) => {
  let PJ_CALLED = ''
  const mockPJ = {
    normalize: async path => {
      if (PJ_ERROR) {
        throw PJ_ERROR
      }
      PJ_CALLED = cleanCwd(path)
      return { content: { some: 'package' } }
    },
  }

  let RUN_SCRIPT_EXEC = null
  const mockRunScript = ({ pkg, event }) => {
    if (event !== '_explore') {
      throw new Error('got wrong event name')
    }

    RUN_SCRIPT_EXEC = pkg.scripts._explore

    if (RUN_SCRIPT_ERROR) {
      return Promise.reject(RUN_SCRIPT_ERROR)
    }

    if (RUN_SCRIPT_EXIT_CODE || RUN_SCRIPT_SIGNAL) {
      return Promise.reject(Object.assign(new Error('command failed'), {
        code: RUN_SCRIPT_EXIT_CODE,
        signal: RUN_SCRIPT_SIGNAL,
      }))
    }

    return Promise.resolve({ code: 0, signal: null })
  }

  const mock = await mockNpm(t, {
    mocks: {
      '@npmcli/package-json': mockPJ,
      '@npmcli/run-script': mockRunScript,
    },
    config: {
      shell: 'shell-command',
    },
  })

  await mock.npm.exec('explore', exec)

  return {
    ...mock,
    PJ_CALLED,
    RUN_SCRIPT_EXEC,
    output: cleanCwd(mock.joinedOutput()).trim(),
  }
}

t.test('basic interactive', async t => {
  const {
    output,
    PJ_CALLED,
    RUN_SCRIPT_EXEC,
  } = await mockExplore(t, ['pkg'])

  t.ok(PJ_CALLED.endsWith('/pkg'))
  t.strictSame(RUN_SCRIPT_EXEC, 'shell-command')
  t.match(output, /Exploring \{CWD\}\/[\w-_/]+\nType 'exit' or \^D when finished/)
})

t.test('interactive tracks exit code', async t => {
  t.test('code', async t => {
    const {
      output,
      PJ_CALLED,
      RUN_SCRIPT_EXEC,
    } = await mockExplore(t, ['pkg'], { RUN_SCRIPT_EXIT_CODE: 99 })

    t.ok(PJ_CALLED.endsWith('/pkg'))
    t.strictSame(RUN_SCRIPT_EXEC, 'shell-command')
    t.match(output, /Exploring \{CWD\}\/[\w-_/]+\nType 'exit' or \^D when finished/)

    t.equal(process.exitCode, 99)
  })

  t.test('spawn fail', async t => {
    const RUN_SCRIPT_ERROR = Object.assign(new Error('glorb'), {
      code: 33,
    })
    await t.rejects(
      mockExplore(t, ['pkg'], { RUN_SCRIPT_ERROR }),
      { message: 'glorb', code: 33 }
    )
    t.equal(process.exitCode, 33)
  })

  t.test('spawn fail, 0 exit code', async t => {
    const RUN_SCRIPT_ERROR = Object.assign(new Error('glorb'), {
      code: 0,
    })
    await t.rejects(
      mockExplore(t, ['pkg'], { RUN_SCRIPT_ERROR }),
      { message: 'glorb', code: 0 }
    )
    t.equal(process.exitCode, 1)
  })

  t.test('spawn fail, no exit code', async t => {
    const RUN_SCRIPT_ERROR = Object.assign(new Error('command failed'), {
      code: 'EPROBLEM',
    })
    await t.rejects(
      mockExplore(t, ['pkg'], { RUN_SCRIPT_ERROR }),
      { message: 'command failed', code: 'EPROBLEM' }
    )
    t.equal(process.exitCode, 1)
  })
})

t.test('basic non-interactive', async t => {
  const {
    output,
    PJ_CALLED,
    RUN_SCRIPT_EXEC,
  } = await mockExplore(t, ['pkg', 'ls'])

  t.ok(PJ_CALLED.endsWith('/pkg'))
  t.strictSame(RUN_SCRIPT_EXEC, 'ls')

  t.strictSame(output, '')
})

t.test('signal fails non-interactive', async t => {
  await t.rejects(
    mockExplore(t, ['pkg', 'ls'], { RUN_SCRIPT_SIGNAL: 'SIGPROBLEM' }),
    {
      message: 'command failed',
      signal: 'SIGPROBLEM',
    }
  )
})

t.test('usage if no pkg provided', async t => {
  const noPkg = [
    [],
    ['foo/../..'],
    ['asdf/..'],
    ['.'],
    ['..'],
    ['../..'],
  ]

  for (const args of noPkg) {
    t.test(JSON.stringify(args), async t => {
      await t.rejects(
        mockExplore(t, args),
        'Usage:'
      )
    })
  }
})

t.test('pkg not installed', async t => {
  const PJ_ERROR = new Error('plurple')

  await t.rejects(
    mockExplore(t, ['pkg', 'ls'], { PJ_ERROR }),
    { message: 'plurple' }
  )
})
