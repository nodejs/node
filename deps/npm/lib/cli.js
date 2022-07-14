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
  const semver = require('semver')
  const supported = require('../package.json').engines.node
  const knownBroken = '<12.5.0'

  const nodejsVersion = process.version.replace(/-.*$/, '')
  /* eslint-disable no-console */
  if (semver.satisfies(nodejsVersion, knownBroken)) {
    console.error('ERROR: npm is known not to run on Node.js ' + process.version)
    console.error("You'll need to upgrade to a newer Node.js version in order to use this")
    console.error('version of npm. You can find the latest version at https://nodejs.org/')
    process.exit(1)
  }
  if (!semver.satisfies(nodejsVersion, supported)) {
    console.error('npm does not support Node.js ' + process.version)
    console.error('You should probably upgrade to a newer version of node as we')
    console.error("can't make any promises that npm will work with this version.")
    console.error('You can find the latest version at https://nodejs.org/')
  }
  /* eslint-enable no-console */

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
  // only log node and npm paths in argv initially since argv can contain
  // sensitive info. a cleaned version will be logged later
  log.verbose('cli', process.argv.slice(0, 2).join(' '))
  log.info('using', 'npm@%s', npm.version)
  log.info('using', 'node@%s', process.version)

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
