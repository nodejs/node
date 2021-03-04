const t = require('tap')

if (process.platform !== 'win32') {
  t.plan(0, 'test only relevant on windows')
  process.exit(0)
}

const has = path => {
  try {
    // If WSL is installed, it *has* a bash.exe, but it fails if
    // there is no distro installed, so we need to detect that.
    const result = spawnSync(path, ['-l', '-c', 'exit 0'])
    if (result.status === 0)
      return true
    else {
      // print whatever error we got
      throw result.error || Object.assign(new Error(String(result.stderr)), {
        code: result.status,
      })
    }
  } catch (er) {
    t.comment(`not installed: ${path}`, er)
    return false
  }
}

const { version } = require('../../package.json')
const spawn = require('@npmcli/promise-spawn')
const { spawnSync } = require('child_process')
const { resolve } = require('path')
const { ProgramFiles, SystemRoot } = process.env
const { readFileSync, chmodSync } = require('fs')
const gitBash = resolve(ProgramFiles, 'Git', 'bin', 'bash.exe')
const gitUsrBinBash = resolve(ProgramFiles, 'Git', 'usr', 'bin', 'bash.exe')
const wslBash = resolve(SystemRoot, 'System32', 'bash.exe')
const cygwinBash = resolve(SystemRoot, '/', 'cygwin64', 'bin', 'bash.exe')

const bashes = Object.entries({
  'wsl bash': wslBash,
  'git bash': gitBash,
  'git internal bash': gitUsrBinBash,
  'cygwin bash': cygwinBash,
})

const npmShim = resolve(__dirname, '../../bin/npm')
const npxShim = resolve(__dirname, '../../bin/npx')

const path = t.testdir({
  'node.exe': readFileSync(process.execPath),
  npm: readFileSync(npmShim),
  npx: readFileSync(npxShim),
  // simulate the state where one version of npm is installed
  // with node, but we should load the globally installed one
  'global-prefix': {
    node_modules: {
      npm: t.fixture('symlink', resolve(__dirname, '../..')),
    },
  },
  // put in a shim that ONLY prints the intended global prefix,
  // and should not be used for anything else.
  node_modules: {
    npm: {
      bin: {
        'npx-cli.js': `
          throw new Error('this should not be called')
        `,
        'npm-cli.js': `
          const assert = require('assert')
          const args = process.argv.slice(2)
          assert.equal(args[0], 'prefix')
          assert.equal(args[1], '-g')
          const { resolve } = require('path')
          console.log(resolve(__dirname, '../../../global-prefix'))
        `,
      },
    },
  },
})
chmodSync(resolve(path, 'npm'), 0o755)
chmodSync(resolve(path, 'npx'), 0o755)

for (const [name, bash] of bashes) {
  if (!has(bash)) {
    t.skip(`${name} not installed`, { bin: bash, diagnostic: true })
    continue
  }

  if (bash === cygwinBash && process.env.NYC_CONFIG) {
    t.skip('Cygwin does not play nicely with NYC, run without coverage')
    continue
  }

  t.test(name, async t => {
    t.plan(2)
    t.test('npm', async t => {
      // only cygwin *requires* the -l, but the others are ok with it
      // don't hit the registry for the update check
      const args = ['-l', 'npm', 'help']

      const result = await spawn(bash, args, {
        env: { PATH: path, npm_config_update_notifier: 'false' },
        cwd: path,
        stdioString: true,
      })
      t.match(result, {
        cmd: bash,
        args: ['-l', 'npm', 'help'],
        code: 0,
        signal: null,
        stderr: String,
        // should have loaded this instance of npm we symlinked in
        stdout: `npm@${version} ${resolve(__dirname, '../..')}`,
      })
    })

    t.test('npx', async t => {
      const args = ['-l', 'npx', '--version']

      const result = await spawn(bash, args, {
        env: { PATH: path, npm_config_update_notifier: 'false' },
        cwd: path,
        stdioString: true,
      })
      t.match(result, {
        cmd: bash,
        args: ['-l', 'npx', '--version'],
        code: 0,
        signal: null,
        stderr: String,
        // should have loaded this instance of npm we symlinked in
        stdout: version,
      })
    })
  })
}
