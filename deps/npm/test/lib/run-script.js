const t = require('tap')
const requireInject = require('require-inject')

const RUN_SCRIPTS = []
const npm = {
  localPrefix: __dirname,
  flatOptions: {
    scriptShell: undefined,
    json: false,
    parseable: false,
  },
  config: {
    settings: {
      'if-present': false,
    },
    get: k => npm.config.settings[k],
    set: (k, v) => {
      npm.config.settings[k] = v
    },
  },
  output: (...msg) => output.push(msg),
}

const output = []

t.afterEach(cb => {
  output.length = 0
  RUN_SCRIPTS.length = 0
  npm.flatOptions.json = false
  npm.flatOptions.parseable = false
  cb()
})

const npmlog = { level: 'warn' }
const getRS = windows => {
  const RunScript = requireInject('../../lib/run-script.js', {
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
    }),
  })
  t.test('no suggestions', t => {
    runScript.exec(['notevenclose'], er => {
      t.match(er, {
        message: 'missing script: notevenclose',
      })
      t.end()
    })
  })
  t.test('suggestions', t => {
    runScript.exec(['helo'], er => {
      t.match(er, {
        message: 'missing script: helo\n\nDid you mean this?\n    hello',
      })
      t.end()
    })
  })
  t.test('with --if-present', t => {
    npm.config.set('if-present', true)
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
  npm.flatOptions.ignoreScripts = true

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

    t.deepEqual(RUN_SCRIPTS, [
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
    delete npm.flatOptions.ignoreScripts
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
        ['Lifecycle scripts included in x:'],
        ['  test\n    exit 2'],
        ['  start\n    node server.js'],
        ['  stop\n    node kill-server.js'],
        ['\navailable via `npm run-script`:'],
        ['  preenv\n    echo before the env'],
        ['  postenv\n    echo after the env'],
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
    npm.flatOptions.json = true
    runScript.exec([], er => {
      if (er)
        throw er
      t.strictSame(output, [[JSON.stringify(scripts, 0, 2)]], 'json report')
      t.end()
    })
  })

  t.test('parseable', t => {
    npm.flatOptions.parseable = true
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
      ['Lifecycle scripts included in x:'],
      ['  preversion\n    echo doing the version dance'],
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
      ['Scripts available in x via `npm run-script`:'],
      ['  glorp\n    echo doing the glerp glop'],
    ])
    t.end()
  })
})
