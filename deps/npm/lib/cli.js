// Separated out for easier unit testing
module.exports = async (process) => {
  // set it here so that regardless of what happens later, we don't
  // leak any private CLI configs to other programs
  process.title = 'npm'

  const {
    checkForBrokenNode,
    checkForUnsupportedNode,
  } = require('../lib/utils/unsupported.js')

  checkForBrokenNode()

  const log = require('npmlog')
  // pause it here so it can unpause when we've loaded the configs
  // and know what loglevel we should be printing.
  log.pause()

  checkForUnsupportedNode()

  const npm = require('../lib/npm.js')
  const exitHandler = require('../lib/utils/exit-handler.js')
  exitHandler.setNpm(npm)

  // if npm is called as "npmg" or "npm_g", then
  // run in global mode.
  if (process.argv[1][process.argv[1].length - 1] === 'g')
    process.argv.splice(1, 1, 'npm', '-g')

  log.verbose('cli', process.argv)

  log.info('using', 'npm@%s', npm.version)
  log.info('using', 'node@%s', process.version)

  process.on('uncaughtException', exitHandler)
  process.on('unhandledRejection', exitHandler)

  const updateNotifier = require('../lib/utils/update-notifier.js')

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

    updateNotifier(npm)

    const cmd = npm.argv.shift()
    if (!cmd) {
      npm.output(npm.usage)
      process.exitCode = 1
      return exitHandler()
    }

    const impl = npm.commands[cmd]
    if (!impl) {
      const didYouMean = require('./utils/did-you-mean.js')
      const suggestions = await didYouMean(npm, npm.localPrefix, cmd)
      npm.output(`Unknown command: "${cmd}"${suggestions}\n\nTo see a list of supported npm commands, run:\n  npm help`)
      process.exitCode = 1
      return exitHandler()
    }

    impl(npm.argv, exitHandler)
  } catch (err) {
    return exitHandler(err)
  }
}
