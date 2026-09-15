// prevent recursion
if ('beenThere' in self) {
  throw '1 stringified to the empty string';
}
beenThere = true;
try {
  importScripts(1);
  postMessage(got);
} catch(ex) {
  postMessage(String(ex));
}