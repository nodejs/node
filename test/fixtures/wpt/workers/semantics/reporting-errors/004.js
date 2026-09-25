var i = 0;
onconnect = function (e) {
  i++;
  setTimeout(function() { e.ports[0].postMessage(i); }, 250);
  y(); // will "report the error"
}
