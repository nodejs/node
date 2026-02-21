const ciInfo = require('ci-info')
const runScript = require('@npmcli/run-script')
const pkgJson = require('@npmcli/package-json')
const { log, output } = require('proc-log')
const noTTY = require('./no-tty.js')
const isWindowsShell = require('./is-windows.js')

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
  // escape executable path
  // necessary for preventing bash/cmd keywords from overriding
  if (!isWindowsShell) {
    if (args.length > 0) {
      args[0] = '"' + args[0] + '"'
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
