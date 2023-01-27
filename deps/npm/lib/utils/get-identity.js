const npmFetch = require('npm-registry-fetch')

module.exports = async (npm, opts) => {
  const { registry } = opts

  // First, check if we have a user/pass-based auth
  const creds = npm.config.getCredentialsByURI(registry)
  if (creds.username) {
    return creds.username
  }

  // No username, but we have other credentials; fetch the username from registry
  if (creds.token || creds.certfile && creds.keyfile) {
    const registryData = await npmFetch.json('/-/whoami', { ...opts })
    if (typeof registryData?.username === 'string') {
      return registryData.username
    }
  }

  // At this point, even if they have a credentials object, it doesn't have a
  // valid token.
  throw Object.assign(
    new Error('This command requires you to be logged in.'),
    { code: 'ENEEDAUTH' }
  )
}
