onconnect = function(e) {
  var xhr = new XMLHttpRequest();
  var log = '';
  var port = e.ports[0];
  var postMessage = port.postMessage;
  xhr.onreadystatechange = function(e) {
    if (this.readyState == 4) {
      if (this.responseXML != null)
        log += 'responseXML was not null. ';
      if (this.responseText && this.responseText != '<x>foo</x>')
        log += 'responseText was ' + this.responseText + ', expected <x>foo</x>. ';
      postMessage.call(port, log);
    }
  }
  xhr.open('GET', '001-1.xml', true);
  xhr.send();
}
