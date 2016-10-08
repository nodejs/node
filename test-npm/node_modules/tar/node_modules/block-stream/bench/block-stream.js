var BlockStream = require("../block-stream.js")

var blockSizes = [16, 25, 1024]
  , writeSizes = [4, 8, 15, 16, 17, 64, 100]
  , writeCounts = [1, 10, 100]
  , tap = require("tap")

writeCounts.forEach(function (writeCount) {
blockSizes.forEach(function (blockSize) {
writeSizes.forEach(function (writeSize) {
  tap.test("writeSize=" + writeSize +
           " blockSize="+blockSize +
           " writeCount="+writeCount, function (t) {
    var f = new BlockStream(blockSize, {nopad: true })

    var actualChunks = 0
    var actualBytes = 0
    var timeouts = 0

    f.on("data", function (c) {
      timeouts ++

      actualChunks ++
      actualBytes += c.length

      // make sure that no data gets corrupted, and basic sanity
      var before = c.toString()
      // simulate a slow write operation
      setTimeout(function () {
        timeouts --

        var after = c.toString()
        t.equal(after, before, "should not change data")

        // now corrupt it, to find leaks.
        for (var i = 0; i < c.length; i ++) {
          c[i] = "x".charCodeAt(0)
        }
      }, 100)
    })

    f.on("end", function () {
      // round up to the nearest block size
      var expectChunks = Math.ceil(writeSize * writeCount  * 2 / blockSize)
      var expectBytes = writeSize * writeCount * 2
      t.equal(actualBytes, expectBytes,
              "bytes=" + expectBytes + " writeSize=" + writeSize)
      t.equal(actualChunks, expectChunks,
              "chunks=" + expectChunks + " writeSize=" + writeSize)

      // wait for all the timeout checks to finish, then end the test
      setTimeout(function WAIT () {
        if (timeouts > 0) return setTimeout(WAIT)
        t.end()
      }, 100)
    })

    for (var i = 0; i < writeCount; i ++) {
      var a = new Buffer(writeSize);
      for (var j = 0; j < writeSize; j ++) a[j] = "a".charCodeAt(0)
      var b = new Buffer(writeSize);
      for (var j = 0; j < writeSize; j ++) b[j] = "b".charCodeAt(0)
      f.write(a)
      f.write(b)
    }
    f.end()
  })
}) }) })
