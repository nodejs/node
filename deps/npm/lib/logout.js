const log = require('npmlog')
const getAuth = require('npm-registry-fetch/auth.js')
const npmFetch = require('npm-registry-fetch')
const BaseCommand = require('./base-command.js')

class Logout extends BaseCommand {
  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get name () {
    return 'logout'
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  static get usage () {
    return ['[--registry=<url>] [--scope=<@scope>]']
  }

  exec (args, cb) {
    this.logout(args).then(() => cb()).catch(cb)
  }

  async logout (args) {
    const { registry, scope } = this.npm.flatOptions
    const regRef = scope ? `${scope}:registry` : 'registry'
    const reg = this.npm.flatOptions[regRef] || registry

    const auth = getAuth(reg, this.npm.flatOptions)

    if (auth.token) {
      log.verbose('logout', `clearing token for ${reg}`)
      await npmFetch(`/-/user/token/${encodeURIComponent(auth.token)}`, {
        ...this.npm.flatOptions,
        method: 'DELETE',
        ignoreBody: true,
      })
    } else if (auth.username || auth.password)
      log.verbose('logout', `clearing user credentials for ${reg}`)
    else {
      const msg = `not logged in to ${reg}, so can't log out!`
      throw Object.assign(new Error(msg), { code: 'ENEEDAUTH' })
    }

    if (scope)
      this.npm.config.delete(regRef, 'user')

    this.npm.config.clearCredentialsByURI(reg)

    await this.npm.config.save('user')
  }
}
module.exports = Logout
