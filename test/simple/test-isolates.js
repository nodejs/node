var fs = require('fs');

console.log("count: %d", process._countIsolate());

if (process.tid === 1) {
  var isolate = process._newIsolate(process.argv);
  //process._joinIsolate(isolate);
  console.error("master");
  fs.stat(__dirname, function(err, stat) {
    if (err) {
      console.error("thread 1 error!");
      throw err;
    }
    console.error('thread 1', stat);
  });
  console.log("thread 1 count: %d", process._countIsolate());
} else {
  console.error("slave");
  fs.stat(__dirname, function(err, stat) {
    if (err) {
      console.error("thread 2 error!");
      throw err;
    }
    console.error('thread 2', stat);
  });
  console.error("thread 2 count: %d", process._countIsolate());
}
