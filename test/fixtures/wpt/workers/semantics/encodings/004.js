onconnect = function(e) {
  var xhr = new XMLHttpRequest();
  xhr.open('GET', '003-1.py?x=å', false);
  xhr.send();
  var passed = xhr.responseText == 'PASS';
  e.ports[0].postMessage(passed);
}