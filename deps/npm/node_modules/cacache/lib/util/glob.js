'use strict'

const { glob } = require('glob')

const globify = (pattern) => pattern.split('//').join('/')
module.exports = (path, options) => glob(globify(path), options)
