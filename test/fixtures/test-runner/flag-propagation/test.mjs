import { test } from 'node:test';
import { strictEqual } from 'node:assert';
import internal from 'internal/options';
import { parseArgs } from 'node:util';

const options = {
  flag: {
    type: 'string',
    default: '',
  },
  expected: {
    type: 'string',
    default: '',
  },
  description: {
    type: 'string',
    default: 'flag propagation test',
  },
};


const { values } = parseArgs({ args: process.argv.slice(2), options });

const { flag, expected, description } = values;

test(description, () => {
  const optionValue = internal.getOptionValue(flag);
  console.error(`testing flag: ${flag}, found value: ${optionValue}, expected: ${expected}`);
  
  const isNumber = !isNaN(Number(expected));
  const booleanValue = expected === 'true' || expected === 'false';
  if (booleanValue) {
    strictEqual(optionValue, expected === 'true');
    return;
  } else if (isNumber) {
    strictEqual(Number(optionValue), Number(expected));
  } else{
    strictEqual(optionValue, expected);
  }
});
