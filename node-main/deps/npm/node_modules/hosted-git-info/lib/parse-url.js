const url = require('url')

const lastIndexOfBefore = (str, char, beforeChar) => {
  const startPosition = str.indexOf(beforeChar)
  return str.lastIndexOf(char, startPosition > -1 ? startPosition : Infinity)
}

const safeUrl = (u) => {
  try {
    return new url.URL(u)
  } catch {
    // this fn should never throw
  }
}

// accepts input like git:github.com:user/repo and inserts the // after the first :
const correctProtocol = (arg, protocols) => {
  const firstColon = arg.indexOf(':')
  const proto = arg.slice(0, firstColon + 1)
  if (Object.prototype.hasOwnProperty.call(protocols, proto)) {
    return arg
  }

  if (arg.substr(firstColon, 3) === '://') {
    // If arg is given as <foo>://<bar>, then this is already a valid URL.
    return arg
  }

  const firstAt = arg.indexOf('@')
  if (firstAt > -1) {
    if (firstAt > firstColon) {
      // URL has the form of <foo>:<bar>@<baz>. Assume this is a git+ssh URL.
      return `git+ssh://${arg}`
    } else {
      // URL has the form 'git@github.com:npm/hosted-git-info.git'.
      return arg
    }
  }

  // Correct <foo>:<bar> to <foo>://<bar>
  return `${arg.slice(0, firstColon + 1)}//${arg.slice(firstColon + 1)}`
}

// attempt to correct an scp style url so that it will parse with `new URL()`
const correctUrl = (giturl) => {
  // ignore @ that come after the first hash since the denotes the start
  // of a committish which can contain @ characters
  const firstAt = lastIndexOfBefore(giturl, '@', '#')
  // ignore colons that come after the hash since that could include colons such as:
  // git@github.com:user/package-2#semver:^1.0.0
  const lastColonBeforeHash = lastIndexOfBefore(giturl, ':', '#')

  if (lastColonBeforeHash > firstAt) {
    // the last : comes after the first @ (or there is no @)
    // like it would in:
    // proto://hostname.com:user/repo
    // username@hostname.com:user/repo
    // :password@hostname.com:user/repo
    // username:password@hostname.com:user/repo
    // proto://username@hostname.com:user/repo
    // proto://:password@hostname.com:user/repo
    // proto://username:password@hostname.com:user/repo
    // then we replace the last : with a / to create a valid path
    giturl = giturl.slice(0, lastColonBeforeHash) + '/' + giturl.slice(lastColonBeforeHash + 1)
  }

  if (lastIndexOfBefore(giturl, ':', '#') === -1 && giturl.indexOf('//') === -1) {
    // we have no : at all
    // as it would be in:
    // username@hostname.com/user/repo
    // then we prepend a protocol
    giturl = `git+ssh://${giturl}`
  }

  return giturl
}

module.exports = (giturl, protocols) => {
  const withProtocol = protocols ? correctProtocol(giturl, protocols) : giturl
  return safeUrl(withProtocol) || safeUrl(correctUrl(withProtocol))
}
