var channel = new MessageChannel();
var i = 0;
onconnect = function(e) {
  i++;
  e.ports[0].postMessage(1, [channel['port' + i]]);
}