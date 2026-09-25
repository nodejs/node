import {foo} from './esm-dep.mjs';

const obj = {
  a: {
    b: 22
  }
};

if (obj?.a?.b === 22) throw Error('an exception');
