var x = new XMLHttpRequest();
x.open("GET", "test.txt", false);
x.send();
postMessage(x.response);
