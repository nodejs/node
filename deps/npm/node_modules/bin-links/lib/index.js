const linkBins = require('./link-bins.js')

const binLinks = opts => {
  const { path, pkg, force, global, top } = opts
  // global top pkgs on windows get bins installed in {prefix}.
  //
  // unix global top pkgs get their bins installed in {prefix}/bin.
  //
  // non-top pkgs get their bins installed in {prefix}/node_modules/.bin.
  //
  // non-global top pkgs don't have any bins linked. From here on out, if it's top, we know that it's global, so no need to pass that option further down the stack.
  //
  // As of v7, bin-links no longer installs man pages into the system man path for any package. `getPaths` still returns legacy man paths so pre-existing installs can be cleaned up on uninstall.
  if (top && !global) {
    return Promise.resolve()
  }

  // allow clobbering within the local node_modules/.bin folder. only global bins are protected in this way, or else it is yet another vector for excessive dependency conflicts.
  return linkBins({ path, pkg, top, force: force || !top })
}

const shimBin = require('./shim-bin.js')
const linkGently = require('./link-gently.js')
const resetSeen = () => {
  shimBin.resetSeen()
  linkGently.resetSeen()
}

const checkBins = require('./check-bins.js')
const getPaths = require('./get-paths.js')

module.exports = Object.assign(binLinks, {
  checkBins,
  resetSeen,
  getPaths,
})
