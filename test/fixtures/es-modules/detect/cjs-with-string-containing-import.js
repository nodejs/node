const { version } = require('process');

const sneakyString = `
import { version } from 'process';
`;

console.log(version);
