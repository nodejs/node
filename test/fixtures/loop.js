var t = 1;
var k = 1;
console.log('A message', 5);
while (t > 0) {
  if (t++ === 1000) {
    t = 0;
    console.log('Outputed message #' + k++);
  }
}
process.exit(55);
