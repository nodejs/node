const x = 10;
let name = 'World';
name = 'Robin';
function sayHello() {
  if (x > 0) {
    console.log(`Hello ${name}`);
  }
}
sayHello();
debugger;
setTimeout(sayHello, 10);

function otherFunction() {
  console.log('x = %d', x);
}
setTimeout(otherFunction, 50);
