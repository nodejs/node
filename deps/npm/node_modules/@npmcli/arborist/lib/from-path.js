// file dependencies need their dependencies resolved based on the
// location where the tarball was found, not the location where they
// end up getting installed.  directory (ie, symlink) deps also need
// to be resolved based on their targets, but that's what realpath is

const { dirname } = require('path')
const npa = require('npm-package-arg')

const fromPath = (node, spec, edge) => {
  if (edge && edge.overrides && edge.overrides.name === edge.name && edge.overrides.value) {
    // fromPath could be called with a node that has a virtual root, if that happens
    // we want to make sure we get the real root node when overrides are in use. this
    // is to allow things like overriding a dependency with a tarball file that's a
    // relative path from the project root
    return node.sourceReference
      ? node.sourceReference.root.realpath
      : node.root.realpath
  }

  return spec && spec.type === 'file' ? dirname(spec.fetchSpec)
    : node.realpath
}

module.exports = (node, edge) => fromPath(node, node.resolved && npa(node.resolved), edge)
