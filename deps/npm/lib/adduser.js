const log = require('npmlog')
const npm = require('./npm.js')
const output = require('./utils/output.js')
const usageUtil = require('./utils/usage.js')
const replaceInfo = require('./utils/replace-info.js')
const authTypes = {
  legacy: require('./auth/legacy.js'),
  oauth: require('./auth/oauth.js'),
  saml: require('./auth/saml.js'),
  sso: require('./auth/sso.js'),
}

const usage = usageUtil(
  'adduser',
  'npm adduser [--registry=url] [--scope=@orgname] [--always-auth]'
)

const cmd = (args, cb) => adduser(args).then(() => cb()).catch(cb)

const getRegistry = ({ scope, registry }) => {
  if (scope) {
    const scopedRegistry = npm.config.get(`${scope}:registry`)
    const cliRegistry = npm.config.get('registry', 'cli')
    if (scopedRegistry && !cliRegistry)
      return scopedRegistry
  }
  return registry
}

const getAuthType = ({ authType }) => {
  const type = authTypes[authType]

  if (!type)
    throw new Error('no such auth module')

  return type
}

const updateConfig = async ({ newCreds, registry, scope }) => {
  npm.config.delete('_token', 'user') // prevent legacy pollution

  if (scope)
    npm.config.set(scope + ':registry', registry, 'user')

  npm.config.setCredentialsByURI(registry, newCreds)
  await npm.config.save('user')
}

const adduser = async (args) => {
  const { scope } = npm.flatOptions
  const registry = getRegistry(npm.flatOptions)
  const auth = getAuthType(npm.flatOptions)
  const creds = npm.config.getCredentialsByURI(registry)

  log.disableProgress()

  log.notice('', `Log in on ${replaceInfo(registry)}`)

  const { message, newCreds } = await auth({
    ...npm.flatOptions,
    creds,
    registry,
    scope,
  })

  await updateConfig({
    newCreds,
    registry,
    scope,
  })

  output(message)
}

module.exports = Object.assign(cmd, { usage })
