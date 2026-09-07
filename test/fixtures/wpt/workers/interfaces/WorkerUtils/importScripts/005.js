var x;
var y;
try {
  importScripts('data:text/javascript,x={',
                'data:text/javascript,}');
} catch(e) {
  y = true;
}
postMessage([x, y]);