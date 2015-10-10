module.exports = {
  "name": prompt("name", function (data) {
    if (data === 'cool') return data
    var er = new Error('not cool')
    er.notValid = true
    return er
  })
}
