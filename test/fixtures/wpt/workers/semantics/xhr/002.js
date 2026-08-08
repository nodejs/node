var xhr = new XMLHttpRequest();
var log = '';
xhr.open('GET', '001-1.xml', false);
xhr.send();
if (xhr.responseXML != null)
  log += 'responseXML was not null. ';
if (xhr.responseText != '<x>foo</x>')
  log += 'responseText was ' + this.responseText + ', expected <x>foo</x>. ';
postMessage(log);