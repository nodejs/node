'use strict'

module.exports = require('bluebird').promisify(require('read-package-json'))
