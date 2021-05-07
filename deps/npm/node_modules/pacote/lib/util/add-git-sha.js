// add a sha to a git remote url spec
const addGitSha = (spec, sha) => {
  if (spec.hosted) {
    const h = spec.hosted
    const opt = { noCommittish: true }
    const base = h.https && h.auth ? h.https(opt) : h.shortcut(opt)

    return `${base}#${sha}`
  } else {
    // don't use new URL for this, because it doesn't handle scp urls
    return spec.rawSpec.replace(/#.*$/, '') + `#${sha}`
  }
}

module.exports = addGitSha
