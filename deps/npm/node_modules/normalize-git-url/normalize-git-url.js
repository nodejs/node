var url = require('url')

module.exports = function normalize (u) {
  var parsed = url.parse(u)
  // If parsing actually alters the URL, it is almost certainly an
  // scp-style URL, or an invalid one.
  var altered = u !== url.format(parsed)

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
  if (altered) {
    if (u.match(/^git\+https?/) && parsed.pathname.match(/\/?:[^0-9]/)) {
      returnedUrl = u.replace(/^git\+(.*:[^:]+):(.*)/, '$1/$2')
    } else {
      returnedUrl = u.replace(/^(?:git\+)?ssh:\/\//, '')
    }
    returnedUrl = returnedUrl.replace(/#[^#]*$/, '')
  } else {
    returnedUrl = url.format(parsed)
  }

  return {
    url: returnedUrl,
    branch: checkout
  }
}
