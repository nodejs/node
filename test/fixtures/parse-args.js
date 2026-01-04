const { parseArgs } = require('util');

const parsedArgs = parseArgs({ strict: false }).values;

process.stdout.write(JSON.stringify(parsedArgs));
