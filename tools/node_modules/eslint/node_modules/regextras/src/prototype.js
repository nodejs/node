/* eslint-disable no-extend-native,
    no-use-extend-native/no-use-extend-native,
    node/no-unsupported-features/es-syntax */

// We copy the regular expression so as to be able to always ensure the
//   exec expression is a global one (and thereby prevent recursion)

import mixinRegex from './mixinRegex.js';

RegExp.prototype.forEach = function (str, cb, thisObj = null) {
  let matches, n0, i = 0;
  const regex = mixinRegex(this, 'g');
  while ((matches = regex.exec(str)) !== null) {
    n0 = matches.splice(0, 1);
    cb.apply(thisObj, matches.concat(i++, n0));
  }
  return this;
};

RegExp.prototype.some = function (str, cb, thisObj = null) {
  let matches, ret, n0, i = 0;
  const regex = mixinRegex(this, 'g');
  while ((matches = regex.exec(str)) !== null) {
    n0 = matches.splice(0, 1);
    ret = cb.apply(thisObj, matches.concat(i++, n0));
    if (ret) {
      return true;
    }
  }
  return false;
};

RegExp.prototype.every = function (str, cb, thisObj = null) {
  let matches, ret, n0, i = 0;
  const regex = mixinRegex(this, 'g');
  while ((matches = regex.exec(str)) !== null) {
    n0 = matches.splice(0, 1);
    ret = cb.apply(thisObj, matches.concat(i++, n0));
    if (!ret) {
      return false;
    }
  }
  return true;
};

RegExp.prototype.map = function (str, cb, thisObj = null) {
  let matches, n0, i = 0;
  const ret = [];
  const regex = mixinRegex(this, 'g');
  while ((matches = regex.exec(str)) !== null) {
    n0 = matches.splice(0, 1);
    ret.push(cb.apply(thisObj, matches.concat(i++, n0)));
  }
  return ret;
};

RegExp.prototype.filter = function (str, cb, thisObj = null) {
  let matches, n0, i = 0;
  const ret = [];
  const regex = mixinRegex(this, 'g');
  while ((matches = regex.exec(str)) !== null) {
    n0 = matches.splice(0, 1);
    matches = matches.concat(i++, n0);
    if (cb.apply(thisObj, matches)) {
      ret.push(matches[0]);
    }
  }
  return ret;
};

RegExp.prototype.reduce = function (str, cb, prev, thisObj = null) {
  let matches, n0, i = 0;
  const regex = mixinRegex(this, 'g');
  if (!prev) {
    if ((matches = regex.exec(str)) !== null) {
      n0 = matches.splice(0, 1);
      prev = cb.apply(thisObj, [''].concat(matches.concat(n0, i++)));
    }
  }
  while ((matches = regex.exec(str)) !== null) {
    n0 = matches.splice(0, 1);
    prev = cb.apply(thisObj, [prev].concat(matches.concat(n0, i++)));
  }
  return prev;
};

RegExp.prototype.reduceRight = function (str, cb, prevOrig, thisObjOrig) {
  let matches, n0, i,
    prev = prevOrig, thisObj = thisObjOrig;
  const regex = mixinRegex(this, 'g');
  const matchesContainer = [];
  thisObj = thisObj || null;
  while ((matches = regex.exec(str)) !== null) {
    matchesContainer.push(matches);
  }
  i = matchesContainer.length;
  if (!i) {
    if (arguments.length < 3) {
      throw new TypeError(
        'reduce of empty matches array with no initial value'
      );
    }
    return prev;
  }
  if (!prev) {
    matches = matchesContainer.splice(-1)[0];
    n0 = matches.splice(0, 1);
    prev = cb.apply(thisObj, [''].concat(matches.concat(n0, i--)));
  }
  matchesContainer.reduceRight(function (container, mtches) {
    n0 = mtches.splice(0, 1);
    prev = cb.apply(thisObj, [prev].concat(mtches.concat(n0, i--)));
    return container;
  }, matchesContainer);
  return prev;
};
