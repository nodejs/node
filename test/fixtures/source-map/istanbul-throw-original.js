/*
 * comments dropped by uglify.
 */
function Hello() {
  throw Error('goodbye');
}

setImmediate(function() {
  Hello();
});
