module.exports = rm

var assert = require('assert')
var url = require('url')

var npa = require('npm-package-arg')

function rm (uri, params, cb) {
  assert(typeof uri === 'string', 'must pass registry URI to distTags.rm')
  assert(
    params && typeof params === 'object',
    'must pass params to distTags.rm'
  )
  assert(typeof cb === 'function', 'muss pass callback to distTags.rm')

  assert(
    typeof params.package === 'string',
    'must pass package name to distTags.rm'
  )
  assert(
    typeof params.distTag === 'string',
    'must pass package distTag name to distTags.rm'
  )
  assert(
    params.auth && typeof params.auth === 'object',
    'must pass auth to distTags.rm'
  )

  var p = npa(params.package)
  var pkg = p.scope ? params.package.replace('/', '%2f') : params.package
  var rest = '-/package/' + pkg + '/dist-tags/' + params.distTag

  var options = {
    method: 'DELETE',
    auth: params.auth
  }
  this.request(url.resolve(uri, rest), options, cb)
}
