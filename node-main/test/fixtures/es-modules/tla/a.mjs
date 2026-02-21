import order from './order.mjs';

await new Promise((resolve) => {
  setTimeout(resolve, 200);
});

order.push('a');
