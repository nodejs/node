const npmFetch = require('npm-registry-fetch')

const needsAuthError = (msg) =>
  Object.assign(new Error(msg), { code: 'ENEEDAUTH' })

module.exports = async (npm, opts = {}) => {
  const { registry } = opts
  if (!registry)
    throw Object.assign(new Error('No registry specified.'), { code: 'ENOREGISTRY' })

  // First, check if we have a user/pass-based auth
  const creds = npm.config.getCredentialsByURI(registry)
  const { username: usernameFromURI, token } = creds

  if (usernameFromURI) {
    // Found username; return it
    return usernameFromURI
  } else if (token) {
    // No username, but we have a token; fetch the username from registry
    const registryData = await npmFetch.json('/-/whoami', {
      ...opts,
    })
    const { username: usernameFromRegistry } = registryData
    // Retrieved username from registry; return it
    if (usernameFromRegistry)
      return usernameFromRegistry
    else {
      // Didn't get username from registry; bad token
      throw needsAuthError(
        'Your auth token is no longer valid. Please login again.'
      )
    }
  } else {
    // At this point, if they have a credentials object, it doesn't have a
    // token or auth in it. Probably just the default registry.
    throw needsAuthError('This command requires you to be logged in.')
  }
}
