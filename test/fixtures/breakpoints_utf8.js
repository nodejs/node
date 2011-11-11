debugger;
function a(x) {
  var i = 10;
  while (--i != 0);
  debugger;
  return i;
}
function b() {
  return ['こんにち', 'わ'].join(' ');
}
a();
a(1);
b();
b();



setInterval(function() {
}, 5000);
