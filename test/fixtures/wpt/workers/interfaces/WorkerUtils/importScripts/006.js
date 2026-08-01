var x;
var y;
var z;
try {
  importScripts('data:text/javascript,x=1',
                'data:text/javascript,throw 2',
                'data:text/javascript,z=3');
} catch(e) {
  y = e;
}
postMessage([x, y, z]);