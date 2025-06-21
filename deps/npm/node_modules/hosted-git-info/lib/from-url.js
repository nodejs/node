'use strict'

const parseUrl = require('./parse-url')

// look for github shorthand inputs, such as npm/cli
const isGitHubShorthand = (arg) => {
  // it cannot contain whitespace before the first #
  // it cannot start with a / because that's probably an absolute file path
  // but it must include a slash since repos are username/repository
  // it cannot start with a . because that's probably a relative file path
  // it cannot start with an @ because that's a scoped package if it passes the other tests
  // it cannot contain a : before a # because that tells us that there's a protocol
  // a second / may not exist before a #
  const firstHash = arg.indexOf('#')
  const firstSlash = arg.indexOf('/')
  const secondSlash = arg.indexOf('/', firstSlash + 1)
  const firstColon = arg.indexOf(':')
  const firstSpace = /\s/.exec(arg)
  const firstAt = arg.indexOf('@')

  const spaceOnlyAfterHash = !firstSpace || (firstHash > -1 && firstSpace.index > firstHash)
  const atOnlyAfterHash = firstAt === -1 || (firstHash > -1 && firstAt > firstHash)
  const colonOnlyAfterHash = firstColon === -1 || (firstHash > -1 && firstColon > firstHash)
  const secondSlashOnlyAfterHash = secondSlash === -1 || (firstHash > -1 && secondSlash > firstHash)
  const hasSlash = firstSlash > 0
  // if a # is found, what we really want to know is that the character
  // immediately before # is not a /
  const doesNotEndWithSlash = firstHash > -1 ? arg[firstHash - 1] !== '/' : !arg.endsWith('/')
  const doesNotStartWithDot = !arg.startsWith('.')

  return spaceOnlyAfterHash && hasSlash && doesNotEndWithSlash &&
    doesNotStartWithDot && atOnlyAfterHash && colonOnlyAfterHash &&
    secondSlashOnlyAfterHash
}

module.exports = (giturl, opts, { gitHosts, protocols }) => {
  if (!giturl) {
    return
  }

  const correctedUrl = isGitHubShorthand(giturl) ? `github:${giturl}` : giturl
  const parsed = parseUrl(correctedUrl, protocols)
  if (!parsed) {
    return
  }

  const gitHostShortcut = gitHosts.byShortcut[parsed.protocol]
  const gitHostDomain = gitHosts.byDomain[parsed.hostname.startsWith('www.')
    ? parsed.hostname.slice(4)
    : parsed.hostname]
  const gitHostName = gitHostShortcut || gitHostDomain
  if (!gitHostName) {
    return
  }

  const gitHostInfo = gitHosts[gitHostShortcut || gitHostDomain]
  let auth = null
  if (protocols[parsed.protocol]?.auth && (parsed.username || parsed.password)) {
    auth = `${parsed.username}${parsed.password ? ':' + parsed.password : ''}`
  }

  let committish = null
  let user = null
  let project = null
  let defaultRepresentation = null

  try {
    if (gitHostShortcut) {
      let pathname = parsed.pathname.startsWith('/') ? parsed.pathname.slice(1) : parsed.pathname
      const firstAt = pathname.indexOf('@')
      // we ignore auth for shortcuts, so just trim it out
      if (firstAt > -1) {
        pathname = pathname.slice(firstAt + 1)
      }

      const lastSlash = pathname.lastIndexOf('/')
      if (lastSlash > -1) {
        user = decodeURIComponent(pathname.slice(0, lastSlash))
        // we want nulls only, never empty strings
        if (!user) {
          user = null
        }
        project = decodeURIComponent(pathname.slice(lastSlash + 1))
      } else {
        project = decodeURIComponent(pathname)
      }

      if (project.endsWith('.git')) {
        project = project.slice(0, -4)
      }

      if (parsed.hash) {
        committish = decodeURIComponent(parsed.hash.slice(1))
      }

      defaultRepresentation = 'shortcut'
    } else {
      if (!gitHostInfo.protocols.includes(parsed.protocol)) {
        return
      }

      const segments = gitHostInfo.extract(parsed)
      if (!segments) {
        return
      }

      user = segments.user && decodeURIComponent(segments.user)
      project = decodeURIComponent(segments.project)
      committish = decodeURIComponent(segments.committish)
      defaultRepresentation = protocols[parsed.protocol]?.name || parsed.protocol.slice(0, -1)
    }
  } catch (err) {
    /* istanbul ignore else */
    if (err instanceof URIError) {
      return
    } else {
      throw err
    }
  }

  return [gitHostName, user, auth, project, committish, defaultRepresentation, opts]
}
