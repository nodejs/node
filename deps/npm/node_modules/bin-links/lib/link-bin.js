const linkGently = require('./link-gently.js')
const fixBin = require('./fix-bin.js')

// linking bins is simple.  just symlink, and if we linked it, fix the bin up
const linkBin = ({path, to, from, absFrom, force}) =>
  linkGently({path, to, from, absFrom, force})
    .then(linked => linked && fixBin(absFrom))

module.exports = linkBin
