module.exports = getUrl

function getUrl (r) {
  if (!r) return null
  if (/^[\w-]+\/[\w\.-]+$/.test(r))
    return "git://github.com/" + r
  else
    return null
}
