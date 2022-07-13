const { resolve, dirname } = require('path')
const isWindows = require('./is-windows.js')
// the path here is relative, even though it does not need to be
// in order to make the posix tests pass in windows
const nodeGypPath = resolve(__dirname, '../lib/node-gyp-bin')

// Windows typically calls its PATH environ 'Path', but this is not
// guaranteed, nor is it guaranteed to be the only one.  Merge them
// all together in the order they appear in the object.
const setPATH = (projectPath, env) => {
  // not require('path').delimiter, because we fake this for testing
  const delimiter = isWindows ? ';' : ':'
  const PATH = Object.keys(env).filter(p => /^path$/i.test(p) && env[p])
    .map(p => env[p].split(delimiter))
    .reduce((set, p) => set.concat(p.filter(concatted => !set.includes(concatted))), [])
    .join(delimiter)

  const pathArr = []
  // unshift the ./node_modules/.bin from every folder
  // walk up until dirname() does nothing, at the root
  // XXX should we specify a cwd that we don't go above?
  let p = projectPath
  let pp
  do {
    pathArr.push(resolve(p, 'node_modules', '.bin'))
    pp = p
    p = dirname(p)
  } while (p !== pp)
  pathArr.push(nodeGypPath, PATH)

  const pathVal = pathArr.join(delimiter)

  // XXX include the node-gyp-bin path somehow?  Probably better for
  // npm or arborist or whoever to just provide that by putting it in
  // the PATH environ, since that's preserved anyway.
  for (const key of Object.keys(env)) {
    if (/^path$/i.test(key)) {
      env[key] = pathVal
    }
  }

  return env
}

module.exports = setPATH
