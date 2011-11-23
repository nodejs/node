var bench = require("bench")

var l = 1000
  , FastList = require("./fast-list.js")

exports.countPerLap = l * 2

exports.compare =
  { "[]": function () {
      var list = []
      for (var j = 0; j < l; j ++) {
        if (j % 2) list.push(j)
        else list.unshift(j)
      }
      for (var j = 0; j < l; j ++) {
        if (j % 2) list.shift(j)
        else list.pop(j)
      }
    }
  , "new Array()": function () {
      var list = new Array()
      for (var j = 0; j < l; j ++) {
        if (j % 2) list.push(j)
        else list.unshift(j)
      }
      for (var j = 0; j < l; j ++) {
        if (j % 2) list.shift(j)
        else list.pop(j)
      }
    }
  // , "FastList()": function () {
  //     var list = FastList()
  //     for (var j = 0; j < l; j ++) {
  //       if (j % 2) list.push(j)
  //       else list.unshift(j)
  //     }
  //     for (var j = 0; j < l; j ++) {
  //       if (j % 2) list.shift(j)
  //       else list.pop(j)
  //     }
  //   }
  , "new FastList()": function () {
      var list = new FastList()
      for (var j = 0; j < l; j ++) {
        if (j % 2) list.push(j)
        else list.unshift(j)
      }
      for (var j = 0; j < l; j ++) {
        if (j % 2) list.shift(j)
        else list.pop(j)
      }
    }
  }

bench.runMain()
