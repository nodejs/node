// Throw a runtime error, the UA must report the error for that script.
// https://html.spec.whatwg.org/#runtime-script-errors-2
for (;;)
  throw new Error("error from onerror.js");
postMessage(1); // shouldn't do anything since the script doesn't compile
