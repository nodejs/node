const getBinFromManifest = (mani) => {
  // if we have a bin matching (unscoped portion of) packagename, use that
  // otherwise if there's 1 bin or all bin value is the same (alias), use
  // that; otherwise, fail
  const bin = mani.bin || {}
  if (new Set(Object.values(bin)).size === 1) {
    return Object.keys(bin)[0]
  }

  // XXX probably a util to parse this better?
  const name = mani.name.replace(/^@[^/]+\//, '')
  if (bin[name]) {
    return name
  }

  // XXX need better error message
  throw Object.assign(new Error('could not determine executable to run'), {
    pkgid: mani._id,
  })
}

module.exports = getBinFromManifest
