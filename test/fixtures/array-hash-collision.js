'use strict';

// See https://hackerone.com/reports/3511792

const payload = [];
const val = 1234;
const MOD = 2 ** 19;
const CHN = 2 ** 17;
const REP = 2 ** 17;

if (process.argv[2] === 'benign') {
  for (let i = 0; i < CHN + REP; i++) {
    payload.push(`${val + i}`);
  }
} else {
  let j = val + MOD;
  for (let i = 1; i < CHN; i++) {
    payload.push(`${j}`);
    j = (j + i) % MOD;
  }
  for (let k = 0; k < REP; k++) {
    payload.push(`${val}`);
  }
}

const string = JSON.stringify({ data: payload });
JSON.parse(string);
