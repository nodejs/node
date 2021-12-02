// Separated out for easier unit testing
module.exports = async process => {
  // set it here so that regardless of what happens later, we don't
  // leak any private CLI configs to other programs
  process.title = 'npm'

  // We used to differentiate between known broken and unsupported
  // versions of node and attempt to only log unsupported but still run.
  // After we dropped node 10 support, we can use new features
  // (like static, private, etc) which will only give vague syntax errors,
  // so now both broken and unsupported use console, but only broken
  // will process.exit. It is important to now perform *both* of these
  // checks as early as possible so the user gets the error message.
  const { checkForBrokenNode, checkForUnsupportedNode } = require('./utils/unsupported.js')
  checkForBrokenNode()
  checkForUnsupportedNode()

  const exitHandler = require('./utils/exit-handler.js')
  process.on('uncaughtException', exitHandler)
  process.on('unhandledRejection', exitHandler)

  const Npm = require('./npm.js')
  const npm = new Npm()
  exitHandler.setNpm(npm)

  // if npm is called as "npmg" or "npm_g", then
  // run in global mode.
  if (process.argv[1][process.argv[1].length - 1] === 'g') {
    process.argv.splice(1, 1, 'npm', '-g')
  }

  const log = require('./utils/log-shim.js')
  const replaceInfo = require('./utils/replace-info.js')
  log.verbose('cli', replaceInfo(process.argv))

  log.info('using', 'npm@%s', npm.version)
  log.info('using', 'node@%s', process.version)

  const updateNotifier = require('./utils/update-notifier.js')

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

    updateNotifier(npm)

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
