module.exports = getUrl

function getUrl (r, forBrowser) {
  if (!r) return null
  if (/^[\w-]+\/[\w\.-]+(#[a-z0-9]*)?$/.test(r)) {
    if (forBrowser)
      r = r.replace("#", "/tree/")
    return "https://github.com/" + r
  }

  return null
}
