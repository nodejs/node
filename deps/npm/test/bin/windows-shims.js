const t = require('tap')
const { spawnSync } = require('node:child_process')
const { resolve, join, extname, basename } = require('node:path')
const { readFileSync, chmodSync, readdirSync, statSync } = require('node:fs')
const Diff = require('diff')
const { moveRemove } = require('rimraf')
const { sync: which } = require('which')
const { version } = require('../../package.json')

const readNonJsFiles = (dir) => readdirSync(dir).reduce((acc, shim) => {
  const p = join(dir, shim)
  if (extname(p) !== '.js' && !statSync(p).isDirectory()) {
    acc[shim] = readFileSync(p, 'utf-8')
  }
  return acc
}, {})

const ROOT = resolve(__dirname, '../..')
const BIN = join(ROOT, 'bin')
const SHIMS = readNonJsFiles(BIN)
const NODE_GYP = readNonJsFiles(join(BIN, 'node-gyp-bin'))
const SHIM_EXTS = [...new Set(Object.keys(SHIMS).map(p => extname(p)))]
const PACKAGE_NAME = 'test'
const PACKAGE_VERSION = '1.0.0'
const SCRIPT_NAME = 'args.js'

t.test('shim contents', t => {
  // these scripts should be kept in sync so this tests the contents of each
  // and does a diff to ensure the only differences between them are necessary
  const diffFiles = (npm, npx) => Diff.diffChars(npm, npx)
    .filter(v => v.added || v.removed)
    .reduce((acc, v) => {
      if (v.value.length === 1) {
        acc.letters.add(v.value.toUpperCase())
      } else {
        acc.diff.push(v.value)
      }
      return acc
    }, { diff: [], letters: new Set() })

  t.plan(SHIM_EXTS.length)

  t.test('bash', t => {
    const { diff, letters } = diffFiles(SHIMS.npm, SHIMS.npx)
    t.strictSame(diff, [])
    t.strictSame([...letters], ['M', 'X'], 'all other changes are m->x')
    t.end()
  })

  t.test('cmd', t => {
    const { diff, letters } = diffFiles(SHIMS['npm.cmd'], SHIMS['npx.cmd'])
    t.strictSame(diff, [])
    t.strictSame([...letters], ['M', 'X'], 'all other changes are m->x')
    t.end()
  })

  t.test('pwsh', t => {
    const { diff, letters } = diffFiles(SHIMS['npm.ps1'], SHIMS['npx.ps1'])
    t.strictSame(diff, [])
    t.strictSame([...letters], ['M', 'X'], 'all other changes are m->x')
    t.end()
  })
})

t.test('node-gyp', t => {
  // these files need to exist to avoid breaking yarn 1.x

  for (const [key, file] of Object.entries(NODE_GYP)) {
    t.match(file, /npm_config_node_gyp/, `${key} contains env var`)
    t.match(
      file,
      /[\\/]\.\.[\\/]\.\.[\\/]node_modules[\\/]node-gyp[\\/]bin[\\/]node-gyp\.js/,
      `${key} contains path`
    )
  }

  t.end()
})

t.test('run shims', t => {
  const path = t.testdir({
    ...SHIMS,
    'node.exe': readFileSync(process.execPath),
    // simulate the state where one version of npm is installed
    // with node, but we should load the globally installed one
    'global-prefix': {
      node_modules: {
        npm: t.fixture('symlink', ROOT),
      },
    },
    // put in a shim that ONLY prints the intended global prefix,
    // and should not be used for anything else.
    node_modules: {
      npm: {
        bin: {
          'npm-prefix.js': `
            const { resolve } = require('path')
            console.log(resolve(__dirname, '../../../global-prefix'))
          `,
          'npx-cli.js': `throw new Error('local npx should not be called')`,
          'npm-cli.js': `throw new Error('local npm should not be called')`,
        },
      },
    },
    // test script returning all command line arguments
    [SCRIPT_NAME]: `#!/usr/bin/env node\n\nprocess.argv.slice(2).forEach((arg) => console.log(arg))`,
    // package.json for the test script
    'package.json': `
      {
        "name": "${PACKAGE_NAME}",
        "version": "${PACKAGE_VERSION}",
        "scripts": {
          "test": "node ${SCRIPT_NAME}"
        },
        "bin": "${SCRIPT_NAME}"
      }`,
  })

  // The removal of this fixture causes this test to fail when done with
  // the default tap removal. Using rimraf's `moveRemove` seems to make this
  // work reliably. Don't remove this line in the future without making sure
  // this test passes the full windows suite at least 3 consecutive times.
  t.teardown(() => moveRemove(join(path, 'node.exe')))

  const spawnPath = (cmd, args, { log, stdioString = true, ...opts } = {}) => {
    if (cmd.endsWith('bash.exe')) {
      // only cygwin *requires* the -l, but the others are ok with it
      args.unshift('-l')
    }
    if (cmd.toLowerCase().endsWith('powershell.exe') || cmd.toLowerCase().endsWith('pwsh.exe')) {
      // pwsh *requires* the -Command, Windows PowerShell defaults to it
      args.unshift('-Command')
      // powershell requires escaping double-quotes for this test
      args = args.map(elem => elem.replaceAll('"', '\\"'))
    }
    const result = spawnSync(`"${cmd}"`, args, {
      // don't hit the registry for the update check
      env: { PATH: path, npm_config_update_notifier: 'false' },
      cwd: path,
      windowsHide: true,
      shell: true,
      ...opts,
    })
    if (stdioString) {
      result.stdout = result.stdout?.toString()?.trim()
      result.stderr = result.stderr?.toString()?.trim()
    }
    return {
      status: result.status,
      signal: result.signal,
      stdout: result.stdout,
      stderr: result.stderr,
    }
  }

  const getWslVersion = (cmd) => {
    const defaultVersion = 1
    try {
      const opts = { shell: cmd, env: process.env }
      const wsl = spawnPath('wslpath', [`'${which('wsl')}'`], opts).stdout
      const distrosRaw = spawnPath(wsl, ['-l', '-v'], { ...opts, stdioString: false }).stdout
      const distros = spawnPath('iconv', ['-f', 'unicode'], { ...opts, input: distrosRaw }).stdout
      const distroArgs = distros
        .replace(/\r\n/g, '\n')
        .split('\n')
        .slice(1)
        .find(d => d.startsWith('*'))
        .replace(/\s+/g, ' ')
        .split(' ')
      return Number(distroArgs[distroArgs.length - 1]) || defaultVersion
    } catch {
      return defaultVersion
    }
  }

  for (const shim of Object.keys(SHIMS)) {
    chmodSync(join(path, shim), 0o755)
  }

  const { ProgramFiles = '/', SystemRoot = '/', NYC_CONFIG, WINDOWS_SHIMS_TEST } = process.env
  const skipDefault = WINDOWS_SHIMS_TEST || process.platform === 'win32'
    ? null : 'test not relevant on platform'

  const shells = Object.entries({
    cmd: 'cmd',
    powershell: 'powershell',
    pwsh: 'pwsh',
    git: join(ProgramFiles, 'Git', 'bin', 'bash.exe'),
    'user git': join(ProgramFiles, 'Git', 'usr', 'bin', 'bash.exe'),
    wsl: join(SystemRoot, 'System32', 'bash.exe'),
    cygwin: resolve(SystemRoot, '/', 'cygwin64', 'bin', 'bash.exe'),
  }).map(([name, cmd]) => {
    let match = {}
    const skip = { reason: skipDefault, fail: WINDOWS_SHIMS_TEST }
    const isBash = cmd.endsWith('bash.exe')
    const testName = `${name} ${isBash ? 'bash' : ''}`.trim()

    if (!skip.reason) {
      if (isBash) {
        try {
          // If WSL is installed, it *has* a bash.exe, but it fails if
          // there is no distro installed, so we need to detect that.
          if (spawnPath(cmd, ['-c', 'exit 0']).status !== 0) {
            throw new Error('not installed')
          }
          if (name === 'cygwin' && NYC_CONFIG) {
            throw new Error('does not play nicely with nyc')
          }
          // WSL version 1 does not work due to
          // https://github.com/microsoft/WSL/issues/2370
          if (name === 'wsl' && getWslVersion(cmd) === 1) {
            match = {
              status: 1,
              stderr: 'WSL 1 is not supported. Please upgrade to WSL 2 or above.',
              stdout: String,
            }
          }
        } catch (err) {
          skip.reason = err.message
        }
      } else {
        try {
          cmd = which(cmd)
        } catch {
          skip.reason = 'not installed'
        }
      }
    }

    return {
      match,
      cmd,
      name: testName,
      skip: {
        ...skip,
        reason: skip.reason ? `${testName} - ${skip.reason}` : null,
      },
    }
  })

  const matchCmd = (t, cmd, bin, match, params, expected) => {
    const args = []
    const opts = {}

    switch (basename(cmd).toLowerCase()) {
      case 'cmd.exe':
        cmd = `${bin}.cmd`
        break
      case 'bash.exe':
        args.push(bin)
        break
      case 'powershell.exe':
      case 'pwsh.exe':
        args.push(`${bin}.ps1`)
        break
      default:
        throw new Error('unknown shell')
    }

    const result = spawnPath(cmd, [...args, ...params], opts)

    // skip the first 3 lines of "npm test" to get the actual script output
    if (params[0].startsWith('test')) {
      result.stdout = result.stdout?.toString().split('\n').slice(3).join('\n').trim()
    }

    t.match(result, {
      status: 0,
      signal: null,
      stderr: '',
      stdout: expected,
      ...match,
    }, `${cmd} ${bin} ${params[0]}`)
  }

  // Array with command line parameters and expected output
  const tests = [
    { bin: 'npm', params: ['help'], expected: `npm@${version} ${ROOT}` },
    { bin: 'npx', params: ['--version'], expected: version },
    { bin: 'npm', params: ['test'], expected: '' },
    { bin: 'npm', params: [`test -- hello -p1 world -p2 "hello world" --q1=hello world --q2="hello world"`], expected: `hello\n-p1\nworld\n-p2\nhello world\n--q1=hello\nworld\n--q2=hello world` },
    { bin: 'npm', params: ['test -- a=1,b=2,c=3'], expected: `a=1,b=2,c=3` },
    { bin: 'npx', params: ['. -- a=1,b=2,c=3'], expected: `a=1,b=2,c=3` },
  ]

  // ensure that all tests are either run or skipped
  t.plan(shells.length)

  for (const { cmd, skip, name, match } of shells) {
    t.test(name, t => {
      if (skip.reason) {
        if (skip.fail) {
          t.fail(skip.reason)
        } else {
          t.skip(skip.reason)
        }
        return t.end()
      }
      t.plan(tests.length)
      for (const { bin, params, expected } of tests) {
        if (name === 'cygwin bash' && (
          (bin === 'npm' && params[0].startsWith('test')) ||
          (bin === 'npx' && params[0].startsWith('.'))
        )) {
          t.skip("`cygwin bash` doesn't respect option `{ cwd: path }` when calling `spawnSync`")
        } else {
          matchCmd(t, cmd, bin, match, params, expected)
        }
      }
    })
  }
})
