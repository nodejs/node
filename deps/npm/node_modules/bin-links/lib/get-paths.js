// get all the paths that are (or might be) installed for a given pkg
// There's no guarantee that all of these will be installed, but if they
// are present, then we can assume that they're associated.
const binTarget = require('./bin-target.js')
const manTarget = require('./man-target.js')
const { resolve, basename } = require('path')
const isWindows = require('./is-windows.js')
module.exports = ({ path, pkg, global, top }) => {
  if (top && !global) {
    return []
  }

  const binSet = []
  const binTarg = binTarget({ path, top })
  if (pkg.bin) {
    for (const bin of Object.keys(pkg.bin)) {
      const b = resolve(binTarg, bin)
      binSet.push(b)
      if (isWindows) {
        binSet.push(b + '.cmd')
        binSet.push(b + '.ps1')
      }
    }
  }

  const manTarg = manTarget({ path, top })
  const manSet = []
  if (manTarg && pkg.man && Array.isArray(pkg.man) && pkg.man.length) {
    for (const man of pkg.man) {
      const parseMan = man.match(/(.*\.([0-9]+)(\.gz)?)$/)
      // invalid entries invalidate the entire man set
      if (!parseMan) {
        return binSet
      }

      const stem = parseMan[1]
      const sxn = parseMan[2]
      const base = basename(stem)
      const absFrom = resolve(path, man)

      /* istanbul ignore if - should be impossible */
      if (absFrom.indexOf(path) !== 0) {
        return binSet
      }

      manSet.push(resolve(manTarg, 'man' + sxn, base))
    }
  }

  return manSet.length ? [...binSet, ...manSet] : binSet
}
