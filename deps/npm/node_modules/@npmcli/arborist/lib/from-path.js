// file dependencies need their dependencies resolved based on the
// location where the tarball was found, not the location where they
// end up getting installed.  directory (ie, symlink) deps also need
// to be resolved based on their targets, but that's what realpath is

const { dirname } = require('path')
const npa = require('npm-package-arg')

const fromPath = (node, spec) =>
  spec && spec.type === 'file' ? dirname(spec.fetchSpec)
  : node.realpath

module.exports = node => fromPath(node, node.resolved && npa(node.resolved))
