'use strict';

async function one() {
  throw new Error('test');
}

async function breaker() {
  return true;
}

async function stack() {
  await breaker();
}

async function two() {
  await stack();
  await one();
}
async function three() {
  await two();
}

async function four() {
  await three();
}

module.exports = four;
