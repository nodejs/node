const log = require('npmlog')
const fetch = require('npm-registry-fetch')

const getIdentity = require('./utils/get-identity.js')

const BaseCommand = require('./base-command.js')
class Stars extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get description () {
    return 'View packages marked as favorites'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'stars'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[<user>]']
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get params () {
    return [
      'registry',
    ]
  }

  exec (args, cb) {
    this.stars(args).then(() => cb()).catch(er => {
      if (er.code === 'ENEEDAUTH')
        log.warn('stars', 'auth is required to look up your username')
      cb(er)
    })
  }

  async stars ([user]) {
    if (!user)
      user = await getIdentity(this.npm, this.npm.flatOptions)

    const { rows } = await fetch.json('/-/_view/starredByUser', {
      ...this.npm.flatOptions,
      query: { key: `"${user}"` },
    })

    if (rows.length === 0)
      log.warn('stars', 'user has not starred any packages')

    for (const row of rows)
      this.npm.output(row.value)
  }
}
module.exports = Stars
