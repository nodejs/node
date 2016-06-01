'use strict';

const async_wrap = process.binding('async_wrap');

const nextIdArray = async_wrap.getNextAsyncIdArray();
const currentIdArray = async_wrap.getCurrentAsyncIdArray();
const fieldsArray = async_wrap.getAsyncHookFields();
const asyncCallbacks = async_wrap.getAsyncCallbacks();

function getLittleEndian(a) {
  return a[0] + a[1] * 0x100000000;
}

function getBigEndian(a) {
  return a[1] + a[0] * 0x100000000;
}

function setLittleEndian(a, val) {

  if (val < 0) {
    throw new Error('Negative value not supported');
  }

  var lword = val & 0xffffffff;
  var hword = 0;
  if (val > 0xffffffff) {
    // effectively we're doing shift-right by 32 bits.  Javascript bit
    // operators convert operands to 32 bits, so we lose the
    // high-order bits if we try to use >>> or >>.
    hword = Math.floor((val / 0x100000000));
  }
  a[0] = lword;
  a[1] = hword;
}

function setBigEndian(a, val) {

  if (val < 0) {
    throw new Error('Negative value not supported');
  }

  var lword = val & 0xffffffff;
  var hword = 0;
  if (val > 0xffffffff) {
    // effectively we're doing shift-right by 32 bits.  Javascript bit
    // operators convert operands to 32 bits, so we lose the
    // high-order bits if we try to use >>> or >>.
    hword = Math.floor((val / 0x100000000));
  }
  a[1] = lword;
  a[0] = hword;
}

function incrementLittleEndian(a) {
  // carry-over if lsb is maxed out
  if (a[0] === 0xffffffff) {
    a[0] = 0;
    a[1]++;
  }
  a[0]++;
  return a[0] + a[1] * 0x100000000;
}

function incrementBigEndian(a) {
  // carry-over if lsb is maxed out
  if (a[1] === 0xffffffff) {
    a[1] = 0;
    a[0]++;
  }
  a[1]++;
  return a[1] + a[0] * 0x100000000;
}

function getCurrentIdLittleEndian() {
  return getLittleEndian(currentIdArray);
}

function getCurrentIdBigEndian() {
  return getBigEndian(currentIdArray);
}

function setCurrentIdLittleEndian(val) {
  return setLittleEndian(currentIdArray, val);
}

function setCurrentIdBigEndian(val) {
  return setBigEndian(currentIdArray, val);
}

function incrementNextIdLittleEndian() {
  return incrementLittleEndian(nextIdArray);
}

function incrementNextIdBigEndian() {
  return incrementBigEndian(nextIdArray);
}

// must match enum definitions in AsyncHook class in env.h
const kEnableCallbacks = 0;

function callbacksEnabled() {
  return (fieldsArray[kEnableCallbacks] !== 0 ? true : false);
}

const getCurrentAsyncId =
    process.binding('os').isBigEndian ?
      getCurrentIdBigEndian : getCurrentIdLittleEndian;

const setCurrentAsyncId =
    process.binding('os').isBigEndian ?
      setCurrentIdBigEndian : setCurrentIdLittleEndian;

const incrementNextAsyncId =
    process.binding('os').isBigEndian ?
      incrementNextIdBigEndian : incrementNextIdLittleEndian;

function notifyAsyncEnqueue(asyncId) {
  if (callbacksEnabled()) {
    const asyncState = {};
    for (let i = 0; i < asyncCallbacks.length; i++) {
      if (asyncCallbacks[i].init) {
        /* init(asyncId, provider, parentId, parentObject) */
        asyncCallbacks[i].init.call(asyncState, asyncId,
         async_wrap.Providers.NEXTTICK, undefined, undefined);
      }
    }
    return asyncState;
  }
  return undefined;
}

function notifyAsyncStart(asyncId, asyncState) {
  setCurrentAsyncId(asyncId);
  if (asyncState) {
    for (let i = 0; i < asyncCallbacks.length; i++) {
      if (asyncCallbacks[i].pre) {
        /* pre(asyncId); */
        asyncCallbacks[i].pre.call(asyncState, asyncId);
      }
    }
  }
}

function notifyAsyncEnd(asyncId, asyncState, callbackThrew) {
  if (asyncState) {
    for (let i = 0; i < asyncCallbacks.length; i++) {
      if (asyncCallbacks[i].post) {
        /* post(asyncId, didUserCodeThrow); */
        asyncCallbacks[i].post.call(asyncState, asyncId, callbackThrew);
      }
    }

    setCurrentAsyncId(0);

    for (let i = 0; i < asyncCallbacks.length; i++) {
      if (asyncCallbacks[i].destroy) {
        /* destroy(asyncId); */
        asyncCallbacks[i].destroy.call(undefined, asyncId);
      }
    }
  }
}

module.exports.incrementNextAsyncId = incrementNextAsyncId;
module.exports.getCurrentAsyncId = getCurrentAsyncId;
module.exports.setCurrentAsyncId = setCurrentAsyncId;
module.exports.callbacksEnabled = callbacksEnabled;
module.exports.notifyAsyncEnqueue = notifyAsyncEnqueue;
module.exports.notifyAsyncStart = notifyAsyncStart;
module.exports.notifyAsyncEnd = notifyAsyncEnd;
