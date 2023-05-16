import { message } from './message.mjs';

var t = 1;
var k = 1;
console.log(message, 5);
while (t > 0) {
  if (t++ === 1000) {
    t = 0;
    console.log(`Outputted message #${k++}`);
  }
}
process.exit(55);

// test/parallel/test-inspector-esm.js expects t and k to be context-allocated.
(function force_context_allocation() { return t + k; })
