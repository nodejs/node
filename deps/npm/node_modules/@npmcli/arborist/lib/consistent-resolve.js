// take a path and a resolved value, and turn it into a resolution from
// the given new path.  This is used with converting a package.json's
// relative file: path into one suitable for a lockfile, or between
// lockfiles, and for converting hosted git repos to a consistent url type.
const npa = require('npm-package-arg')
const relpath = require('./relpath.js')
const consistentResolve = (resolved, fromPath, toPath, relPaths = false) => {
  if (!resolved) {
    return null
  }

  try {
    const hostedOpt = { noCommittish: false }
    const {
      fetchSpec,
      saveSpec,
      type,
      hosted,
      rawSpec,
      raw,
    } = npa(resolved, fromPath)
    if (type === 'file' || type === 'directory') {
      const cleanFetchSpec = fetchSpec
      if (relPaths && toPath) {
        return `file:${relpath(toPath, cleanFetchSpec)}`
      }
      return `file:${cleanFetchSpec}`
    }
    if (hosted) {
      return `git+${hosted.auth ? hosted.https(hostedOpt) : hosted.sshurl(hostedOpt)}`
    }
    if (type === 'git') {
      return saveSpec
    }
    if (rawSpec === '*') {
      return raw
    }
    return rawSpec
  } catch (_) {
    // whatever we passed in was not acceptable to npa.
    // leave it 100% untouched.
    return resolved
  }
}
module.exports = consistentResolve
