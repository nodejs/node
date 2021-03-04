const log = require('npmlog')
const fetch = require('npm-registry-fetch')

const output = require('./utils/output.js')
const getIdentity = require('./utils/get-identity.js')
const usageUtil = require('./utils/usage.js')

class Stars {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil('stars', 'npm stars [<user>]')
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
      output(row.value)
  }
}
module.exports = Stars
