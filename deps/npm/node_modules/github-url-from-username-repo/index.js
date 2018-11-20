module.exports = getUrl

function getUrl (r, forBrowser) {
  if (!r) return null
  // Regex taken from https://github.com/npm/npm-package-arg/commit/01dce583c64afae07b66a2a8a6033aeba871c3cd
  // Note: This does not fully test the git ref format.
  // See https://www.kernel.org/pub/software/scm/git/docs/git-check-ref-format.html
  //
  // The only way to do this properly would be to shell out to
  // git-check-ref-format, and as this is a fast sync function,
  // we don't want to do that. Just let git fail if it turns
  // out that the commit-ish is invalid.
  // GH usernames cannot start with . or -
  if (/^[^@%\/\s\.-][^:@%\/\s]*\/[^@\s\/%]+(?:#.*)?$/.test(r)) {
    if (forBrowser)
      r = r.replace("#", "/tree/")
    return "https://github.com/" + r
  }

  return null
}
