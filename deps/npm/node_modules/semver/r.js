var semver = require('./')
var r = new semver.Range('git+https://user:password0123@github.com/foo/bar.git', true)
r.inspect = null
console.log(r)
