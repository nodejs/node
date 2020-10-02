// given a manifest, try to get the hosted git info from it based on
// repository (if a string) or repository.url (if an object)
// returns null if it's not a valid repo, or not a known hosted repo
const hostedGitInfo = require('hosted-git-info')
module.exports = mani => {
  const r = mani.repository
  const rurl = !r ? null
    : typeof r === 'string' ? r
    : typeof r === 'object' && typeof r.url === 'string' ? r.url
    : null

  // hgi returns undefined sometimes, but let's always return null here
  return (rurl && hostedGitInfo.fromUrl(rurl.replace(/^git\+/, ''))) || null
}
