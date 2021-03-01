const log = require('npmlog')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')

const usage = usageUtil('ping', 'npm ping\nping registry')

const cmd = (args, cb) => ping(args).then(() => cb()).catch(cb)
const pingUtil = require('./utils/ping.js')

const ping = async args => {
  log.notice('PING', npm.flatOptions.registry)
  const start = Date.now()
  const details = await pingUtil(npm.flatOptions)
  const time = Date.now() - start
  log.notice('PONG', `${time / 1000}ms`)
  if (npm.flatOptions.json) {
    output(JSON.stringify({
      registry: npm.flatOptions.registry,
      time,
      details,
    }, null, 2))
  } else if (Object.keys(details).length)
    log.notice('PONG', `${JSON.stringify(details, null, 2)}`)
}

module.exports = Object.assign(cmd, { usage })
