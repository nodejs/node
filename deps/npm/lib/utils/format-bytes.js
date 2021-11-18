// Convert bytes to printable output, for file reporting in tarballs
// Only supports up to GB because that's way larger than anything the registry
// supports anyways.

const formatBytes = (bytes, space = true) => {
  let spacer = ''
  if (space) {
    spacer = ' '
  }

  if (bytes < 1000) {
    // B
    return `${bytes}${spacer}B`
  }

  if (bytes < 1000000) {
    // kB
    return `${(bytes / 1000).toFixed(1)}${spacer}kB`
  }

  if (bytes < 1000000000) {
    // MB
    return `${(bytes / 1000000).toFixed(1)}${spacer}MB`
  }

  return `${(bytes / 1000000000).toFixed(1)}${spacer}GB`
}

module.exports = formatBytes
