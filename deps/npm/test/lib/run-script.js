const t = require('tap')
const { resolve } = require('path')
const { fake: mockNpm } = require('../fixtures/mock-npm')

const normalizePath = p => p
  .replace(/\\+/g, '/')
  .replace(/\r\n/g, '\n')

const cleanOutput = (str) => normalizePath(str)
  .replace(normalizePath(process.cwd()), '{CWD}')

const RUN_SCRIPTS = []
const flatOptions = {
  scriptShell: undefined,
}
const config = {
  json: false,
  parseable: false,
  'if-present': false,
}

const npm = mockNpm({
  localPrefix: __dirname,
  flatOptions,
  config,
  commands: {
    help: {
      description: 'test help description',
    },
    test: {
      description: 'test test description',
    },
  },
  output: (...msg) => output.push(msg),
})

const output = []

const npmlog = {
  disableProgress: () => null,
  level: 'warn',
  error: () => null,
}

t.afterEach(() => {
  npm.color = false
  npmlog.level = 'warn'
  npmlog.error = () => null
  output.length = 0
  RUN_SCRIPTS.length = 0
  config['if-present'] = false
  config.json = false
  config.parseable = false
})

const getRS = windows => {
  const RunScript = t.mock('../../lib/run-script.js', {
    '@npmcli/run-script': Object.assign(async opts => {
      RUN_SCRIPTS.push(opts)
    }, {
      isServerPackage: require('@npmcli/run-script').isServerPackage,
    }),
    npmlog,
    '../../lib/utils/is-windows-shell.js': windows,
  })
  return new RunScript(npm)
}

const runScript = getRS(false)
const runScriptWin = getRS(true)

const { writeFileSync } = require('fs')
t.test('completion', t => {
  const dir = t.testdir()
  npm.localPrefix = dir
  t.test('already have a script name', async t => {
    const res = await runScript.completion({conf: {argv: {remain: ['npm', 'run', 'x']}}})
    t.equal(res, undefined)
    t.end()
  })
  t.test('no package.json', async t => {
    const res = await runScript.completion({conf: {argv: {remain: ['npm', 'run']}}})
    t.strictSame(res, [])
    t.end()
  })
  t.test('has package.json, no scripts', async t => {
    writeFileSync(`${dir}/package.json`, JSON.stringify({}))
    const res = await runScript.completion({conf: {argv: {remain: ['npm', 'run']}}})
    t.strictSame(res, [])
    t.end()
  })
  t.test('has package.json, with scripts', async t => {
    writeFileSync(`${dir}/package.json`, JSON.stringify({
      scripts: { hello: 'echo hello', world: 'echo world' },
    }))
    const res = await runScript.completion({conf: {argv: {remain: ['npm', 'run']}}})
    t.strictSame(res, ['hello', 'world'])
    t.end()
  })
  t.end()
})

t.test('fail if no package.json', t => {
  t.plan(2)
  npm.localPrefix = t.testdir()
  runScript.exec([], er => t.match(er, { code: 'ENOENT' }))
  runScript.exec(['test'], er => t.match(er, { code: 'ENOENT' }))
})

t.test('default env, start, and restart scripts', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({ name: 'x', version: '1.2.3' }),
    'server.js': 'console.log("hello, world")',
  })

  t.test('start', t => {
    runScript.exec(['start'], er => {
      if (er)
        throw er

      t.match(RUN_SCRIPTS, [
        {
          path: npm.localPrefix,
          args: [],
          scriptShell: undefined,
          stdio: 'inherit',
          stdioString: true,
          pkg: { name: 'x', version: '1.2.3', _id: 'x@1.2.3', scripts: {}},
          event: 'start',
        },
      ])
      t.end()
    })
  })

  t.test('env', t => {
    runScript.exec(['env'], er => {
      if (er)
        throw er

      t.match(RUN_SCRIPTS, [
        {
          path: npm.localPrefix,
          args: [],
          scriptShell: undefined,
          stdio: 'inherit',
          stdioString: true,
          pkg: {
            name: 'x',
            version: '1.2.3',
            _id: 'x@1.2.3',
            scripts: {
              env: 'env',
            },
          },
          event: 'env',
        },
      ])
      t.end()
    })
  })

  t.test('windows env', t => {
    runScriptWin.exec(['env'], er => {
      if (er)
        throw er

      t.match(RUN_SCRIPTS, [
        {
          path: npm.localPrefix,
          args: [],
          scriptShell: undefined,
          stdio: 'inherit',
          stdioString: true,
          pkg: { name: 'x',
            version: '1.2.3',
            _id: 'x@1.2.3',
            scripts: {
              env: 'SET',
            } },
          event: 'env',
        },
      ])
      t.end()
    })
  })

  t.test('restart', t => {
    runScript.exec(['restart'], er => {
      if (er)
        throw er

      t.match(RUN_SCRIPTS, [
        {
          path: npm.localPrefix,
          args: [],
          scriptShell: undefined,
          stdio: 'inherit',
          stdioString: true,
          pkg: { name: 'x',
            version: '1.2.3',
            _id: 'x@1.2.3',
            scripts: {
              restart: 'npm stop --if-present && npm start',
            } },
          event: 'restart',
        },
      ])
      t.end()
    })
  })
  t.end()
})

t.test('non-default env script', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
      scripts: {
        env: 'hello',
      },
    }),
  })

  t.test('env', t => {
    runScript.exec(['env'], er => {
      if (er)
        throw er

      t.match(RUN_SCRIPTS, [
        {
          path: npm.localPrefix,
          args: [],
          scriptShell: undefined,
          stdio: 'inherit',
          stdioString: true,
          pkg: {
            name: 'x',
            version: '1.2.3',
            _id: 'x@1.2.3',
            scripts: {
              env: 'hello',
            },
          },
          event: 'env',
        },
      ])
      t.end()
    })
  })

  t.test('env windows', t => {
    runScriptWin.exec(['env'], er => {
      if (er)
        throw er

      t.match(RUN_SCRIPTS, [
        {
          path: npm.localPrefix,
          args: [],
          scriptShell: undefined,
          stdio: 'inherit',
          stdioString: true,
          pkg: { name: 'x',
            version: '1.2.3',
            _id: 'x@1.2.3',
            scripts: {
              env: 'hello',
            },
          },
          event: 'env',
        },
      ])
      t.end()
    })
  })
  t.end()
})

t.test('try to run missing script', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      scripts: { hello: 'world' },
      bin: { goodnight: 'moon' },
    }),
  })
  t.test('no suggestions', t => {
    runScript.exec(['notevenclose'], er => {
      t.match(er, {
        message: 'Missing script: "notevenclose"',
      })
      t.end()
    })
  })
  t.test('script suggestions', t => {
    runScript.exec(['helo'], er => {
      t.match(er, {
        message: 'Missing script: "helo"',
      })
      t.match(er, {
        message: 'npm run hello',
      })
      t.end()
    })
  })
  t.test('bin suggestions', t => {
    runScript.exec(['goodneght'], er => {
      t.match(er, {
        message: 'Missing script: "goodneght"',
      })
      t.match(er, {
        message: 'npm exec goodnight',
      })
      t.end()
    })
  })
  t.test('with --if-present', t => {
    config['if-present'] = true
    runScript.exec(['goodbye'], er => {
      if (er)
        throw er

      t.strictSame(RUN_SCRIPTS, [], 'did not try to run anything')
      t.end()
    })
  })
  t.end()
})

t.test('run pre/post hooks', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
      scripts: {
        preenv: 'echo before the env',
        postenv: 'echo after the env',
      },
    }),
  })

  runScript.exec(['env'], er => {
    if (er)
      throw er

    t.match(RUN_SCRIPTS, [
      { event: 'preenv' },
      {
        path: npm.localPrefix,
        args: [],
        scriptShell: undefined,
        stdio: 'inherit',
        stdioString: true,
        pkg: { name: 'x',
          version: '1.2.3',
          _id: 'x@1.2.3',
          scripts: {
            env: 'env',
          } },
        event: 'env',
      },
      { event: 'postenv' },
    ])
    t.end()
  })
})

t.test('skip pre/post hooks when using ignoreScripts', t => {
  config['ignore-scripts'] = true

  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
      scripts: {
        preenv: 'echo before the env',
        postenv: 'echo after the env',
      },
    }),
  })

  runScript.exec(['env'], er => {
    if (er)
      throw er

    t.same(RUN_SCRIPTS, [
      {
        path: npm.localPrefix,
        args: [],
        scriptShell: undefined,
        stdio: 'inherit',
        stdioString: true,
        pkg: { name: 'x',
          version: '1.2.3',
          _id: 'x@1.2.3',
          scripts: {
            preenv: 'echo before the env',
            postenv: 'echo after the env',
            env: 'env',
          } },
        banner: true,
        event: 'env',
      },
    ])
    t.end()
    delete config['ignore-scripts']
  })
})

t.test('run silent', t => {
  npmlog.level = 'silent'
  t.teardown(() => {
    npmlog.level = 'warn'
  })

  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
      scripts: {
        preenv: 'echo before the env',
        postenv: 'echo after the env',
      },
    }),
  })

  runScript.exec(['env'], er => {
    if (er)
      throw er

    t.match(RUN_SCRIPTS, [
      {
        event: 'preenv',
        stdio: 'inherit',
      },
      {
        path: npm.localPrefix,
        args: [],
        scriptShell: undefined,
        stdio: 'inherit',
        stdioString: true,
        pkg: { name: 'x',
          version: '1.2.3',
          _id: 'x@1.2.3',
          scripts: {
            env: 'env',
          } },
        event: 'env',
        banner: false,
      },
      {
        event: 'postenv',
        stdio: 'inherit',
      },
    ])
    t.end()
  })
})

t.test('list scripts', t => {
  const scripts = {
    test: 'exit 2',
    start: 'node server.js',
    stop: 'node kill-server.js',
    preenv: 'echo before the env',
    postenv: 'echo after the env',
  }
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
      scripts,
    }),
  })

  t.test('no args', t => {
    runScript.exec([], er => {
      if (er)
        throw er
      t.strictSame(output, [
        ['Lifecycle scripts included in x@1.2.3:'],
        ['  test\n    exit 2'],
        ['  start\n    node server.js'],
        ['  stop\n    node kill-server.js'],
        ['\navailable via `npm run-script`:'],
        ['  preenv\n    echo before the env'],
        ['  postenv\n    echo after the env'],
        [''],
      ], 'basic report')
      t.end()
    })
  })

  t.test('silent', t => {
    npmlog.level = 'silent'
    runScript.exec([], er => {
      if (er)
        throw er
      t.strictSame(output, [])
      t.end()
    })
  })
  t.test('warn json', t => {
    npmlog.level = 'warn'
    config.json = true
    runScript.exec([], er => {
      if (er)
        throw er
      t.strictSame(output, [[JSON.stringify(scripts, 0, 2)]], 'json report')
      t.end()
    })
  })

  t.test('parseable', t => {
    config.parseable = true
    runScript.exec([], er => {
      if (er)
        throw er
      t.strictSame(output, [
        ['test:exit 2'],
        ['start:node server.js'],
        ['stop:node kill-server.js'],
        ['preenv:echo before the env'],
        ['postenv:echo after the env'],
      ])
      t.end()
    })
  })
  t.end()
})

t.test('list scripts when no scripts', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
    }),
  })

  runScript.exec([], er => {
    if (er)
      throw er
    t.strictSame(output, [], 'nothing to report')
    t.end()
  })
})

t.test('list scripts, only commands', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
      scripts: { preversion: 'echo doing the version dance' },
    }),
  })

  runScript.exec([], er => {
    if (er)
      throw er
    t.strictSame(output, [
      ['Lifecycle scripts included in x@1.2.3:'],
      ['  preversion\n    echo doing the version dance'],
      [''],
    ])
    t.end()
  })
})

t.test('list scripts, only non-commands', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
      scripts: { glorp: 'echo doing the glerp glop' },
    }),
  })

  runScript.exec([], er => {
    if (er)
      throw er
    t.strictSame(output, [
      ['Scripts available in x@1.2.3 via `npm run-script`:'],
      ['  glorp\n    echo doing the glerp glop'],
      [''],
    ])
    t.end()
  })
})

t.test('workspaces', t => {
  npm.localPrefix = t.testdir({
    packages: {
      a: {
        'package.json': JSON.stringify({
          name: 'a',
          version: '1.0.0',
          scripts: { glorp: 'echo a doing the glerp glop' },
        }),
      },
      b: {
        'package.json': JSON.stringify({
          name: 'b',
          version: '2.0.0',
          scripts: { glorp: 'echo b doing the glerp glop' },
        }),
      },
      c: {
        'package.json': JSON.stringify({
          name: 'c',
          version: '1.0.0',
          scripts: {
            test: 'exit 0',
            posttest: 'echo posttest',
            lorem: 'echo c lorem',
          },
        }),
      },
      d: {
        'package.json': JSON.stringify({
          name: 'd',
          version: '1.0.0',
          scripts: {
            test: 'exit 0',
            posttest: 'echo posttest',
          },
        }),
      },
      e: {
        'package.json': JSON.stringify({
          name: 'e',
          scripts: { test: 'exit 0', start: 'echo start something' },
        }),
      },
      noscripts: {
        'package.json': JSON.stringify({
          name: 'noscripts',
          version: '1.0.0',
        }),
      },
    },
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
      workspaces: ['packages/*'],
    }),
  })

  t.test('list all scripts', t => {
    runScript.execWorkspaces([], [], er => {
      if (er)
        throw er
      t.strictSame(output, [
        ['Scripts available in a@1.0.0 via `npm run-script`:'],
        ['  glorp\n    echo a doing the glerp glop'],
        [''],
        ['Scripts available in b@2.0.0 via `npm run-script`:'],
        ['  glorp\n    echo b doing the glerp glop'],
        [''],
        ['Lifecycle scripts included in c@1.0.0:'],
        ['  test\n    exit 0'],
        ['  posttest\n    echo posttest'],
        ['\navailable via `npm run-script`:'],
        ['  lorem\n    echo c lorem'],
        [''],
        ['Lifecycle scripts included in d@1.0.0:'],
        ['  test\n    exit 0'],
        ['  posttest\n    echo posttest'],
        [''],
        ['Lifecycle scripts included in e:'],
        ['  test\n    exit 0'],
        ['  start\n    echo start something'],
        [''],
      ])
      t.end()
    })
  })

  t.test('list regular scripts, filtered by name', t => {
    runScript.execWorkspaces([], ['a', 'b'], er => {
      if (er)
        throw er
      t.strictSame(output, [
        ['Scripts available in a@1.0.0 via `npm run-script`:'],
        ['  glorp\n    echo a doing the glerp glop'],
        [''],
        ['Scripts available in b@2.0.0 via `npm run-script`:'],
        ['  glorp\n    echo b doing the glerp glop'],
        [''],
      ])
      t.end()
    })
  })

  t.test('list regular scripts, filtered by path', t => {
    runScript.execWorkspaces([], ['./packages/a'], er => {
      if (er)
        throw er
      t.strictSame(output, [
        ['Scripts available in a@1.0.0 via `npm run-script`:'],
        ['  glorp\n    echo a doing the glerp glop'],
        [''],
      ])
      t.end()
    })
  })

  t.test('list regular scripts, filtered by parent folder', t => {
    runScript.execWorkspaces([], ['./packages'], er => {
      if (er)
        throw er
      t.strictSame(output, [
        ['Scripts available in a@1.0.0 via `npm run-script`:'],
        ['  glorp\n    echo a doing the glerp glop'],
        [''],
        ['Scripts available in b@2.0.0 via `npm run-script`:'],
        ['  glorp\n    echo b doing the glerp glop'],
        [''],
        ['Lifecycle scripts included in c@1.0.0:'],
        ['  test\n    exit 0'],
        ['  posttest\n    echo posttest'],
        ['\navailable via `npm run-script`:'],
        ['  lorem\n    echo c lorem'],
        [''],
        ['Lifecycle scripts included in d@1.0.0:'],
        ['  test\n    exit 0'],
        ['  posttest\n    echo posttest'],
        [''],
        ['Lifecycle scripts included in e:'],
        ['  test\n    exit 0'],
        ['  start\n    echo start something'],
        [''],
      ])
      t.end()
    })
  })

  t.test('list all scripts with colors', t => {
    npm.color = true
    runScript.execWorkspaces([], [], er => {
      if (er)
        throw er
      t.strictSame(output, [
        [
          '\u001b[1mScripts\u001b[22m available in \x1B[32ma@1.0.0\x1B[39m via `\x1B[34mnpm run-script\x1B[39m`:',
        ],
        ['  glorp\n    \x1B[2mecho a doing the glerp glop\x1B[22m'],
        [''],
        [
          '\u001b[1mScripts\u001b[22m available in \x1B[32mb@2.0.0\x1B[39m via `\x1B[34mnpm run-script\x1B[39m`:',
        ],
        ['  glorp\n    \x1B[2mecho b doing the glerp glop\x1B[22m'],
        [''],
        [
          '\x1B[0m\x1B[1mLifecycle scripts\x1B[22m\x1B[0m included in \x1B[32mc@1.0.0\x1B[39m:',
        ],
        ['  test\n    \x1B[2mexit 0\x1B[22m'],
        ['  posttest\n    \x1B[2mecho posttest\x1B[22m'],
        ['\navailable via `\x1B[34mnpm run-script\x1B[39m`:'],
        ['  lorem\n    \x1B[2mecho c lorem\x1B[22m'],
        [''],
        [
          '\x1B[0m\x1B[1mLifecycle scripts\x1B[22m\x1B[0m included in \x1B[32md@1.0.0\x1B[39m:',
        ],
        ['  test\n    \x1B[2mexit 0\x1B[22m'],
        ['  posttest\n    \x1B[2mecho posttest\x1B[22m'],
        [''],
        [
          '\x1B[0m\x1B[1mLifecycle scripts\x1B[22m\x1B[0m included in \x1B[32me\x1B[39m:',
        ],
        ['  test\n    \x1B[2mexit 0\x1B[22m'],
        ['  start\n    \x1B[2mecho start something\x1B[22m'],
        [''],
      ])
      t.end()
    })
  })

  t.test('list all scripts --json', t => {
    config.json = true
    runScript.execWorkspaces([], [], er => {
      if (er)
        throw er
      t.strictSame(output, [
        [
          '{\n' +
          '  "a": {\n' +
          '    "glorp": "echo a doing the glerp glop"\n' +
          '  },\n' +
          '  "b": {\n' +
          '    "glorp": "echo b doing the glerp glop"\n' +
          '  },\n' +
          '  "c": {\n' +
          '    "test": "exit 0",\n' +
          '    "posttest": "echo posttest",\n' +
          '    "lorem": "echo c lorem"\n' +
          '  },\n' +
          '  "d": {\n' +
          '    "test": "exit 0",\n' +
          '    "posttest": "echo posttest"\n' +
          '  },\n' +
          '  "e": {\n' +
          '    "test": "exit 0",\n' +
          '    "start": "echo start something"\n' +
          '  },\n' +
          '  "noscripts": {}\n' +
          '}',
        ],
      ])
      t.end()
    })
  })

  t.test('list all scripts --parseable', t => {
    config.parseable = true
    runScript.execWorkspaces([], [], er => {
      if (er)
        throw er
      t.strictSame(output, [
        ['a:glorp:echo a doing the glerp glop'],
        ['b:glorp:echo b doing the glerp glop'],
        ['c:test:exit 0'],
        ['c:posttest:echo posttest'],
        ['c:lorem:echo c lorem'],
        ['d:test:exit 0'],
        ['d:posttest:echo posttest'],
        ['e:test:exit 0'],
        ['e:start:echo start something'],
      ])
      t.end()
    })
  })

  t.test('list no scripts --loglevel=silent', t => {
    npmlog.level = 'silent'
    runScript.execWorkspaces([], [], er => {
      if (er)
        throw er
      t.strictSame(output, [])
      t.end()
    })
  })

  t.test('run scripts across all workspaces', t => {
    runScript.execWorkspaces(['test'], [], er => {
      if (er)
        throw er

      t.match(RUN_SCRIPTS, [
        {
          path: resolve(npm.localPrefix, 'packages/c'),
          pkg: { name: 'c', version: '1.0.0' },
          event: 'test',
        },
        {
          path: resolve(npm.localPrefix, 'packages/c'),
          pkg: { name: 'c', version: '1.0.0' },
          event: 'posttest',
        },
        {
          path: resolve(npm.localPrefix, 'packages/d'),
          pkg: { name: 'd', version: '1.0.0' },
          event: 'test',
        },
        {
          path: resolve(npm.localPrefix, 'packages/d'),
          pkg: { name: 'd', version: '1.0.0' },
          event: 'posttest',
        },
        {
          path: resolve(npm.localPrefix, 'packages/e'),
          pkg: { name: 'e' },
          event: 'test',
        },
      ])
      t.end()
    })
  })

  t.test('missing scripts in all workspaces', t => {
    const LOG = []
    npmlog.error = (err) => {
      LOG.push(String(err))
    }
    runScript.execWorkspaces(['missing-script'], [], er => {
      t.match(
        er,
        /Missing script: missing-script/,
        'should throw missing script error'
      )

      process.exitCode = 0 // clean exit code

      t.match(RUN_SCRIPTS, [])
      t.strictSame(LOG.map(cleanOutput), [
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: a@1.0.0',
        '  at location: {CWD}/test/lib/tap-testdir-run-script-workspaces/packages/a',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: b@2.0.0',
        '  at location: {CWD}/test/lib/tap-testdir-run-script-workspaces/packages/b',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: c@1.0.0',
        '  at location: {CWD}/test/lib/tap-testdir-run-script-workspaces/packages/c',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: d@1.0.0',
        '  at location: {CWD}/test/lib/tap-testdir-run-script-workspaces/packages/d',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: e',
        '  at location: {CWD}/test/lib/tap-testdir-run-script-workspaces/packages/e',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: noscripts@1.0.0',
        '  at location: {CWD}/test/lib/tap-testdir-run-script-workspaces/packages/noscripts',
      ], 'should log error msgs for each workspace script')

      t.end()
    })
  })

  t.test('missing scripts in some workspaces', t => {
    const LOG = []
    npmlog.error = (err) => {
      LOG.push(String(err))
    }
    runScript.execWorkspaces(['test'], ['a', 'b', 'c', 'd'], er => {
      if (er)
        throw er

      t.match(RUN_SCRIPTS, [])
      t.strictSame(LOG.map(cleanOutput), [
        'Lifecycle script `test` failed with error:',
        'Error: Missing script: "test"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: a@1.0.0',
        '  at location: {CWD}/test/lib/tap-testdir-run-script-workspaces/packages/a',
        'Lifecycle script `test` failed with error:',
        'Error: Missing script: "test"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: b@2.0.0',
        '  at location: {CWD}/test/lib/tap-testdir-run-script-workspaces/packages/b',
      ], 'should log error msgs for each workspace script')
      t.end()
    })
  })

  t.test('no workspaces when filtering by user args', t => {
    runScript.execWorkspaces([], ['foo', 'bar'], er => {
      t.equal(
        er.message,
        'No workspaces found:\n  --workspace=foo --workspace=bar',
        'should throw error msg'
      )
      t.end()
    })
  })

  t.test('no workspaces', t => {
    const _prevPrefix = npm.localPrefix
    npm.localPrefix = t.testdir({
      'package.json': JSON.stringify({
        name: 'foo',
        version: '1.0.0',
      }),
    })

    runScript.execWorkspaces([], [], er => {
      t.match(er, /No workspaces found!/, 'should throw error msg')
      npm.localPrefix = _prevPrefix
      t.end()
    })
  })

  t.test('single failed workspace run', t => {
    const RunScript = t.mock('../../lib/run-script.js', {
      '@npmcli/run-script': () => {
        throw new Error('err')
      },
      npmlog,
      '../../lib/utils/is-windows-shell.js': false,
    })
    const runScript = new RunScript(npm)

    runScript.execWorkspaces(['test'], ['c'], er => {
      t.ok('should complete running all targets')
      process.exitCode = 0 // clean up exit code
      t.end()
    })
  })

  t.test('failed workspace run with succeeded runs', t => {
    const RunScript = t.mock('../../lib/run-script.js', {
      '@npmcli/run-script': async opts => {
        if (opts.pkg.name === 'a')
          throw new Error('ERR')

        RUN_SCRIPTS.push(opts)
      },
      npmlog,
      '../../lib/utils/is-windows-shell.js': false,
    })
    const runScript = new RunScript(npm)

    runScript.execWorkspaces(['glorp'], ['a', 'b'], er => {
      t.match(RUN_SCRIPTS, [
        {
          path: resolve(npm.localPrefix, 'packages/b'),
          pkg: { name: 'b', version: '2.0.0' },
          event: 'glorp',
        },
      ])

      process.exitCode = 0 // clean up exit code
      t.end()
    })
  })

  t.end()
})
