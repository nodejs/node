const t = require('tap')
const spawn = require('@npmcli/promise-spawn')
const { spawnSync } = require('child_process')
const { resolve, join } = require('path')
const { readFileSync, chmodSync } = require('fs')
const Diff = require('diff')
const { version } = require('../../package.json')

const root = resolve(__dirname, '../..')
const npmShim = join(root, 'bin/npm')
const npxShim = join(root, 'bin/npx')

t.test('npm vs npx', t => {
  // these scripts should be kept in sync so this tests the contents of each
  // and does a diff to ensure the only differences between them are necessary
  const diffFiles = (ext = '') => Diff.diffChars(
    readFileSync(`${npmShim}${ext}`, 'utf8'),
    readFileSync(`${npxShim}${ext}`, 'utf8')
  ).filter(v => v.added || v.removed).map((v, i) => i === 0 ? v.value : v.value.toUpperCase())

  t.test('bash', t => {
    const [npxCli, ...changes] = diffFiles()
    const npxCliLine = npxCli.split('\n').reverse().join('')
    t.match(npxCliLine, /^NPX_CLI_JS=/, 'has NPX_CLI')
    t.equal(changes.length, 20)
    t.strictSame([...new Set(changes)], ['M', 'X'], 'all other changes are m->x')
    t.end()
  })

  t.test('cmd', t => {
    const [npxCli, ...changes] = diffFiles('.cmd')
    t.match(npxCli, /^SET "NPX_CLI_JS=/, 'has NPX_CLI')
    t.equal(changes.length, 12)
    t.strictSame([...new Set(changes)], ['M', 'X'], 'all other changes are m->x')
    t.end()
  })

  t.end()
})

t.test('basic', async t => {
  if (process.platform !== 'win32') {
    t.comment('test only relevant on windows')
    return
  }

  const path = t.testdir({
    'node.exe': readFileSync(process.execPath),
    npm: readFileSync(npmShim),
    npx: readFileSync(npxShim),
    // simulate the state where one version of npm is installed
    // with node, but we should load the globally installed one
    'global-prefix': {
      node_modules: {
        npm: t.fixture('symlink', root),
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

  chmodSync(join(path, 'npm'), 0o755)
  chmodSync(join(path, 'npx'), 0o755)

  const { ProgramFiles, SystemRoot, NYC_CONFIG } = process.env
  const gitBash = join(ProgramFiles, 'Git', 'bin', 'bash.exe')
  const gitUsrBinBash = join(ProgramFiles, 'Git', 'usr', 'bin', 'bash.exe')
  const wslBash = join(SystemRoot, 'System32', 'bash.exe')
  const cygwinBash = join(SystemRoot, '/', 'cygwin64', 'bin', 'bash.exe')

  const bashes = Object.entries({
    'wsl bash': wslBash,
    'git bash': gitBash,
    'git internal bash': gitUsrBinBash,
    'cygwin bash': cygwinBash,
  }).map(([name, bash]) => {
    let skip
    if (bash === cygwinBash && NYC_CONFIG) {
      skip = 'does not play nicely with NYC, run without coverage'
    } else {
      try {
        // If WSL is installed, it *has* a bash.exe, but it fails if
        // there is no distro installed, so we need to detect that.
        if (spawnSync(bash, ['-l', '-c', 'exit 0']).status !== 0) {
          throw new Error('not installed')
        }
      } catch {
        skip = 'not installed'
      }
    }
    return { name, bash, skip }
  })

  for (const { name, bash, skip } of bashes) {
    if (skip) {
      t.skip(name, { diagnostic: true, bin: bash, reason: skip })
      continue
    }

    await t.test(name, async t => {
      const bins = Object.entries({
        // should have loaded this instance of npm we symlinked in
        npm: [['help'], `npm@${version} ${root}`],
        npx: [['--version'], version],
      })

      for (const [binName, [cmdArgs, stdout]] of bins) {
        await t.test(binName, async t => {
          // only cygwin *requires* the -l, but the others are ok with it
          const args = ['-l', binName, ...cmdArgs]
          const result = await spawn(bash, args, {
            // don't hit the registry for the update check
            env: { PATH: path, npm_config_update_notifier: 'false' },
            cwd: path,
          })
          t.match(result, {
            cmd: bash,
            args: args,
            code: 0,
            signal: null,
            stderr: String,
            stdout,
          })
        })
      }
    })
  }
})
