// unix absolute paths are also absolute on win32, so we use this for both
const { isAbsolute, parse } = require('path').win32

// returns [root, stripped]
module.exports = path => {
  let r = ''
  while (isAbsolute(path)) {
    // windows will think that //x/y/z has a "root" of //x/y/
    const root = path.charAt(0) === '/' ? '/' : parse(path).root
    path = path.substr(root.length)
    r += root
  }
  return [r, path]
}
