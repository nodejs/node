const PackageUrlCmd = require('../package-url-cmd.js')

class Bugs extends PackageUrlCmd {
  static description = 'Report bugs for a package in a web browser'
  static name = 'bugs'

  getUrl (spec, mani) {
    if (mani.bugs) {
      if (typeof mani.bugs === 'string') {
        return mani.bugs
      }

      if (typeof mani.bugs === 'object' && mani.bugs.url) {
        return mani.bugs.url
      }

      if (typeof mani.bugs === 'object' && mani.bugs.email) {
        return `mailto:${mani.bugs.email}`
      }
    }

    // try to get it from the repo, if possible
    const info = this.hostedFromMani(mani)
    const infoUrl = info?.bugs()
    if (infoUrl) {
      return infoUrl
    }

    // just send them to the website, hopefully that has some info!
    return `https://www.npmjs.com/package/${mani.name}`
  }
}

module.exports = Bugs
