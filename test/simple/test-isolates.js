console.log("count: %d", process._countIsolate());

if (process.tid === 1) {
  var isolate = process._newIsolate(process.argv);
  //process._joinIsolate(isolate);
  console.error("master");
  console.log("count: %d", process._countIsolate());
} else {
  console.error("FUCK YEAH!");
  console.log("count: %d", process._countIsolate());
}
