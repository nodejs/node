// return the PATH array in a cross-platform way
const PATH = process.env.PATH || process.env.Path || process.env.path
const { delimiter } = require('path')
module.exports = PATH.split(delimiter)
