const linkBins = require('./link-bins.js')
const linkMans = require('./link-mans.js')

const binLinks = opts => {
  const { path, pkg, force, global, top } = opts
  // global top pkgs on windows get bins installed in {prefix}, and no mans
  //
  // unix global top pkgs get their bins installed in {prefix}/bin,
  // and mans in {prefix}/share/man
  //
  // non-top pkgs get their bins installed in {prefix}/node_modules/.bin,
  // and do not install mans
  //
  // non-global top pkgs don't have any bins or mans linked.  From here on
  // out, if it's top, we know that it's global, so no need to pass that
  // option further down the stack.
  if (top && !global) {
    return Promise.resolve()
  }

  return Promise.all([
    // allow clobbering within the local node_modules/.bin folder.
    // only global bins are protected in this way, or else it is
    // yet another vector for excessive dependency conflicts.
    linkBins({ path, pkg, top, force: force || !top }),
    linkMans({ path, pkg, top, force }),
  ])
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
