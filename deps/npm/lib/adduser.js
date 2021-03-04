const log = require('npmlog')
const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')
const replaceInfo = require('./utils/replace-info.js')
const authTypes = {
  legacy: require('./auth/legacy.js'),
  oauth: require('./auth/oauth.js'),
  saml: require('./auth/saml.js'),
  sso: require('./auth/sso.js'),
}

class AddUser {
  constructor (npm) {
    this.npm = npm
  }

  /* istanbul ignore next - see test/lib/load-all-commands.js */
  get usage () {
    return usageUtil(
      'adduser',
      'npm adduser [--registry=url] [--scope=@orgname] [--always-auth]'
    )
  }

  exec (args, cb) {
    this.adduser(args).then(() => cb()).catch(cb)
  }

  async adduser (args) {
    const { scope } = this.npm.flatOptions
    const registry = this.getRegistry(this.npm.flatOptions)
    const auth = this.getAuthType(this.npm.flatOptions)
    const creds = this.npm.config.getCredentialsByURI(registry)

    log.disableProgress()

    log.notice('', `Log in on ${replaceInfo(registry)}`)

    const { message, newCreds } = await auth(this.npm, {
      ...this.npm.flatOptions,
      creds,
      registry,
      scope,
    })

    await this.updateConfig({
      newCreds,
      registry,
      scope,
    })

    output(message)
  }

  getRegistry ({ scope, registry }) {
    if (scope) {
      const scopedRegistry = this.npm.config.get(`${scope}:registry`)
      const cliRegistry = this.npm.config.get('registry', 'cli')
      if (scopedRegistry && !cliRegistry)
        return scopedRegistry
    }
    return registry
  }

  getAuthType ({ authType }) {
    const type = authTypes[authType]

    if (!type)
      throw new Error('no such auth module')

    return type
  }

  async updateConfig ({ newCreds, registry, scope }) {
    this.npm.config.delete('_token', 'user') // prevent legacy pollution

    if (scope)
      this.npm.config.set(scope + ':registry', registry, 'user')

    this.npm.config.setCredentialsByURI(registry, newCreds)
    await this.npm.config.save('user')
  }
}
module.exports = AddUser
