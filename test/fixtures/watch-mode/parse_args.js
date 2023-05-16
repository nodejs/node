const { parseArgs } = require('node:util');

const { values } = parseArgs({ options: { random: { type: 'string' } } });
console.log(values.random);
