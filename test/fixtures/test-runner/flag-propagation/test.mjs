import { test } from 'node:test';
import { deepStrictEqual } from 'node:assert';
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

let { flag, expected, description } = values;

test(description, () => {
  const optionValue = internal.getOptionValue(flag);
  const isArrayOption = Array.isArray(optionValue);

  if (isArrayOption) {
    expected = [expected];
  }

  console.error(`testing flag: ${flag}, found value: ${optionValue}, expected: ${expected}`);
  
  const isNumber = !isNaN(Number(expected));
  const booleanValue = expected === 'true' || expected === 'false';
  if (booleanValue) {
    deepStrictEqual(optionValue, expected === 'true');
    return;
  } else if (isNumber) {
    deepStrictEqual(Number(optionValue), Number(expected));
  } else{
    deepStrictEqual(optionValue, expected);
  }
});
