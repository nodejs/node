var x = 'a';
try {
  importScripts('data:text/javascript,x+="b"',
                'data:text/javascript,x+="c"');
} catch(e) {
  x += "d"
}
postMessage(x);