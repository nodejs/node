const {parse, sep, normalize: norm} = require('path')

function* commonArrayMembers (a, b) {
  const [l, s] = a.length > b.length ? [a, b] : [b, a]
  for (const x of s) {
    if (x === l.shift())
      yield x
    else
      break
  }
}

const commonAncestorPath = (a, b) => a === b ? a
  : parse(a).root !== parse(b).root ? null
  : [...commonArrayMembers(norm(a).split(sep), norm(b).split(sep))].join(sep)

module.exports = (...paths) => paths.reduce(commonAncestorPath)
