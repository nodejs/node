const log = require('npmlog')
const pingUtil = require('./utils/ping.js')
const BaseCommand = require('./base-command.js')

class Ping extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'ping'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'ping registry'
  }

  exec (args, cb) {
    this.ping(args).then(() => cb()).catch(cb)
  }

  async ping (args) {
    log.notice('PING', this.npm.flatOptions.registry)
    const start = Date.now()
    const details = await pingUtil(this.npm.flatOptions)
    const time = Date.now() - start
    log.notice('PONG', `${time / 1000}ms`)
    if (this.npm.flatOptions.json) {
      this.npm.output(JSON.stringify({
        registry: this.npm.flatOptions.registry,
        time,
        details,
      }, null, 2))
    } else if (Object.keys(details).length)
      log.notice('PONG', `${JSON.stringify(details, null, 2)}`)
  }
}
module.exports = Ping
