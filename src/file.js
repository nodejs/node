node.fs.exists = function (path, callback) {
  var p = node.fs.stat(path);
  p.addCallback(function () { callback(true); });
  p.addErrback(function () { callback(false); });
}

node.fs.cat = function (path, encoding) {
  var open_promise = node.fs.open(path, node.O_RDONLY, 0666);
  var cat_promise = new node.Promise();

  encoding = (encoding === "raw" ? node.RAW : node.UTF8);

  open_promise.addErrback(function () { cat_promise.emitError(); });
  open_promise.addCallback(function (fd) {
    var content = (encoding === node.UTF8 ? "" : []);
    var pos = 0;

    function readChunk () {
      var read_promise = node.fs.read(fd, 16*1024, pos, encoding);

      read_promise.addErrback(function () { cat_promise.emitError(); });

      read_promise.addCallback(function (chunk, bytes_read) {
        if (chunk) {
          if (chunk.constructor == String)
            content += chunk;
          else
            content = content.concat(chunk);

          pos += bytes_read;
          readChunk();
        } else {
          cat_promise.emitSuccess([content]);
          node.fs.close(fd);
        }
      });
    }
    readChunk();
  });
  return cat_promise;
};
