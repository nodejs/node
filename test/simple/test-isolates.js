var fs = require('fs');
var http = require('http');

console.log("count: %d", process._countIsolate());

if (process.tid === 1) {
  var isolate = process._newIsolate(process.argv);
  //process._joinIsolate(isolate);
  console.error("master");
  fs.stat(__dirname, function(err, stat) {
    if (err) throw err;
    console.error('thread 1', stat.mtime);
  });

  setTimeout(function() {
    fs.stat(__dirname, function(err, stat) {
      if (err) throw err;
      console.error('thread 1', stat.mtime);
    });
  }, 500);

  console.log("thread 1 count: %d", process._countIsolate());
} else {
  console.error("slave");
  fs.stat(__dirname, function(err, stat) {
    if (err) throw err;
    console.error('thread 2', stat.mtime);
  });

  setTimeout(function() {
    fs.stat(__dirname, function(err, stat) {
      if (err) throw err;
      console.error('thread 2', stat.mtime);
    });
  }, 500);

  console.error("thread 2 count: %d", process._countIsolate());
}
