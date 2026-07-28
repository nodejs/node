// add a sha to a git remote url spec
const addGitSha = (spec, sha) => {
  if (spec.hosted) {
    const h = spec.hosted
    const opt = { noCommittish: true }
    const base = h.https && h.auth ? h.https(opt) : h.shortcut(opt)

    return `${base}#${sha}`
  } else {
    // don't use new URL for this, because it doesn't handle scp urls
    // strip the committish with indexOf/slice to avoid a regexp redos
    const hashIndex = spec.rawSpec.indexOf('#')
    const base = hashIndex === -1 ? spec.rawSpec : spec.rawSpec.slice(0, hashIndex)
    return `${base}#${sha}`
  }
}

module.exports = addGitSha
