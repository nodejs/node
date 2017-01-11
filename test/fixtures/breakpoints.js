debugger;
function a(x) {
  let i = 10;
  while (--i != 0);
  debugger;
  return i;
}
function b() {
  return ['hello', 'world'].join(' ');
}
a();
debugger;
a(1);
b();
b();


setInterval(() => {
}, 5000);


now = new Date();
debugger;
