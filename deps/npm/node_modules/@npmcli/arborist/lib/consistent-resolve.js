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
    const isPath = type === 'file' || type === 'directory'
    return isPath && !relPaths ? `file:${fetchSpec}`
      : isPath ? 'file:' + (toPath ? relpath(toPath, fetchSpec) : fetchSpec)
      : hosted ? `git+${
        hosted.auth ? hosted.https(hostedOpt) : hosted.sshurl(hostedOpt)
      }`
      : type === 'git' ? saveSpec
      // always return something.  'foo' is interpreted as 'foo@' otherwise.
      : rawSpec === '' && raw.slice(-1) !== '@' ? raw
      // just strip off the name, but otherwise return as-is
      : rawSpec
  } catch (_) {
    // whatever we passed in was not acceptable to npa.
    // leave it 100% untouched.
    return resolved
  }
}
module.exports = consistentResolve
