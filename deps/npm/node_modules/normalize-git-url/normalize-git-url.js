var url = require('url')

module.exports = function normalize (u) {
  var parsed = url.parse(u, true)

  // git is so tricky!
  // if the path is like ssh://foo:22/some/path then it works, but
  // it needs the ssh://
  // If the path is like ssh://foo:some/path then it works, but
  // only if you remove the ssh://
  if (parsed.protocol) {
    parsed.protocol = parsed.protocol.replace(/^git\+/, '')
  }

  // figure out what we should check out.
  var checkout = parsed.hash && parsed.hash.substr(1) || 'master'
  parsed.hash = ''

  var returnedUrl
  if (parsed.pathname.match(/\/?:/)) {
    returnedUrl = u.replace(/^(?:git\+)?ssh:\/\//, '').replace(/#[^#]*$/, '')
  } else {
    returnedUrl = url.format(parsed)
  }

  return {
    url: returnedUrl,
    branch: checkout
  }
}
