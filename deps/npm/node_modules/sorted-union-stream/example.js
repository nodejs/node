var union = require('./')
var from = require('from2')

var sorted1 = from.obj([{key: 'a'}, {key: 'b'}, {key: 'c'}])
var sorted2 = from.obj([{key: 'b'}, {key: 'd'}])

var u = union(sorted1, sorted2)

u.on('data', function (data) {
  console.log(data)
})

u.on('end', function () {
  console.log('no more data')
})
