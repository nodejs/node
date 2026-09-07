// prevent recursion
if ('beenThere' in self) {
  throw 'null stringified to the empty string';
}
beenThere = true;
try {
  importScripts(null);
  postMessage(got);
} catch(ex) {
  postMessage(String(ex));
}