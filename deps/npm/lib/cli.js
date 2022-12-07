// This is separate to indicate that it should contain code we expect to work in
// all conceivably runnable versions of node.  This is a best effort to catch
// syntax errors to give users a good error message if they are using a node
// version that doesn't allow syntax we are using such as private properties, etc
const createEnginesValidation = () => {
  const node = process.version.replace(/-.*$/, '')
  const pkg = require('../package.json')
  const engines = pkg.engines.node
  const npm = `v${pkg.version}`

  const cols = Math.min(Math.max(20, process.stdout.columns) || 80, 80)
  const wrap = (lines) => lines
    .join(' ')
    .split(/[ \n]+/)
    .reduce((left, right) => {
      const last = left.split('\n').pop()
      const join = last.length && last.length + right.length > cols ? '\n' : ' '
      return left + join + right
    })
    .trim()

  const unsupportedMessage = wrap([
    `npm ${npm} does not support Node.js ${node}.`,
    `You should probably upgrade to a newer version of node as we can't make any`,
    `promises that npm will work with this version.`,
    `This version of npm supports the following node versions: \`${engines}\`.`,
    'You can find the latest version at https://nodejs.org/.',
  ])

  const brokenMessage = wrap([
    `ERROR: npm ${npm} is known not to run on Node.js ${node}.`,
    `You'll need to upgrade to a newer Node.js version in order to use this version of npm.`,
    `This version of npm supports the following node versions: \`${engines}\`.`,
    'You can find the latest version at https://nodejs.org/.',
  ])

  // coverage ignored because this is only hit in very unsupported node versions
  // and it's a best effort attempt to show something nice in those cases
  /* istanbul ignore next */
  const syntaxErrorHandler = (err) => {
    if (err instanceof SyntaxError) {
      // eslint-disable-next-line no-console
      console.error(`${brokenMessage}\n\nERROR:`)
      // eslint-disable-next-line no-console
      console.error(err)
      return process.exit(1)
    }
    throw err
  }

  process.on('uncaughtException', syntaxErrorHandler)
  process.on('unhandledRejection', syntaxErrorHandler)

  return {
    node,
    engines,
    unsupportedMessage,
    off: () => {
      process.off('uncaughtException', syntaxErrorHandler)
      process.off('unhandledRejection', syntaxErrorHandler)
    },
  }
}

// Separated out for easier unit testing
module.exports = async process => {
  // set it here so that regardless of what happens later, we don't
  // leak any private CLI configs to other programs
  process.title = 'npm'

  // Nothing should happen before this line if we can't guarantee it will
  // not have syntax errors in some version of node
  const validateEngines = createEnginesValidation()

  const satisfies = require('semver/functions/satisfies')
  const exitHandler = require('./utils/exit-handler.js')
  const Npm = require('./npm.js')
  const npm = new Npm()
  exitHandler.setNpm(npm)

  // if npm is called as "npmg" or "npm_g", then run in global mode.
  if (process.argv[1][process.argv[1].length - 1] === 'g') {
    process.argv.splice(1, 1, 'npm', '-g')
  }

  // only log node and npm paths in argv initially since argv can contain
  // sensitive info. a cleaned version will be logged later
  const log = require('./utils/log-shim.js')
  log.verbose('cli', process.argv.slice(0, 2).join(' '))
  log.info('using', 'npm@%s', npm.version)
  log.info('using', 'node@%s', process.version)

  // At this point we've required a few files and can be pretty sure
  // we dont contain invalid syntax for this version of node. It's
  // possible a lazy require would, but that's unlikely enough that
  // it's not worth catching anymore and we attach the more important
  // exit handlers.
  validateEngines.off()
  process.on('uncaughtException', exitHandler)
  process.on('unhandledRejection', exitHandler)

  // It is now safe to log a warning if they are using a version of node
  // that is not going to fail on syntax errors but is still unsupported
  // and untested and might not work reliably. This is safe to use the logger
  // now which we want since this will show up in the error log too.
  if (!satisfies(validateEngines.node, validateEngines.engines)) {
    log.warn('cli', validateEngines.unsupportedMessage)
  }

  let cmd
  // now actually fire up npm and run the command.
  // this is how to use npm programmatically:
  try {
    await npm.load()
    if (npm.config.get('version', 'cli')) {
      npm.output(npm.version)
      return exitHandler()
    }

    // npm --versions=cli
    if (npm.config.get('versions', 'cli')) {
      npm.argv = ['version']
      npm.config.set('usage', false, 'cli')
    }

    cmd = npm.argv.shift()
    if (!cmd) {
      npm.output(await npm.usage)
      process.exitCode = 1
      return exitHandler()
    }

    await npm.exec(cmd, npm.argv)
    return exitHandler()
  } catch (err) {
    if (err.code === 'EUNKNOWNCOMMAND') {
      const didYouMean = require('./utils/did-you-mean.js')
      const suggestions = await didYouMean(npm, npm.localPrefix, cmd)
      npm.output(`Unknown command: "${cmd}"${suggestions}\n`)
      npm.output('To see a list of supported npm commands, run:\n  npm help')
      process.exitCode = 1
      return exitHandler()
    }
    return exitHandler(err)
  }
}
