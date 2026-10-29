// prevent recursion
if ('beenThere' in self) {
  throw 'undefined stringified to the empty string';
}
beenThere = true;
try {
  importScripts(undefined);
  postMessage(got);
} catch(ex) {
  postMessage(String(ex));
}