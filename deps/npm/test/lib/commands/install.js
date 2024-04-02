const t = require('tap')
const { load: loadMockNpm } = require('../../fixtures/mock-npm')

t.test('exec commands', async t => {
  await t.test('with args, dev=true', async t => {
    const SCRIPTS = []
    let ARB_ARGS = null
    let REIFY_CALLED = false
    let ARB_OBJ = null

    const { npm } = await loadMockNpm(t, {
      mocks: {
        '@npmcli/run-script': ({ event }) => {
          SCRIPTS.push(event)
        },
        '@npmcli/arborist': function (args) {
          ARB_ARGS = args
          ARB_OBJ = this
          this.reify = () => {
            REIFY_CALLED = true
          }
        },
        '{LIB}/utils/reify-finish.js': (_, arb) => {
          if (arb !== ARB_OBJ) {
            throw new Error('got wrong object passed to reify-finish')
          }
        },
      },
      config: {
        // This is here because CI calls tests with `--ignore-scripts`, which config
        // picks up from argv
        'ignore-scripts': false,
        'audit-level': 'low',
        dev: true,
      },
    })

    await npm.exec('install', ['fizzbuzz'])

    t.match(
      ARB_ARGS,
      { global: false, path: npm.prefix, auditLevel: null },
      'Arborist gets correct args and ignores auditLevel'
    )
    t.equal(REIFY_CALLED, true, 'called reify')
    t.strictSame(SCRIPTS, [], 'no scripts when adding dep')
  })

  await t.test('without args', async t => {
    const SCRIPTS = []
    let ARB_ARGS = null
    let REIFY_CALLED = false
    let ARB_OBJ = null

    const { npm } = await loadMockNpm(t, {
      mocks: {
        '@npmcli/run-script': ({ event }) => {
          SCRIPTS.push(event)
        },
        '@npmcli/arborist': function (args) {
          ARB_ARGS = args
          ARB_OBJ = this
          this.reify = () => {
            REIFY_CALLED = true
          }
        },
        '{LIB}/utils/reify-finish.js': (_, arb) => {
          if (arb !== ARB_OBJ) {
            throw new Error('got wrong object passed to reify-finish')
          }
        },
      },
      config: {

      },
    })

    await npm.exec('install', [])
    t.match(ARB_ARGS, { global: false, path: npm.prefix })
    t.equal(REIFY_CALLED, true, 'called reify')
    t.strictSame(SCRIPTS, [
      'preinstall',
      'install',
      'postinstall',
      'prepublish',
      'preprepare',
      'prepare',
      'postprepare',
    ], 'exec scripts when doing local build')
  })

  await t.test('should ignore scripts with --ignore-scripts', async t => {
    const SCRIPTS = []
    let REIFY_CALLED = false
    const { npm } = await loadMockNpm(t, {
      mocks: {
        '{LIB}/utils/reify-finish.js': async () => {},
        '@npmcli/run-script': ({ event }) => {
          SCRIPTS.push(event)
        },
        '@npmcli/arborist': function () {
          this.reify = () => {
            REIFY_CALLED = true
          }
        },
      },
      config: {
        'ignore-scripts': true,
      },
    })

    await npm.exec('install', [])
    t.equal(REIFY_CALLED, true, 'called reify')
    t.strictSame(SCRIPTS, [], 'no scripts when adding dep')
  })

  await t.test('should install globally using Arborist', async t => {
    const SCRIPTS = []
    let ARB_ARGS = null
    let REIFY_CALLED
    const { npm } = await loadMockNpm(t, {
      mocks: {
        '@npmcli/run-script': ({ event }) => {
          SCRIPTS.push(event)
        },
        '{LIB}/utils/reify-finish.js': async () => {},
        '@npmcli/arborist': function (args) {
          ARB_ARGS = args
          this.reify = () => {
            REIFY_CALLED = true
          }
        },
      },
      config: {
        global: true,
      },
    })
    await npm.exec('install', [])
    t.match(
      ARB_ARGS,
      { global: true, path: npm.globalPrefix }
    )
    t.equal(REIFY_CALLED, true, 'called reify')
    t.strictSame(SCRIPTS, [], 'no scripts when installing globally')
    t.notOk(npm.config.get('audit', 'cli'))
  })

  await t.test('should not install invalid global package name', async t => {
    const { npm } = await loadMockNpm(t, {
      mocks: {
        '@npmcli/run-script': () => {},
        '{LIB}/utils/reify-finish.js': async () => {},
        '@npmcli/arborist': function (args) {
          throw new Error('should not reify')
        },
      },
      config: {
        global: true,
      },
    })
    await t.rejects(
      npm.exec('install', ['']),
      /Usage:/,
      'should not install invalid package name'
    )
  })

  await t.test('npm i -g npm engines check success', async t => {
    const { npm } = await loadMockNpm(t, {
      mocks: {
        '{LIB}/utils/reify-finish.js': async () => {},
        '@npmcli/arborist': function () {
          this.reify = () => {}
        },
        pacote: {
          manifest: () => {
            return {
              version: '100.100.100',
              engines: {
                node: '>1',
              },
            }
          },
        },
      },
      config: {
        global: true,
      },
    })
    await npm.exec('install', ['npm'])
    t.ok('No exceptions happen')
  })

  await t.test('npm i -g npm engines check failure', async t => {
    const { npm } = await loadMockNpm(t, {
      mocks: {
        pacote: {
          manifest: () => {
            return {
              _id: 'npm@1.2.3',
              version: '100.100.100',
              engines: {
                node: '>1000',
              },
            }
          },
        },
      },
      config: {
        global: true,
      },
    })
    await t.rejects(
      npm.exec('install', ['npm']),
      {
        message: 'Unsupported engine',
        pkgid: 'npm@1.2.3',
        current: {
          node: process.version,
          npm: '100.100.100',
        },
        required: {
          node: '>1000',
        },
        code: 'EBADENGINE',
      }
    )
  })

  await t.test('npm i -g npm engines check failure forced override', async t => {
    const { npm } = await loadMockNpm(t, {
      mocks: {
        '{LIB}/utils/reify-finish.js': async () => {},
        '@npmcli/arborist': function () {
          this.reify = () => {}
        },
        pacote: {
          manifest: () => {
            return {
              _id: 'npm@1.2.3',
              version: '100.100.100',
              engines: {
                node: '>1000',
              },
            }
          },
        },
      },
      config: {
        global: true,
        force: true,
      },
    })
    await npm.exec('install', ['npm'])
    t.ok('Does not throw')
  })

  await t.test('npm i -g npm@version engines check failure', async t => {
    const { npm } = await loadMockNpm(t, {
      mocks: {
        '{LIB}/utils/reify-finish.js': async () => {},
        '@npmcli/arborist': function () {
          this.reify = () => {}
        },
        pacote: {
          manifest: () => {
            return {
              _id: 'npm@1.2.3',
              version: '100.100.100',
              engines: {
                node: '>1000',
              },
            }
          },
        },
      },
      config: {
        global: true,
      },
    })
    await t.rejects(
      npm.exec('install', ['npm@100']),
      {
        message: 'Unsupported engine',
        pkgid: 'npm@1.2.3',
        current: {
          node: process.version,
          npm: '100.100.100',
        },
        required: {
          node: '>1000',
        },
        code: 'EBADENGINE',
      }
    )
  })
})

t.test('completion', async t => {
  const mockComp = async (t, { noChdir } = {}) => loadMockNpm(t, {
    command: 'install',
    prefixDir: {
      arborist: {
        'package.json': '{}',
      },
      'arborist.txt': 'just a file',
      'other-dir': { a: 'a' },
    },
    ...(noChdir ? { chdir: false } : {}),
  })

  await t.test('completion to folder - has a match', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: './ar' })
    t.strictSame(res, ['arborist'], 'package dir match')
  })

  await t.test('completion to folder - invalid dir', async t => {
    const { install } = await mockComp(t, { noChdir: true })
    const res = await install.completion({ partialWord: '/does/not/exist' })
    t.strictSame(res, [], 'invalid dir: no matching')
  })

  await t.test('completion to folder - no matches', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: './pa' })
    t.strictSame(res, [], 'no name match')
  })

  await t.test('completion to folder - match is not a package', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: './othe' })
    t.strictSame(res, [], 'no name match')
  })

  await t.test('completion to url', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: 'http://path/to/url' })
    t.strictSame(res, [])
  })

  await t.test('no /', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: 'toto' })
    t.notOk(res)
  })

  await t.test('only /', async t => {
    const { install } = await mockComp(t)
    const res = await install.completion({ partialWord: '/' })
    t.strictSame(res, [])
  })
})

t.test('location detection and audit', async (t) => {
  await t.test('audit false without package.json', async t => {
    const { npm } = await loadMockNpm(t, {
      command: 'install',
      prefixDir: {
        // no package.json
        'readme.txt': 'just a file',
        'other-dir': { a: 'a' },
      },
    })
    t.equal(npm.config.get('location'), 'user')
    t.equal(npm.config.get('audit'), false)
  })

  await t.test('audit true with package.json', async t => {
    const { npm } = await loadMockNpm(t, {
      command: 'install',
      prefixDir: {
        'package.json': '{ "name": "testpkg", "version": "1.0.0" }',
        'readme.txt': 'just a file',
      },
    })
    t.equal(npm.config.get('location'), 'user')
    t.equal(npm.config.get('audit'), true)
  })

  await t.test('audit true without package.json when set', async t => {
    const { npm } = await loadMockNpm(t, {
      command: 'install',
      prefixDir: {
        // no package.json
        'readme.txt': 'just a file',
        'other-dir': { a: 'a' },
      },
      config: {
        audit: true,
      },
    })
    t.equal(npm.config.get('location'), 'user')
    t.equal(npm.config.get('audit'), true)
  })

  await t.test('audit true in root config without package.json', async t => {
    const { npm } = await loadMockNpm(t, {
      command: 'install',
      prefixDir: {
        // no package.json
        'readme.txt': 'just a file',
        'other-dir': { a: 'a' },
      },
      // change npmRoot to get it to use a builtin rc file
      otherDirs: { npmrc: 'audit=true' },
      npm: ({ other }) => ({ npmRoot: other }),
    })
    t.equal(npm.config.get('location'), 'user')
    t.equal(npm.config.get('audit'), true)
  })

  await t.test('test for warning when --global & --audit', async t => {
    const { npm, logs } = await loadMockNpm(t, {
      command: 'install',
      prefixDir: {
        // no package.json
        'readme.txt': 'just a file',
        'other-dir': { a: 'a' },
      },
      config: {
        audit: true,
        global: true,
      },
    })
    t.equal(npm.config.get('location'), 'user')
    t.equal(npm.config.get('audit'), true)
    t.equal(logs.warn[0][0], 'config')
    t.equal(logs.warn[0][1], 'includes both --global and --audit, which is currently unsupported.')
  })
})
