var x = '';
var exception;
try {
  importScripts('data:text/javascript,x+="first script successful. "',
                'data:text/javascript,x+="FAIL (second script). "; for(;) break;', // doesn't compile
                'data:text/javascript,x+="FAIL (third script)"');
} catch(ex) {
  if (ex instanceof SyntaxError)
    exception = true;
  else
    exception = String(ex);
}
postMessage([x, exception]);