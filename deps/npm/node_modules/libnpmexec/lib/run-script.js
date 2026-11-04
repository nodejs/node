const ciInfo = require('ci-info')
const runScript = require('@npmcli/run-script')
const pkgJson = require('@npmcli/package-json')
const { log, output } = require('proc-log')
const noTTY = require('./no-tty.js')
const isWindows = require('./is-windows.js')

const isCmd = shell => /(?:^|[\\/])cmd(?:\.exe)?$/i.test(shell)
const isPowerShell = shell => /(?:^|[\\/])(?:powershell|pwsh)(?:\.exe)?$/i.test(shell)

const run = async ({
  args,
  call,
  flatOptions,
  locationMsg,
  path,
  binPaths,
  runPath,
  scriptShell,
}) => {
  if (!call && args.length > 0) {
    const shell = scriptShell || (isWindows ? process.env.ComSpec || 'cmd' : 'sh')
    if (isCmd(shell)) {
      // Variable expansion (including delayed expansion) and embedded quotes
      // cannot be safely escaped here. Control characters are invalid filenames.
      if (/["%!]/.test(args[0]) || [...args[0]].some(c => c.charCodeAt(0) < 32)) {
        throw Object.assign(
          new Error(`Invalid executable name for cmd.exe: ${JSON.stringify(args[0])}`),
          { code: 'EINVALIDCOMMAND' }
        )
      }
      // Protect both cmd.exe's metacharacter parsing and executable-name parsing.
      args[0] = `"${args[0]}"`.replace(/[ ^&()<>|";,*?=@]/g, '^$&')
    } else if (isPowerShell(shell)) {
      // Encode the whole invocation to avoid promise-spawn's POSIX argument escaping.
      const quotedArgs = args.map(arg => `'${arg.replace(/['\u2018-\u201b]/g, '$&$&')}'`)
      args = [`& ${quotedArgs.join(' ')}`]
    } else {
      // single-quote so shell metacharacters in the executable name are taken
      // literally; double quotes still expand $(), backticks, $var and "
      args[0] = `'${args[0].replace(/'/g, `'\\''`)}'`
    }
  }

  // turn list of args into command string
  const script = call || args.shift() || scriptShell

  // do the fakey runScript dance
  // still should work if no package.json in cwd
  const { content: realPkg } = await pkgJson.normalize(path, { steps: [
    'binDir',
    ...pkgJson.normalizeSteps,
  ] }).catch(() => ({ content: {} }))
  const pkg = {
    ...realPkg,
    scripts: {
      ...(realPkg.scripts || {}),
      npx: script,
    },
  }

  if (script === scriptShell) {
    if (!noTTY()) {
      if (ciInfo.isCI) {
        return log.warn('exec', 'Interactive mode disabled in CI environment')
      }

      const { chalk } = flatOptions

      output.standard(`${
        chalk.reset('\nEntering npm script environment')
      }${
        chalk.reset(locationMsg || ` at location:\n${chalk.dim(runPath)}`)
      }${
        chalk.bold('\nType \'exit\' or ^D when finished\n')
      }`)
    }
  }
  return runScript({
    ...flatOptions,
    pkg,
    // we always run in cwd, not --prefix
    path: runPath,
    binPaths,
    event: 'npx',
    args,
    stdio: 'inherit',
    scriptShell,
  })
}

module.exports = run
