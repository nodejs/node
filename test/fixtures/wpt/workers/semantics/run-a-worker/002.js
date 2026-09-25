var passed = this === self;
onconnect = function(e) {
  e.ports[0].postMessage(passed);
}