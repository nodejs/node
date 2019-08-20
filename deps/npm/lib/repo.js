module.exports = repo

repo.usage = 'npm repo [<pkg>]'

const openUrl = require('./utils/open-url')
const hostedGitInfo = require('hosted-git-info')
const url_ = require('url')
const fetchPackageMetadata = require('./fetch-package-metadata.js')

repo.completion = function (opts, cb) {
  // FIXME: there used to be registry completion here, but it stopped making
  // sense somewhere around 50,000 packages on the registry
  cb()
}

function repo (args, cb) {
  const n = args.length ? args[0] : '.'
  fetchPackageMetadata(n, '.', {fullMetadata: true}, function (er, d) {
    if (er) return cb(er)
    getUrlAndOpen(d, cb)
  })
}

function getUrlAndOpen (d, cb) {
  const r = d.repository
  if (!r) return cb(new Error('no repository'))
  // XXX remove this when npm@v1.3.10 from node 0.10 is deprecated
  // from https://github.com/npm/npm-www/issues/418
  const info = hostedGitInfo.fromUrl(r.url)
  const url = info ? info.browse() : unknownHostedUrl(r.url)

  if (!url) return cb(new Error('no repository: could not get url'))

  openUrl(url, 'repository available at the following URL', cb)
}

function unknownHostedUrl (url) {
  try {
    const idx = url.indexOf('@')
    if (idx !== -1) {
      url = url.slice(idx + 1).replace(/:([^\d]+)/, '/$1')
    }
    url = url_.parse(url)
    const protocol = url.protocol === 'https:'
      ? 'https:'
      : 'http:'
    return protocol + '//' + (url.host || '') +
      url.path.replace(/\.git$/, '')
  } catch (e) {}
}
