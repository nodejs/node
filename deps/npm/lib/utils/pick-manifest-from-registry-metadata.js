'use strict'
module.exports = pickManifestFromRegistryMetadata

var log = require('npmlog')
var semver = require('semver')

function pickManifestFromRegistryMetadata (spec, tag, versions, metadata) {
  log.silly('pickManifestFromRegistryMetadata', 'spec', spec, 'tag', tag, 'versions', versions)

  // if the tagged version satisfies, then use that.
  var tagged = metadata['dist-tags'][tag]
  if (tagged &&
      metadata.versions[tagged] &&
      semver.satisfies(tagged, spec, true)) {
    return {resolvedTo: tag, manifest: metadata.versions[tagged]}
  }
  // find the max satisfying version.
  var ms = semver.maxSatisfying(versions, spec, true)
  if (ms) {
    return {resolvedTo: ms, manifest: metadata.versions[ms]}
  } else if (spec === '*' && versions.length && tagged && metadata.versions[tagged]) {
    return {resolvedTo: tag, manifest: metadata.versions[tagged]}
  } else {

  }
}
