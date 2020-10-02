const {dirname, resolve} = require('path')
module.exports = function* (path) {
  for (path = resolve(path); path;) {
    yield path
    const pp = dirname(path)
    if (pp === path)
      path = null
    else
      path = pp
  }
}
