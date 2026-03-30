'use strict';
function fnA() {
  let cnt = 0;

  try {
    cnt++;
    throw new Error('boom');
    cnt++;
  } catch (err) {
    cnt++;
  } finally {
    if (false) {

    }

    return cnt;
  }
  cnt++;
}

function fnB(arr) {
  for (let i = 0; i < arr.length; ++i) {
    if (i === 2) {
      continue;
    } else {
      fnE(1);
    }
  }
}

function fnC(arg1, arg2) {
  if (arg1 === 1) {
    if (arg2 === 3) {
      return -1;
    }

    if (arg2 === 4) {
      return 3;
    }

    if (arg2 === 5) {
      return 9;
    }
  }
}

function fnD(arg) {
  let cnt = 0;

  if (arg % 2 === 0) {
    cnt++;
  } else if (arg === 1) {
    cnt++;
  } else if (arg === 3) {
    cnt++;
  } else {
    fnC(1, 5);
  }

  return cnt;
}

function fnE(arg) {
  const a = arg ?? 5;

  return a;
}

module.exports = { fnA, fnB, fnC, fnD, fnE };
