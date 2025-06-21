const checkBin = require('./check-bin.js')
const normalize = require('npm-normalize-package-bin')
const checkBins = async ({ pkg, path, top, global, force }) => {
  // always ok to clobber when forced
  // always ok to clobber local bins, or when forced
  if (force || !global || !top) {
    return
  }

  pkg = normalize(pkg)
  if (!pkg.bin) {
    return
  }

  await Promise.all(Object.keys(pkg.bin)
    .map(bin => checkBin({ bin, path, top, global, force })))
}
module.exports = checkBins
