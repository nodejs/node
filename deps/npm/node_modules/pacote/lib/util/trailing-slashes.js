const removeTrailingSlashes = (input) => {
  // in order to avoid regexp redos detection
  let output = input
  while (output.endsWith('/')) {
    output = output.slice(0, -1)
  }
  return output
}

module.exports = removeTrailingSlashes
