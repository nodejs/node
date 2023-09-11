import order from './order.mjs';

const end = Date.now() + 500;
while (end < Date.now()) {}

order.push('d');
