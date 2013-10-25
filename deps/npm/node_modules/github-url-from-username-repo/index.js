module.exports = getUrl

function getUrl (r) {
  if (!r) return
  if (/^[\w-]+\/[\w-]+$/.test(r))
    return "git://github.com/" + r
  else
    return null
}