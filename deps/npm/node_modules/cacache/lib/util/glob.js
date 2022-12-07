'use strict'

const { promisify } = require('util')
const glob = promisify(require('glob'))

const globify = (pattern) => pattern.split('//').join('/')
module.exports = (path, options) => glob(globify(path), options)
