/* eslint-disable max-len */

// Separated out for easier unit testing
module.exports = async (process, validateEngines) => {
  // set it here so that regardless of what happens later, we don't
  // leak any private CLI configs to other programs
  process.title = 'npm'

  // if npm is called as "npmg" or "npm_g", then run in global mode.
  if (process.argv[1][process.argv[1].length - 1] === 'g') {
    process.argv.splice(1, 1, 'npm', '-g')
  }

  const satisfies = require('semver/functions/satisfies')
  const exitHandler = require('./utils/exit-handler.js')
  const Npm = require('./npm.js')
  const npm = new Npm()
  exitHandler.setNpm(npm)

  // only log node and npm paths in argv initially since argv can contain sensitive info. a cleaned version will be logged later
  const log = require('./utils/log-shim.js')
  log.verbose('cli', process.argv.slice(0, 2).join(' '))
  log.info('using', 'npm@%s', npm.version)
  log.info('using', 'node@%s', process.version)

  // At this point we've required a few files and can be pretty sure we dont contain invalid syntax for this version of node. It's possible a lazy require would, but that's unlikely enough that it's not worth catching anymore and we attach the more important exit handlers.
  validateEngines.off()
  process.on('uncaughtException', exitHandler)
  process.on('unhandledRejection', exitHandler)

  // It is now safe to log a warning if they are using a version of node that is not going to fail on syntax errors but is still unsupported and untested and might not work reliably. This is safe to use the logger now which we want since this will show up in the error log too.
  if (!satisfies(validateEngines.node, validateEngines.engines)) {
    log.warn('cli', validateEngines.unsupportedMessage)
  }

  let cmd
  // Now actually fire up npm and run the command.
  // This is how to use npm programmatically:
  try {
    await npm.load()

    // npm -v
    if (npm.config.get('version', 'cli')) {
      npm.output(npm.version)
      return exitHandler()
    }

    // npm --versions
    if (npm.config.get('versions', 'cli')) {
      npm.argv = ['version']
      npm.config.set('usage', false, 'cli')
    }

    cmd = npm.argv.shift()
    if (!cmd) {
      npm.output(npm.usage)
      process.exitCode = 1
      return exitHandler()
    }

    await npm.exec(cmd)
    return exitHandler()
  } catch (err) {
    if (err.code === 'EUNKNOWNCOMMAND') {
      const didYouMean = require('./utils/did-you-mean.js')
      const suggestions = await didYouMean(npm.localPrefix, cmd)
      npm.output(`Unknown command: "${cmd}"${suggestions}\n`)
      npm.output('To see a list of supported npm commands, run:\n  npm help')
      process.exitCode = 1
      return exitHandler()
    }
    return exitHandler(err)
  }
}
