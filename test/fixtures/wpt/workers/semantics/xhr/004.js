onconnect = function(e) {
  var xhr = new XMLHttpRequest();
  var log = '';
  xhr.open('GET', '001-1.xml', false);
  xhr.send();
  if (xhr.responseXML != null)
    log += 'responseXML was not null. ';
  if (xhr.responseText != '<x>foo</x>')
    log += 'responseText was ' + xhr.responseText + ', expected <x>foo</x>. ';
  e.ports[0].postMessage(log);
}
