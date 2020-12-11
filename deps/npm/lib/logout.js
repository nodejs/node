const eu = encodeURIComponent
const log = require('npmlog')
const getAuth = require('npm-registry-fetch/auth.js')
const npmFetch = require('npm-registry-fetch')
const npm = require('./npm.js')
const usageUtil = require('./utils/usage.js')
const completion = require('./utils/completion/none.js')

const usage = usageUtil(
  'logout',
  'npm logout [--registry=<url>] [--scope=<@scope>]'
)

const cmd = (args, cb) => logout(args).then(() => cb()).catch(cb)

const logout = async (args) => {
  const { registry, scope } = npm.flatOptions
  const regRef = scope ? `${scope}:registry` : 'registry'
  const reg = npm.flatOptions[regRef] || registry

  const auth = getAuth(reg, npm.flatOptions)

  if (auth.token) {
    log.verbose('logout', `clearing token for ${reg}`)
    await npmFetch(`/-/user/token/${eu(auth.token)}`, {
      ...npm.flatOptions,
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
    npm.config.delete(regRef, 'user')

  npm.config.clearCredentialsByURI(reg)

  await npm.config.save('user')
}

module.exports = Object.assign(cmd, { completion, usage })
