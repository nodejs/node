const removeTrailingSlashes = (input) => {
  // in order to avoid regexp redos detection
  let output = input
  while (output.endsWith('/')) {
    output = output.substr(0, output.length - 1)
  }
  return output
}

module.exports = removeTrailingSlashes
