const log = require('npmlog')
const pingUtil = require('./utils/ping.js')
const BaseCommand = require('./base-command.js')

class Ping extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'Ping npm registry'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return ['registry']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'ping'
  }

  exec (args, cb) {
    this.ping(args).then(() => cb()).catch(cb)
  }

  async ping (args) {
    log.notice('PING', this.npm.config.get('registry'))
    const start = Date.now()
    const details = await pingUtil(this.npm.flatOptions)
    const time = Date.now() - start
    log.notice('PONG', `${time}ms`)
    if (this.npm.config.get('json')) {
      this.npm.output(JSON.stringify({
        registry: this.npm.config.get('registry'),
        time,
        details,
      }, null, 2))
    } else if (Object.keys(details).length)
      log.notice('PONG', `${JSON.stringify(details, null, 2)}`)
  }
}
module.exports = Ping
