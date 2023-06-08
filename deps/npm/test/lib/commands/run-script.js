const t = require('tap')
const { resolve } = require('path')
const realRunScript = require('@npmcli/run-script')
const mockNpm = require('../../fixtures/mock-npm')
const { cleanCwd } = require('../../fixtures/clean-snapshot')

const mockRs = async (t, { windows = false, runScript, ...opts } = {}) => {
  let RUN_SCRIPTS = []

  t.afterEach(() => RUN_SCRIPTS = [])

  const mock = await mockNpm(t, {
    ...opts,
    command: 'run-script',
    mocks: {
      '@npmcli/run-script': Object.assign(
        async rs => {
          if (runScript) {
            await runScript(rs)
          }
          RUN_SCRIPTS.push(rs)
        },
        realRunScript
      ),
      '{LIB}/utils/is-windows.js': { isWindowsShell: windows },
    },
  })

  return {
    ...mock,
    RUN_SCRIPTS: () => RUN_SCRIPTS,
    runScript: mock['run-script'],
    cleanLogs: () => mock.logs.error.flat().map(v => v.toString()).map(cleanCwd),
  }
}

t.test('completion', async t => {
  const completion = async (t, remain, pkg, isFish = false) => {
    const { runScript } = await mockRs(t,
      pkg ? { prefixDir: { 'package.json': JSON.stringify(pkg) } } : {}
    )
    return runScript.completion({ conf: { argv: { remain } }, isFish })
  }

  t.test('already have a script name', async t => {
    const res = await completion(t, ['npm', 'run', 'x'])
    t.equal(res, undefined)
  })
  t.test('no package.json', async t => {
    const res = await completion(t, ['npm', 'run'])
    t.strictSame(res, [])
  })
  t.test('has package.json, no scripts', async t => {
    const res = await completion(t, ['npm', 'run'], {})
    t.strictSame(res, [])
  })
  t.test('has package.json, with scripts', async t => {
    const res = await completion(t, ['npm', 'run'], {
      scripts: { hello: 'echo hello', world: 'echo world' },
    })
    t.strictSame(res, ['hello', 'world'])
  })

  t.test('fish shell', async t => {
    const res = await completion(t, ['npm', 'run'], {
      scripts: { hello: 'echo hello', world: 'echo world' },
    }, true)
    t.strictSame(res, ['hello\techo hello', 'world\techo world'])
  })
})

t.test('fail if no package.json', async t => {
  const { runScript } = await mockRs(t)
  await t.rejects(runScript.exec([]), { code: 'ENOENT' })
  await t.rejects(runScript.exec(['test']), { code: 'ENOENT' })
})

t.test('default env, start, and restart scripts', async t => {
  const { npm, runScript, RUN_SCRIPTS } = await mockRs(t, {
    prefixDir: {
      'package.json': JSON.stringify({ name: 'x', version: '1.2.3' }),
      'server.js': 'console.log("hello, world")',
    },
  })

  t.test('start', async t => {
    await runScript.exec(['start'])
    t.match(RUN_SCRIPTS(), [
      {
        path: npm.localPrefix,
        args: [],
        scriptShell: undefined,
        stdio: 'inherit',
        pkg: { name: 'x', version: '1.2.3', _id: 'x@1.2.3', scripts: {} },
        event: 'start',
      },
    ])
  })

  t.test('env', async t => {
    await runScript.exec(['env'])
    t.match(RUN_SCRIPTS(), [
      {
        path: npm.localPrefix,
        args: [],
        scriptShell: undefined,
        stdio: 'inherit',
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

  t.test('restart', async t => {
    await runScript.exec(['restart'])

    t.match(RUN_SCRIPTS(), [
      {
        path: npm.localPrefix,
        args: [],
        scriptShell: undefined,
        stdio: 'inherit',
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
})

t.test('default windows env', async t => {
  const { npm, runScript, RUN_SCRIPTS } = await mockRs(t, {
    windows: true,
    prefixDir: {
      'package.json': JSON.stringify({ name: 'x', version: '1.2.3' }),
      'server.js': 'console.log("hello, world")',
    },
  })
  await runScript.exec(['env'])
  t.match(RUN_SCRIPTS(), [
    {
      path: npm.localPrefix,
      args: [],
      scriptShell: undefined,
      stdio: 'inherit',
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

t.test('non-default env script', async t => {
  const { npm, runScript, RUN_SCRIPTS } = await mockRs(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'x',
        version: '1.2.3',
        scripts: {
          env: 'hello',
        },
      }),
    },
  })

  t.test('env', async t => {
    await runScript.exec(['env'])
    t.match(RUN_SCRIPTS(), [
      {
        path: npm.localPrefix,
        args: [],
        scriptShell: undefined,
        stdio: 'inherit',
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
})

t.test('non-default env script windows', async t => {
  const { npm, runScript, RUN_SCRIPTS } = await mockRs(t, {
    windows: true,
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'x',
        version: '1.2.3',
        scripts: {
          env: 'hello',
        },
      }),
    },
  })

  await runScript.exec(['env'])

  t.match(RUN_SCRIPTS(), [
    {
      path: npm.localPrefix,
      args: [],
      scriptShell: undefined,
      stdio: 'inherit',
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

t.test('try to run missing script', async t => {
  t.test('errors', async t => {
    const { runScript } = await mockRs(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          scripts: { hello: 'world' },
          bin: { goodnight: 'moon' },
        }),
      },
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
  })

  t.test('with --if-present', async t => {
    const { runScript, RUN_SCRIPTS } = await mockRs(t, {
      config: { 'if-present': true },
      prefixDir: {
        'package.json': JSON.stringify({
          scripts: { hello: 'world' },
          bin: { goodnight: 'moon' },
        }),
      },
    })
    await runScript.exec(['goodbye'])
    t.strictSame(RUN_SCRIPTS(), [], 'did not try to run anything')
  })
})

t.test('run pre/post hooks', async t => {
  const { npm, runScript, RUN_SCRIPTS } = await mockRs(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'x',
        version: '1.2.3',
        scripts: {
          preenv: 'echo before the env',
          postenv: 'echo after the env',
        },
      }),
    },
  })

  await runScript.exec(['env'])

  t.match(RUN_SCRIPTS(), [
    { event: 'preenv' },
    {
      path: npm.localPrefix,
      args: [],
      scriptShell: undefined,
      stdio: 'inherit',
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
  const { npm, runScript, RUN_SCRIPTS } = await mockRs(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'x',
        version: '1.2.3',
        scripts: {
          preenv: 'echo before the env',
          postenv: 'echo after the env',
        },
      }),
    },
    config: { 'ignore-scripts': true },
  })

  await runScript.exec(['env'])

  t.same(RUN_SCRIPTS(), [
    {
      path: npm.localPrefix,
      args: [],
      scriptShell: undefined,
      stdio: 'inherit',
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
})

t.test('run silent', async t => {
  const { npm, runScript, RUN_SCRIPTS } = await mockRs(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'x',
        version: '1.2.3',
        scripts: {
          preenv: 'echo before the env',
          postenv: 'echo after the env',
        },
      }),
    },
    config: { silent: true },
  })

  await runScript.exec(['env'])
  t.match(RUN_SCRIPTS(), [
    {
      event: 'preenv',
      stdio: 'inherit',
    },
    {
      path: npm.localPrefix,
      args: [],
      scriptShell: undefined,
      stdio: 'inherit',
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

t.test('list scripts', async t => {
  const scripts = {
    test: 'exit 2',
    start: 'node server.js',
    stop: 'node kill-server.js',
    preenv: 'echo before the env',
    postenv: 'echo after the env',
  }

  const mockList = async (t, config = {}) => {
    const mock = await mockRs(t, {
      prefixDir: {
        'package.json': JSON.stringify({
          name: 'x',
          version: '1.2.3',
          scripts,
        }),
      },
      config,
    })

    await mock.runScript.exec([])

    return mock.outputs
  }

  t.test('no args', async t => {
    const output = await mockList(t)
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
    const outputs = await mockList(t, { silent: true })
    t.strictSame(outputs, [])
  })
  t.test('warn json', async t => {
    const outputs = await mockList(t, { json: true })
    t.strictSame(outputs, [[JSON.stringify(scripts, 0, 2)]], 'json report')
  })

  t.test('parseable', async t => {
    const outputs = await mockList(t, { parseable: true })
    t.strictSame(outputs, [
      ['test:exit 2'],
      ['start:node server.js'],
      ['stop:node kill-server.js'],
      ['preenv:echo before the env'],
      ['postenv:echo after the env'],
    ])
  })
})

t.test('list scripts when no scripts', async t => {
  const { runScript, outputs } = await mockRs(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'x',
        version: '1.2.3',
      }),
    },
  })

  await runScript.exec([])
  t.strictSame(outputs, [], 'nothing to report')
})

t.test('list scripts, only commands', async t => {
  const { runScript, outputs } = await mockRs(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'x',
        version: '1.2.3',
        scripts: { preversion: 'echo doing the version dance' },
      }),
    },
  })

  await runScript.exec([])
  t.strictSame(outputs, [
    ['Lifecycle scripts included in x@1.2.3:'],
    ['  preversion\n    echo doing the version dance'],
    [''],
  ])
})

t.test('list scripts, only non-commands', async t => {
  const { runScript, outputs } = await mockRs(t, {
    prefixDir: {
      'package.json': JSON.stringify({
        name: 'x',
        version: '1.2.3',
        scripts: { glorp: 'echo doing the glerp glop' },
      }),
    },
  })

  await runScript.exec([])
  t.strictSame(outputs, [
    ['Scripts available in x@1.2.3 via `npm run-script`:'],
    ['  glorp\n    echo doing the glerp glop'],
    [''],
  ])
})

t.test('workspaces', async t => {
  const mockWorkspaces = async (t, {
    runScript,
    prefixDir,
    workspaces = true,
    exec = [],
    ...config
  } = {}) => {
    const mock = await mockRs(t, {
      prefixDir: prefixDir || {
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
      },
      config: {
        ...Array.isArray(workspaces) ? { workspace: workspaces } : { workspaces },
        ...config,
      },
      runScript,
    })
    if (exec) {
      await mock.runScript.exec(exec)
    }
    return mock
  }

  t.test('list all scripts', async t => {
    const { outputs } = await mockWorkspaces(t)
    t.strictSame(outputs, [
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
    const { outputs } = await mockWorkspaces(t, { workspaces: ['a', 'b'] })
    t.strictSame(outputs, [
      ['Scripts available in a@1.0.0 via `npm run-script`:'],
      ['  glorp\n    echo a doing the glerp glop'],
      [''],
      ['Scripts available in b@2.0.0 via `npm run-script`:'],
      ['  glorp\n    echo b doing the glerp glop'],
      [''],
    ])
  })

  t.test('list regular scripts, filtered by path', async t => {
    const { outputs } = await mockWorkspaces(t, { workspaces: ['./packages/a'] })
    t.strictSame(outputs, [
      ['Scripts available in a@1.0.0 via `npm run-script`:'],
      ['  glorp\n    echo a doing the glerp glop'],
      [''],
    ])
  })

  t.test('list regular scripts, filtered by parent folder', async t => {
    const { outputs } = await mockWorkspaces(t, { workspaces: ['./packages'] })
    t.strictSame(outputs, [
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
    const { outputs } = await mockWorkspaces(t, { color: 'always' })
    t.strictSame(outputs, [
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
    const { outputs } = await mockWorkspaces(t, { json: true })
    t.strictSame(outputs, [
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
    const { outputs } = await mockWorkspaces(t, { parseable: true })
    t.strictSame(outputs, [
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
    const { outputs } = await mockWorkspaces(t, { silent: true })
    t.strictSame(outputs, [])
  })

  t.test('run scripts across all workspaces', async t => {
    const { npm, RUN_SCRIPTS } = await mockWorkspaces(t, { exec: ['test'] })

    t.match(RUN_SCRIPTS(), [
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
    const { runScript, RUN_SCRIPTS, cleanLogs } = await mockWorkspaces(t, { exec: null })

    await t.rejects(
      runScript.exec(['missing-script']),
      /Missing script: missing-script/,
      'should throw missing script error'
    )

    t.match(RUN_SCRIPTS(), [])
    t.strictSame(
      cleanLogs(),
      [
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: a@1.0.0',
        '  at location: {CWD}/prefix/packages/a',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: b@2.0.0',
        '  at location: {CWD}/prefix/packages/b',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: c@1.0.0',
        '  at location: {CWD}/prefix/packages/c',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: d@1.0.0',
        '  at location: {CWD}/prefix/packages/d',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: e',
        '  at location: {CWD}/prefix/packages/e',
        'Lifecycle script `missing-script` failed with error:',
        'Error: Missing script: "missing-script"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: noscripts@1.0.0',
        '  at location: {CWD}/prefix/packages/noscripts',
      ],
      'should log error msgs for each workspace script'
    )
  })

  t.test('missing scripts in some workspaces', async t => {
    const { RUN_SCRIPTS, cleanLogs } = await mockWorkspaces(t, {
      exec: ['test'],
      workspaces: ['a', 'b', 'c', 'd'],
    })

    t.match(RUN_SCRIPTS(), [])
    t.strictSame(
      cleanLogs(),
      [
        'Lifecycle script `test` failed with error:',
        'Error: Missing script: "test"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: a@1.0.0',
        '  at location: {CWD}/prefix/packages/a',
        'Lifecycle script `test` failed with error:',
        'Error: Missing script: "test"\n\nTo see a list of scripts, run:\n  npm run',
        '  in workspace: b@2.0.0',
        '  at location: {CWD}/prefix/packages/b',
      ],
      'should log error msgs for each workspace script'
    )
  })

  t.test('no workspaces when filtering by user args', async t => {
    await t.rejects(
      mockWorkspaces(t, { workspaces: ['foo', 'bar'] }),
      'No workspaces found:\n  --workspace=foo --workspace=bar',
      'should throw error msg'
    )
  })

  t.test('no workspaces', async t => {
    await t.rejects(
      mockWorkspaces(t, {
        prefixDir: {
          'package.json': JSON.stringify({
            name: 'foo',
            version: '1.0.0',
          }),
        },
      }),
      /No workspaces found!/,
      'should throw error msg'
    )
  })

  t.test('single failed workspace run', async t => {
    const { cleanLogs } = await mockWorkspaces(t, {
      runScript: () => {
        throw new Error('err')
      },
      exec: ['test'],
      workspaces: ['c'],
    })

    t.strictSame(
      cleanLogs(),
      [
        'Lifecycle script `test` failed with error:',
        'Error: err',
        '  in workspace: c@1.0.0',
        '  at location: {CWD}/prefix/packages/c',
      ],
      'should log error msgs for each workspace script'
    )
  })

  t.test('failed workspace run with succeeded runs', async t => {
    const { cleanLogs, RUN_SCRIPTS, prefix } = await mockWorkspaces(t, {
      runScript: (opts) => {
        if (opts.pkg.name === 'a') {
          throw new Error('ERR')
        }
      },
      exec: ['glorp'],
      workspaces: ['a', 'b'],
    })

    t.strictSame(
      cleanLogs(),
      [
        'Lifecycle script `glorp` failed with error:',
        'Error: ERR',
        '  in workspace: a@1.0.0',
        '  at location: {CWD}/prefix/packages/a',
      ],
      'should log error msgs for each workspace script'
    )

    t.match(RUN_SCRIPTS(), [
      {
        path: resolve(prefix, 'packages/b'),
        pkg: { name: 'b', version: '2.0.0' },
        event: 'glorp',
      },
    ])
  })
})
