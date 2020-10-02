// we know it's global and/or not top, so the path has to be
// {prefix}/node_modules/{name}.  Can't rely on pkg.name, because
// it might be installed as an alias.

const {dirname, basename} = require('path')
// this gets called a lot and can't change, so memoize it
const memo = new Map()
module.exports = path => {
  if (memo.has(path))
    return memo.get(path)

  const scopeOrNm = dirname(path)
  const nm = basename(scopeOrNm) === 'node_modules' ? scopeOrNm
    : dirname(scopeOrNm)

  memo.set(path, nm)
  return nm
}
