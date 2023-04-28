const t = require('tap')
const mockGlobals = require('../../fixtures/mock-globals')

/* eslint-disable no-console */
const originals = {
  platform: process.platform,
  error: console.error,
  stderrOn: process.stderr.on,
  stderrWrite: process.stderr.write,
  shell: process.env.SHELL,
  home: process.env.HOME,
  argv: process.argv,
  env: process.env,
  setInterval,
}

t.test('console', async t => {
  await t.test('mocks', async (t) => {
    const errors = []
    mockGlobals(t, {
      'console.error': (...args) => errors.push(...args),
    })

    console.error(1)
    console.error(2)
    console.error(3)
    t.strictSame(errors, [1, 2, 3], 'i got my errors')
  })

  t.equal(console.error, originals.error)
})
/* eslint-enable no-console */

t.test('platform', async (t) => {
  t.equal(process.platform, originals.platform)

  await t.test('posix', async (t) => {
    mockGlobals(t, { 'process.platform': 'posix' })
    t.equal(process.platform, 'posix')

    await t.test('win32 --> woo', async (t) => {
      mockGlobals(t, { 'process.platform': 'win32' })
      t.equal(process.platform, 'win32')

      mockGlobals(t, { 'process.platform': 'woo' })
      t.equal(process.platform, 'woo')
    })

    t.equal(process.platform, 'posix')
  })

  t.equal(process.platform, originals.platform)
})

t.test('manual reset', async t => {
  let errorHandler, data

  const { reset } = mockGlobals(t, {
    'process.stderr.on': (__, handler) => {
      errorHandler = handler
      reset['process.stderr.on']()
    },
    'process.stderr.write': (chunk, callback) => {
      data = chunk
      process.nextTick(() => {
        errorHandler({ errno: 'EPIPE' })
        callback()
      })
      reset['process.stderr.write']()
    },
  })

  await new Promise((res, rej) => {
    process.stderr.on('error', er => er.errno === 'EPIPE' ? res() : rej(er))
    process.stderr.write('hey', res)
  })

  t.equal(process.stderr.on, originals.stderrOn)
  t.equal(process.stderr.write, originals.stderrWrite)
  t.equal(data, 'hey', 'handles EPIPE errors')
  t.ok(errorHandler)
})

t.test('reset called multiple times', async (t) => {
  await t.test('single reset', async t => {
    const { reset } = mockGlobals(t, { 'process.platform': 'z' })
    t.equal(process.platform, 'z')

    reset['process.platform']()
    t.equal(process.platform, originals.platform)

    reset['process.platform']()
    reset['process.platform']()
    reset['process.platform']()
    t.equal(process.platform, originals.platform)
  })

  t.equal(process.platform, originals.platform)
})

t.test('object mode', async t => {
  await t.test('mocks', async t => {
    const home = t.testdir()

    mockGlobals(t, {
      process: {
        stderr: {
          on: '1',
        },
        env: {
          HOME: home,
        },
      },
    })

    t.equal(process.stderr.on, '1')
    t.equal(process.env.HOME, home)
  })

  t.equal(process.env.HOME, originals.home)
  t.equal(process.stderr.write, originals.stderrWrite)
})

t.test('mixed object/string mode', async t => {
  await t.test('mocks', async t => {
    const home = t.testdir()

    mockGlobals(t, {
      'process.env': {
        HOME: home,
        TEST: '1',
      },
    })

    t.equal(process.env.HOME, home)
    t.equal(process.env.TEST, '1')
  })

  t.equal(process.env.HOME, originals.home)
  t.equal(process.env.TEST, undefined)
})

t.test('conflicting mixed object/string mode', async t => {
  await t.test('same key', async t => {
    t.throws(
      () => mockGlobals(t, {
        process: {
          env: {
            HOME: '1',
            TEST: '1',
            NODE_ENV: '1',
          },
          stderr: {
            write: '1',
          },
        },
        'process.env.HOME': '1',
        'process.stderr.write': '1',
      }),
      /process.env.HOME,process.stderr.write/
    )
  })

  await t.test('partial overwrite with replace', async t => {
    t.throws(
      () => mockGlobals(t, {
        process: {
          env: {
            HOME: '1',
            TEST: '1',
            NODE_ENV: '1',
          },
          stderr: {
            write: '1',
          },
        },
        'process.env.HOME': '1',
        'process.stderr.write': '1',
      }, { replace: true }),
      /process -> process.env.HOME,process.stderr.write/
    )
  })
})

t.test('falsy values', async t => {
  await t.test('undefined deletes', async t => {
    mockGlobals(t, { 'process.platform': undefined })
    t.notOk(Object.prototype.hasOwnProperty.call(process, 'platform'))
    t.equal(process.platform, undefined)
  })

  await t.test('null', async t => {
    mockGlobals(t, { 'process.platform': null })
    t.ok(Object.prototype.hasOwnProperty.call(process, 'platform'))
    t.equal(process.platform, null)
  })

  t.equal(process.platform, originals.platform)
})

t.test('date', async t => {
  await t.test('mocks', async t => {
    mockGlobals(t, {
      'Date.now': () => 100,
      'Date.prototype.toISOString': () => 'DDD',
    })
    t.equal(Date.now(), 100)
    t.equal(new Date().toISOString(), 'DDD')
  })

  t.ok(Date.now() > 100)
  t.ok(new Date().toISOString().includes('T'))
})

t.test('argv', async t => {
  await t.test('argv', async t => {
    mockGlobals(t, { 'process.argv': ['node', 'woo'] })
    t.strictSame(process.argv, ['node', 'woo'])
  })

  t.strictSame(process.argv, originals.argv)
})

t.test('replace', async (t) => {
  await t.test('env', async t => {
    mockGlobals(t, { 'process.env': { HOME: '1' } }, { replace: true })
    t.strictSame(process.env, { HOME: '1' })
    t.equal(Object.keys(process.env).length, 1)
  })

  await t.test('setInterval', async t => {
    mockGlobals(t, { setInterval: 0 }, { replace: true })
    t.strictSame(setInterval, 0)
  })

  t.strictSame(setInterval, originals.setInterval)
  t.strictSame(process.env, originals.env)
})

t.test('dot key', async t => {
  const dotKey = 'this.is.a.single.key'
  mockGlobals(t, {
    [`process.env."${dotKey}"`]: 'value',
  })
  t.strictSame(process.env[dotKey], 'value')
})

t.test('multiple mocks and resets', async (t) => {
  const initial = 'a'
  const platforms = ['b', 'c', 'd', 'e', 'f', 'g']

  await t.test('first in, first out', async t => {
    mockGlobals(t, { 'process.platform': initial })
    t.equal(process.platform, initial)

    await t.test('platforms', async (t) => {
      const resets = platforms.map((platform) => {
        const { reset } = mockGlobals(t, { 'process.platform': platform })
        t.equal(process.platform, platform)
        return reset['process.platform']
      }).reverse()

      ;[...platforms.reverse()].forEach((platform, index) => {
        const reset = resets[index]
        const nextPlatform = index === platforms.length - 1 ? initial : platforms[index + 1]
        t.equal(process.platform, platform)
        reset()
        t.equal(process.platform, nextPlatform, 'first reset')
        reset()
        reset()
        t.equal(process.platform, nextPlatform, 'multiple resets are indempotent')
      })
    })

    t.equal(process.platform, initial)
  })

  await t.test('last in,first out', async t => {
    mockGlobals(t, { 'process.platform': initial })
    t.equal(process.platform, initial)

    await t.test('platforms', async (t) => {
      const resets = platforms.map((platform) => {
        const { reset } = mockGlobals(t, { 'process.platform': platform })
        t.equal(process.platform, platform)
        return reset['process.platform']
      })

      resets.forEach((reset, index) => {
        // Calling a reset out of order removes it from the stack
        // but does not change the descriptor so it should still be the
        // last in descriptor until there are none left
        const lastPlatform = platforms[platforms.length - 1]
        const nextPlatform = index === platforms.length - 1 ? initial : lastPlatform
        t.equal(process.platform, lastPlatform)
        reset()
        t.equal(process.platform, nextPlatform, 'multiple resets are indempotent')
        reset()
        reset()
        t.equal(process.platform, nextPlatform, 'multiple resets are indempotent')
      })
    })

    t.equal(process.platform, initial)
  })

  t.test('reset all', async (t) => {
    const { teardown } = mockGlobals(t, { 'process.platform': initial })

    await t.test('platforms', async (t) => {
      const resets = platforms.map((p) => {
        const { teardown: nestedTeardown, reset } = mockGlobals(t, { 'process.platform': p })
        t.equal(process.platform, p)
        return [
          reset['process.platform'],
          nestedTeardown,
        ]
      })

      resets.forEach(r => r[1]())
      t.equal(process.platform, initial, 'teardown goes to initial value')

      resets.forEach((r) => r[0]())
      t.equal(process.platform, initial, 'calling resets after teardown does nothing')
    })

    t.equal(process.platform, initial)
    teardown()
    t.equal(process.platform, originals.platform)
  })
})
