/* eslint-disable node/no-unsupported-features/es-syntax,
  jsdoc/require-jsdoc */

// We copy the regular expression so as to be able to always ensure the
//   exec expression is a global one (and thereby prevent recursion)

import mixinRegex from './mixinRegex.js';

class RegExtras {
  constructor (regex, flags, newLastIndex) {
    this.regex = mixinRegex(
      (typeof regex === 'string' ? new RegExp(regex) : mixinRegex(regex)),
      flags || '',
      newLastIndex
    );
  }

  forEach (str, cb, thisObj = null) {
    const regex = mixinRegex(this.regex, 'g');

    let matches, n0, i = 0;
    while ((matches = regex.exec(str)) !== null) {
      n0 = matches.splice(0, 1);
      cb.apply(thisObj, matches.concat(i++, n0));
    }
    return this;
  }

  some (str, cb, thisObj = null) {
    const regex = mixinRegex(this.regex, 'g');
    let matches, ret, n0, i = 0;
    while ((matches = regex.exec(str)) !== null) {
      n0 = matches.splice(0, 1);
      ret = cb.apply(thisObj, matches.concat(i++, n0));
      if (ret) {
        return true;
      }
    }
    return false;
  }

  every (str, cb, thisObj = null) {
    const regex = mixinRegex(this.regex, 'g');
    let matches, ret, n0, i = 0;
    while ((matches = regex.exec(str)) !== null) {
      n0 = matches.splice(0, 1);
      ret = cb.apply(thisObj, matches.concat(i++, n0));
      if (!ret) {
        return false;
      }
    }
    return true;
  }

  map (str, cb, thisObj = null) {
    const ret = [];
    const regex = mixinRegex(this.regex, 'g');
    let matches, n0, i = 0;
    while ((matches = regex.exec(str)) !== null) {
      n0 = matches.splice(0, 1);
      ret.push(cb.apply(thisObj, matches.concat(i++, n0)));
    }
    return ret;
  }

  filter (str, cb, thisObj = null) {
    let matches, n0, i = 0;
    const ret = [], regex = mixinRegex(this.regex, 'g');
    while ((matches = regex.exec(str)) !== null) {
      n0 = matches.splice(0, 1);
      matches = matches.concat(i++, n0);
      if (cb.apply(thisObj, matches)) {
        ret.push(n0[0]);
      }
    }
    return ret;
  }

  reduce (str, cb, prev, thisObj = null) {
    let matches, n0, i = 0;
    const regex = mixinRegex(this.regex, 'g');
    if (!prev) {
      if ((matches = regex.exec(str)) !== null) {
        n0 = matches.splice(0, 1);
        prev = cb.apply(thisObj, [''].concat(matches.concat(i++, n0)));
      }
    }
    while ((matches = regex.exec(str)) !== null) {
      n0 = matches.splice(0, 1);
      prev = cb.apply(thisObj, [prev].concat(matches.concat(i++, n0)));
    }
    return prev;
  }

  reduceRight (str, cb, prevOrig, thisObjOrig) {
    let matches, n0, i, thisObj = thisObjOrig, prev = prevOrig;
    const matchesContainer = [],
      regex = mixinRegex(this.regex, 'g');
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
      prev = cb.apply(thisObj, [''].concat(matches.concat(i--, n0)));
    }
    matchesContainer.reduceRight((container, mtches) => {
      n0 = mtches.splice(0, 1);
      prev = cb.apply(thisObj, [prev].concat(mtches.concat(i--, n0)));
      return container;
    }, matchesContainer);
    return prev;
  }

  find (str, cb, thisObj = null) {
    let matches, ret, n0, i = 0;
    const regex = mixinRegex(this.regex, 'g');
    while ((matches = regex.exec(str)) !== null) {
      n0 = matches.splice(0, 1);
      ret = cb.apply(thisObj, matches.concat(i++, n0));
      if (ret) {
        return n0[0];
      }
    }
    return false;
  }

  findIndex (str, cb, thisObj = null) {
    const regex = mixinRegex(this.regex, 'g');
    let matches, i = 0;
    while ((matches = regex.exec(str)) !== null) {
      const n0 = matches.splice(0, 1);
      const ret = cb.apply(thisObj, matches.concat(i++, n0));
      if (ret) {
        return i - 1;
      }
    }
    return -1;
  }

  findExec (str, cb, thisObj = null) {
    const regex = mixinRegex(this.regex, 'g');
    let matches, i = 0;
    while ((matches = regex.exec(str)) !== null) {
      const n0 = matches.splice(0, 1);
      const ret = cb.apply(thisObj, matches.concat(i++, n0));
      if (ret) {
        return matches;
      }
    }
    return false;
  }

  filterExec (str, cb, thisObj = null) {
    let matches, n0, i = 0;
    const ret = [], regex = mixinRegex(this.regex, 'g');
    while ((matches = regex.exec(str)) !== null) {
      n0 = matches.splice(0, 1);
      matches.push(i++, n0[0]);
      if (cb.apply(thisObj, matches)) {
        ret.push(matches);
      }
    }
    return ret;
  }
}

const _RegExtras = RegExtras;
RegExtras = function (...args) { // eslint-disable-line no-class-assign
  return new _RegExtras(...args);
};
RegExtras.prototype = _RegExtras.prototype;

RegExtras.mixinRegex = mixinRegex;

export {mixinRegex, RegExtras};
