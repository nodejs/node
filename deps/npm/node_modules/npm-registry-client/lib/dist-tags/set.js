module.exports = set

var assert = require('assert')
var url = require('url')

var npa = require('npm-package-arg')

function set (uri, params, cb) {
  assert(typeof uri === 'string', 'must pass registry URI to distTags.set')
  assert(
    params && typeof params === 'object',
    'must pass params to distTags.set'
  )
  assert(typeof cb === 'function', 'muss pass callback to distTags.set')

  assert(
    typeof params.package === 'string',
    'must pass package name to distTags.set'
  )
  assert(
    params.distTags && typeof params.distTags === 'object',
    'must pass distTags map to distTags.set'
  )
  assert(
    params.auth && typeof params.auth === 'object',
    'must pass auth to distTags.set'
  )

  var p = npa(params.package)
  var pkg = p.scope ? params.package.replace('/', '%2f') : params.package
  var rest = '-/package/' + pkg + '/dist-tags'

  var options = {
    method: 'PUT',
    body: JSON.stringify(params.distTags),
    auth: params.auth
  }
  this.request(url.resolve(uri, rest), options, cb)
}
