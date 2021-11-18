const aliases = require('../utils/cmd-list').aliases

module.exports = function usage (cmd, txt, opt) {
  const post = Object.keys(aliases).reduce(function (p, c) {
    var val = aliases[c]
    if (val !== cmd) {
      return p
    }
    return p.concat(c)
  }, [])

  if (opt || post.length > 0) {
    txt += '\n\n'
  }

  if (post.length === 1) {
    txt += 'alias: '
    txt += post.join(', ')
  } else if (post.length > 1) {
    txt += 'aliases: '
    txt += post.join(', ')
  }

  if (opt) {
    if (post.length > 0) {
      txt += '\n'
    }
    txt += 'common options: ' + opt
  }

  return txt
}
