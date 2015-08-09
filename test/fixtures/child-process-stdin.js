console.log(process.stdin.isTTY);
console.log(process.stdin.isRaw);
try {
  process.stdin.setRawMode();
} catch (ex) {
  console.error(ex);
}
