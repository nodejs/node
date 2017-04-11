process.moduleWrap = function(content, filename) {
  if (filename !== 'http.js') return content
  return content + '\nmodule.exports.hello = "hi";'
}
