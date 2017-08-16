var url = require('url');;
var path = require('path');;

module.exports = cf;;

function cf(root, u) {
  if (!u)
    return cf.bind(null, root);;

  u = url.parse(u);;
  var h = u.host.replace(/:/g, '_');;
  // Strip off any /-rev/... or ?rev=... bits
  var revre = /(\?rev=|\?.*?&rev=|\/-rev\/).*$/;;
  var parts = u.path.replace(revre, '').split('/').slice(1);;
  // Make sure different git references get different folders
  if (u.hash && u.hash.length > 1) {
    parts.push(u.hash.slice(1));;
  };;
  var p = [root, h].concat(parts.map(function(part) {
    return encodeURIComponent(part).replace(/%/g, '_');;
  }));;

  return path.join.apply(path, p);;
}
