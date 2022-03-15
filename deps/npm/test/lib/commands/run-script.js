const t = require('tap')
const { resolve } = require('path')
const { fake: mockNpm } = require('../../fixtures/mock-npm')

const normalizePath = p => p.replace(/\\+/g, '/').replace(/\r\n/g, '\n')

const cleanOutput = str => normalizePath(str).replace(normalizePath(process.cwd()), '{CWD}')

const RUN_SCRIPTS = []
const flatOptions = {
  scriptShell: undefined,
}
const defaultLoglevel = 'info'
const config = {
  json: false,
  parseable: false,
  'if-present': false,
  loglevel: defaultLoglevel,
}

const npm = mockNpm({
  localPrefix: __dirname,
  flatOptions,
  config,
  cmd: c => {
    return { description: `test ${c} description` }
  },
  output: (...msg) => output.push(msg),
})

const setLoglevel = (t, level) => {
  npm.config.set('loglevel', level)
  t.teardown(() => {
    npm.config.set('loglevel', defaultLoglevel)
  })
}

const output = []

const log = {
  error: () => null,
}

t.afterEach(() => {
  npm.color = false
  log.error = () => null
  output.length = 0
  RUN_SCRIPTS.length = 0
  config['if-present'] = false
  config.json = false
  config.parseable = false
})

const getRS = windows => {
  const RunScript = t.mock('../../../lib/commands/run-script.js', {
    '@npmcli/run-script': Object.assign(
      async opts => {
        RUN_SCRIPTS.push(opts)
      },
      {
        isServerPackage: require('@npmcli/run-script').isServerPackage,
      }
    ),
    'proc-log': log,
    '../../../lib/utils/is-windows-shell.js': windows,
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
    const res = await runScript.completion({ conf: { argv: { remain: ['npm', 'run', 'x'] } } })
    t.equal(res, undefined)
    t.end()
  })
  t.test('no package.json', async t => {
    const res = await runScript.completion({ conf: { argv: { remain: ['npm', 'run'] } } })
    t.strictSame(res, [])
    t.end()
  })
  t.test('has package.json, no scripts', async t => {
    writeFileSync(`${dir}/package.json`, JSON.stringify({}))
    const res = await runScript.completion({ conf: { argv: { remain: ['npm', 'run'] } } })
    t.strictSame(res, [])
    t.end()
  })
  t.test('has package.json, with scripts', async t => {
    writeFileSync(
      `${dir}/package.json`,
      JSON.stringify({
        scripts: { hello: 'echo hello', world: 'echo world' },
      })
    )
    const res = await runScript.completion({ conf: { argv: { remain: ['npm', 'run'] } } })
    t.strictSame(res, ['hello', 'world'])
    t.end()
  })
  t.end()
})

t.test('fail if no package.json', async t => {
  t.plan(2)
  npm.localPrefix = t.testdir()
  await t.rejects(runScript.exec([]), { code: 'ENOENT' })
  await t.rejects(runScript.exec(['test']), { code: 'ENOENT' })
})

t.test('default env, start, and restart scripts', t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({ name: 'x', version: '1.2.3' }),
    'server.js': 'console.log("hello, world")',
  })

  t.test('start', async t => {
    await runScript.exec(['start'])
    t.match(RUN_SCRIPTS, [
      {
        path: npm.localPrefix,
        args: [],
        scriptShell: undefined,
        stdio: 'inherit',
        stdioString: true,
        pkg: { name: 'x', version: '1.2.3', _id: 'x@1.2.3', scripts: {} },
        event: 'start',
      },
    ])
  })

  t.test('env', async t => {
    await runScript.exec(['env'])
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
  })

  t.test('windows env', async t => {
    await runScriptWin.exec(['env'])
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
            env: 'SET',
          },
        },
        event: 'env',
      },
    ])
  })

  t.test('restart', async t => {
    await runScript.exec(['restart'])

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
            restart: 'npm stop --if-present && npm start',
          },
        },
        event: 'restart',
      },
    ])
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

  t.test('env', async t => {
    await runScript.exec(['env'])
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
  })

  t.test('env windows', async t => {
    await runScriptWin.exec(['env'])
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
  t.test('no suggestions', async t => {
    await t.rejects(runScript.exec(['notevenclose']), 'Missing script: "notevenclose"')
  })
  t.test('script suggestions', async t => {
    await t.rejects(runScript.exec(['helo']), /Missing script: "helo"/)
    await t.rejects(runScript.exec(['helo']), /npm run hello/)
  })
  t.test('bin suggestions', async t => {
    await t.rejects(runScript.exec(['goodneght']), /Missing script: "goodneght"/)
    await t.rejects(runScript.exec(['goodneght']), /npm exec goodnight/)
  })
  t.test('with --if-present', async t => {
    config['if-present'] = true
    await runScript.exec(['goodbye'])
    t.strictSame(RUN_SCRIPTS, [], 'did not try to run anything')
  })
  t.end()
})

t.test('run pre/post hooks', async t => {
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

  await runScript.exec(['env'])

  t.match(RUN_SCRIPTS, [
    { event: 'preenv' },
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
    { event: 'postenv' },
  ])
})

t.test('skip pre/post hooks when using ignoreScripts', async t => {
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

  await runScript.exec(['env'])

  t.same(RUN_SCRIPTS, [
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
          preenv: 'echo before the env',
          postenv: 'echo after the env',
          env: 'env',
        },
      },
      banner: true,
      event: 'env',
    },
  ])
  delete config['ignore-scripts']
})

t.test('run silent', async t => {
  setLoglevel(t, 'silent')

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

  await runScript.exec(['env'])
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
      pkg: {
        name: 'x',
        version: '1.2.3',
        _id: 'x@1.2.3',
        scripts: {
          env: 'env',
        },
      },
      event: 'env',
      banner: false,
    },
    {
      event: 'postenv',
      stdio: 'inherit',
    },
  ])
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

  t.test('no args', async t => {
    await runScript.exec([])
    t.strictSame(
      output,
      [
        ['Lifecycle scripts included in x@1.2.3:'],
        ['  test\n    exit 2'],
        ['  start\n    node server.js'],
        ['  stop\n    node kill-server.js'],
        ['\navailable via `npm run-script`:'],
        ['  preenv\n    echo before the env'],
        ['  postenv\n    echo after the env'],
        [''],
      ],
      'basic report'
    )
  })

  t.test('silent', async t => {
    setLoglevel(t, 'silent')
    await runScript.exec([])
    t.strictSame(output, [])
  })
  t.test('warn json', async t => {
    config.json = true
    await runScript.exec([])
    t.strictSame(output, [[JSON.stringify(scripts, 0, 2)]], 'json report')
  })

  t.test('parseable', async t => {
    config.parseable = true
    await runScript.exec([])
    t.strictSame(output, [
      ['test:exit 2'],
      ['start:node server.js'],
      ['stop:node kill-server.js'],
      ['preenv:echo before the env'],
      ['postenv:echo after the env'],
    ])
  })
  t.end()
})

t.test('list scripts when no scripts', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
    }),
  })

  await runScript.exec([])
  t.strictSame(output, [], 'nothing to report')
})

t.test('list scripts, only commands', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
      scripts: { preversion: 'echo doing the version dance' },
    }),
  })

  await runScript.exec([])
  t.strictSame(output, [
    ['Lifecycle scripts included in x@1.2.3:'],
    ['  preversion\n    echo doing the version dance'],
    [''],
  ])
})

t.test('list scripts, only non-commands', async t => {
  npm.localPrefix = t.testdir({
    'package.json': JSON.stringify({
      name: 'x',
      version: '1.2.3',
      scripts: { glorp: 'echo doing the glerp glop' },
    }),
  })

  await runScript.exec([])
  t.strictSame(output, [
    ['Scripts available in x@1.2.3 via `npm run-script`:'],
    ['  glorp\n    echo doing the glerp glop'],
    [''],
  ])
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

  t.test('list all scripts', async t => {
    await runScript.execWorkspaces([], [])
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
  })

  t.test('list regular scripts, filtered by name', async t => {
    await runScript.execWorkspaces([], ['a', 'b'])
    t.strictSame(output, [
      ['Scripts available in a@1.0.0 via `npm run-script`:'],
      ['  glorp\n    echo a doing the glerp glop'],
      [''],
      ['Scripts available in b@2.0.0 via `npm run-script`:'],
      ['  glorp\n    echo b doing the glerp glop'],
      [''],
    ])
  })

  t.test('list regular scripts, filtered by path', async t => {
    await runScript.execWorkspaces([], ['./packages/a'])
    t.strictSame(output, [
      ['Scripts available in a@1.0.0 via `npm run-script`:'],
      ['  glorp\n    echo a doing the glerp glop'],
      [''],
    ])
  })

  t.test('list regular scripts, filtered by parent folder', async t => {
    await runScript.execWorkspaces([], ['./packages'])
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
  })

  t.test('list all scripts with colors', async t => {
    npm.color = true
    await runScript.execWorkspaces([], [])
    t.strictSame(output, [
      [
        /* eslint-disable-next-line max-len */
        '\u001b[1mScripts\u001b[22m available in \x1B[32ma@1.0.0\x1B[39m via `\x1B[34mnpm run-script\x1B[39m`:',
      ],
      ['  glorp\n    \x1B[2mecho a doing the glerp glop\x1B[22m'],
      [''],
      [
        /* eslint-disable-next-line max-len */
        '\u001b[1mScripts\u001b[22m available in \x1B[32mb@2.0.0\x1B[39m via `\x1B[34mnpm run-script\x1B[39m`:',
      ],
      ['  glorp\n    \x1B[2mecho b doing the glerp glop\x1B[22m'],
      [''],
      ['\x1B[0m\x1B[1mLifecycle scripts\x1B[22m\x1B[0m included in \x1B[32mc@1.0.0\x1B[39m:'],
      ['  test\n    \x1B[2mexit 0\x1B[22m'],
      ['  posttest\n    \x1B[2mecho posttest\x1B[22m'],
      ['\navailable via `\x1B[34mnpm run-script\x1B[39m`:'],
      ['  lorem\n    \x1B[2mecho c lorem\x1B[22m'],
      [''],
      ['\x1B[0m\x1B[1mLifecycle scripts\x1B[22m\x1B[0m included in \x1B[32md@1.0.0\x1B[39m:'],
      ['  test\n    \x1B[2mexit 0\x1B[22m'],
      ['  posttest\n    \x1B[2mecho posttest\x1B[22m'],
      [''],
      ['\x1B[0m\x1B[1mLifecycle scripts\x1B[22m\x1B[0m included in \x1B[32me\x1B[39m:'],
      ['  test\n    \x1B[2mexit 0\x1B[22m'],
      ['  start\n    \x1B[2mecho start something\x1B[22m'],
      [''],
    ])
  })

  t.test('list all scripts --json', async t => {
    config.json = true
    await runScript.execWorkspaces([], [])
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
  })

  t.test('list all scripts --parseable', async t => {
    config.parseable = true
    await runScript.execWorkspaces([], [])
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
  })

  t.test('list no scripts --loglevel=silent', async t => {
    setLoglevel(t, 'silent')
    await runScript.execWorkspaces([], [])
    t.strictSame(output, [])
  })

  t.test('run scripts across all workspaces', async t => {
    await runScript.execWorkspaces(['test'], [])

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
  })

  t.test('missing scripts in all workspaces', async t => {
    const LOG = []
    log.error = err => {
      LOG.push(String(err))
    }
    await t.rejects(
      runScript.execWorkspaces(['missing-script'], []),
      /Missing script: missing-script/,
      'should throw missing script error'
    )

    process.exitCode = 0 // clean exit code

    t.match(RUN_SCRIPTS, [])
    t.strictSame(
      LOG.map(cleanOutput),
      [
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: a@1.0.0',
        '  at location: {CWD}/test/lib/commands/tap-testdir-run-script-workspaces/packages/a',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: b@2.0.0',
        '  at location: {CWD}/test/lib/commands/tap-testdir-run-script-workspaces/packages/b',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: c@1.0.0',
        '  at location: {CWD}/test/lib/commands/tap-testdir-run-script-workspaces/packages/c',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: d@1.0.0',
        '  at location: {CWD}/test/lib/commands/tap-testdir-run-script-workspaces/packages/d',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: e',
        '  at location: {CWD}/test/lib/commands/tap-testdir-run-script-workspaces/packages/e',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: noscripts@1.0.0',
        /* eslint-disable-next-line max-len */
        '  at location: {CWD}/test/lib/commands/tap-testdir-run-script-workspaces/packages/noscripts',
      ],
      'should log error msgs for each workspace script'
    )
  })

  t.test('missing scripts in some workspaces', async t => {
    const LOG = []
    log.error = err => {
      LOG.push(String(err))
    }
    await runScript.execWorkspaces(['test'], ['a', 'b', 'c', 'd'])
    t.match(RUN_SCRIPTS, [])
    t.strictSame(
      LOG.map(cleanOutput),
      [
        'Lifecycle script `test` failed with error:',
        'Error: Missing script: "test"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: a@1.0.0',
        '  at location: {CWD}/test/lib/commands/tap-testdir-run-script-workspaces/packages/a',
        'Lifecycle script `test` failed with error:',
        'Error: Missing script: "test"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: b@2.0.0',
        '  at location: {CWD}/test/lib/commands/tap-testdir-run-script-workspaces/packages/b',
      ],
      'should log error msgs for each workspace script'
    )
  })

  t.test('no workspaces when filtering by user args', async t => {
    await t.rejects(
      runScript.execWorkspaces([], ['foo', 'bar']),
      'No workspaces found:\n  --workspace=foo --workspace=bar',
      'should throw error msg'
    )
  })

  t.test('no workspaces', async t => {
    const _prevPrefix = npm.localPrefix
    npm.localPrefix = t.testdir({
      'package.json': JSON.stringify({
        name: 'foo',
        version: '1.0.0',
      }),
    })

    await t.rejects(
      runScript.execWorkspaces([], []),
      /No workspaces found!/,
      'should throw error msg'
    )
    npm.localPrefix = _prevPrefix
  })

  t.test('single failed workspace run', async t => {
    const RunScript = t.mock('../../../lib/commands/run-script.js', {
      '@npmcli/run-script': () => {
        throw new Error('err')
      },
      'proc-log': log,
      '../../../lib/utils/is-windows-shell.js': false,
    })
    const runScript = new RunScript(npm)

    await runScript.execWorkspaces(['test'], ['c'])
    process.exitCode = 0 // clean up exit code
  })

  t.test('failed workspace run with succeeded runs', async t => {
    const RunScript = t.mock('../../../lib/commands/run-script.js', {
      '@npmcli/run-script': async opts => {
        if (opts.pkg.name === 'a') {
          throw new Error('ERR')
        }

        RUN_SCRIPTS.push(opts)
      },
      'proc-log': log,
      '../../../lib/utils/is-windows-shell.js': false,
    })
    const runScript = new RunScript(npm)

    await runScript.execWorkspaces(['glorp'], ['a', 'b'])
    t.match(RUN_SCRIPTS, [
      {
        path: resolve(npm.localPrefix, 'packages/b'),
        pkg: { name: 'b', version: '2.0.0' },
        event: 'glorp',
      },
    ])

    process.exitCode = 0 // clean up exit code
  })

  t.end()
})
