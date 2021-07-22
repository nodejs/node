// module to set the appropriate log settings based on configs
// returns a boolean to say whether we should enable color on
// stdout or not.
//
// Also (and this is a really inexcusable kludge), we patch the
// log.warn() method so that when we see a peerDep override
// explanation from Arborist, we can replace the object with a
// highly abbreviated explanation of what's being overridden.
const log = require('npmlog')
const { explain } = require('./explain-eresolve.js')

module.exports = (config) => {
  const color = config.get('color')

  const { warn } = log

  const stdoutTTY = process.stdout.isTTY
  const stderrTTY = process.stderr.isTTY
  const dumbTerm = process.env.TERM === 'dumb'
  const stderrNotDumb = stderrTTY && !dumbTerm
  const enableColorStderr = color === 'always' ? true
    : color === false ? false
    : stderrTTY

  const enableColorStdout = color === 'always' ? true
    : color === false ? false
    : stdoutTTY

  log.warn = (heading, ...args) => {
    if (heading === 'ERESOLVE' && args[1] && typeof args[1] === 'object') {
      warn(heading, args[0])
      return warn('', explain(args[1], enableColorStdout, 2))
    }
    return warn(heading, ...args)
  }

  if (config.get('timing') && config.get('loglevel') === 'notice')
    log.level = 'timing'
  else
    log.level = config.get('loglevel')

  log.heading = config.get('heading') || 'npm'

  if (enableColorStderr)
    log.enableColor()
  else
    log.disableColor()

  if (config.get('unicode'))
    log.enableUnicode()
  else
    log.disableUnicode()

  // if it's more than error, don't show progress
  const quiet = log.levels[log.level] > log.levels.error

  if (config.get('progress') && stderrNotDumb && !quiet)
    log.enableProgress()
  else
    log.disableProgress()

  return enableColorStdout
}
