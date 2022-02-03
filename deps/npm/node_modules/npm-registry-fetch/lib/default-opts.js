const pkg = require('../package.json')
module.exports = {
  log: require('./silentlog.js'),
  maxSockets: 12,
  method: 'GET',
  registry: 'https://registry.npmjs.org/',
  timeout: 5 * 60 * 1000, // 5 minutes
  strictSSL: true,
  noProxy: process.env.NOPROXY,
  userAgent: `${pkg.name
    }@${
      pkg.version
    }/node@${
      process.version
    }+${
      process.arch
    } (${
      process.platform
    })`,
}
