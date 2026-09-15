var port;
onconnect = function(e) {
  if (!port)
    port = e.ports[0];
  port.postMessage(1);
}