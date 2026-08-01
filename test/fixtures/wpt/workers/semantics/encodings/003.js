var xhr = new XMLHttpRequest();
xhr.open('GET', '003-1.py?x=å', false);
xhr.send();
var passed = xhr.responseText == 'PASS';
postMessage(passed);