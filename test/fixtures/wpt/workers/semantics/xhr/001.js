var xhr = new XMLHttpRequest();
var log = '';
xhr.onreadystatechange = function(e) {
  if (this.readyState == 4) {
    if (this.responseXML != null)
      log += 'responseXML was not null. ';
    if (this.responseText != '<x>foo</x>')
      log += 'responseText was ' + this.responseText + ', expected <x>foo</x>. ';
    postMessage(log);
  }
}
xhr.open('GET', '001-1.xml', true);
xhr.send();