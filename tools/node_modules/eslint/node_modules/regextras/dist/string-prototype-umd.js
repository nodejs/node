(function (factory) {
  typeof define === 'function' && define.amd ? define(factory) :
  factory();
}((function () { 'use strict';

  /* eslint-disable node/no-unsupported-features/es-syntax */

  /**
   * @param {RegExp} regex
   * @param {string} newFlags
   * @param {Integer} [newLastIndex=regex.lastIndex]
   * @returns {RegExp}
   */
  function mixinRegex(regex, newFlags) {
    var newLastIndex = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : regex.lastIndex;
    newFlags = newFlags || '';
    regex = new RegExp(regex.source, (newFlags.includes('g') ? 'g' : regex.global ? 'g' : '') + (newFlags.includes('i') ? 'i' : regex.ignoreCase ? 'i' : '') + (newFlags.includes('m') ? 'm' : regex.multiline ? 'm' : '') + (newFlags.includes('u') ? 'u' : regex.unicode ? 'u' : '') + (newFlags.includes('y') ? 'y' : regex.sticky ? 'y' : '') + (newFlags.includes('s') ? 's' : regex.dotAll ? 's' : ''));
    regex.lastIndex = newLastIndex;
    return regex;
  }

  // We copy the regular expression so as to be able to always ensure the exec

  String.prototype.forEach = function (regex, cb) {
    var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
    var matches,
        n0,
        i = 0;
    regex = mixinRegex(regex, 'g');

    while ((matches = regex.exec(this)) !== null) {
      n0 = matches.splice(0, 1);
      cb.apply(thisObj, matches.concat(i++, n0));
    }

    return this;
  };

  String.prototype.some = function (regex, cb) {
    var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
    var matches,
        ret,
        n0,
        i = 0;
    regex = mixinRegex(regex, 'g');

    while ((matches = regex.exec(this)) !== null) {
      n0 = matches.splice(0, 1);
      ret = cb.apply(thisObj, matches.concat(i++, n0));

      if (ret) {
        return true;
      }
    }

    return false;
  };

  String.prototype.every = function (regex, cb) {
    var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
    var matches,
        ret,
        n0,
        i = 0;
    regex = mixinRegex(regex, 'g');

    while ((matches = regex.exec(this)) !== null) {
      n0 = matches.splice(0, 1);
      ret = cb.apply(thisObj, matches.concat(i++, n0));

      if (!ret) {
        return false;
      }
    }

    return true;
  };

  String.prototype.map = function (regex, cb) {
    var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
    var matches,
        n0,
        i = 0;
    var ret = [];
    regex = mixinRegex(regex, 'g');

    while ((matches = regex.exec(this)) !== null) {
      n0 = matches.splice(0, 1);
      ret.push(cb.apply(thisObj, matches.concat(i++, n0)));
    }

    return ret;
  };

  String.prototype.filter = function (regex, cb) {
    var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
    var matches,
        n0,
        i = 0;
    var ret = [];
    regex = mixinRegex(regex, 'g');

    while ((matches = regex.exec(this)) !== null) {
      n0 = matches.splice(0, 1);
      matches = matches.concat(i++, n0);

      if (cb.apply(thisObj, matches)) {
        ret.push(n0[0]);
      }
    }

    return ret;
  };

  String.prototype.reduce = function (regex, cb, prev) {
    var thisObj = arguments.length > 3 && arguments[3] !== undefined ? arguments[3] : null;
    var matches,
        n0,
        i = 0;
    regex = mixinRegex(regex, 'g');

    if (!prev) {
      if ((matches = regex.exec(this)) !== null) {
        n0 = matches.splice(0, 1);
        prev = cb.apply(thisObj, [''].concat(matches.concat(i++, n0)));
      }
    }

    while ((matches = regex.exec(this)) !== null) {
      n0 = matches.splice(0, 1);
      prev = cb.apply(thisObj, [prev].concat(matches.concat(i++, n0)));
    }

    return prev;
  };

  String.prototype.reduceRight = function (regex, cb, prevOrig, thisObjOrig) {
    var matches,
        n0,
        i,
        prev = prevOrig,
        thisObj = thisObjOrig;
    var matchesContainer = [];
    regex = mixinRegex(regex, 'g');
    thisObj = thisObj || null;

    while ((matches = regex.exec(this)) !== null) {
      matchesContainer.push(matches);
    }

    i = matchesContainer.length;

    if (!i) {
      if (arguments.length < 3) {
        throw new TypeError('reduce of empty matches array with no initial value');
      }

      return prev;
    }

    if (!prev) {
      matches = matchesContainer.splice(-1)[0];
      n0 = matches.splice(0, 1);
      prev = cb.apply(thisObj, [''].concat(matches.concat(i--, n0)));
    }

    matchesContainer.reduceRight(function (container, mtches) {
      n0 = mtches.splice(0, 1);
      prev = cb.apply(thisObj, [prev].concat(mtches.concat(i--, n0)));
      return container;
    }, matchesContainer);
    return prev;
  };

  String.prototype.find = function (regex, cb) {
    var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
    var matches,
        ret,
        n0,
        i = 0;
    regex = mixinRegex(regex, 'g');

    while ((matches = regex.exec(this)) !== null) {
      n0 = matches.splice(0, 1);
      ret = cb.apply(thisObj, matches.concat(i++, n0));

      if (ret) {
        return n0[0];
      }
    }

    return false;
  };

  String.prototype.findIndex = function (regex, cb) {
    var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
    var matches,
        ret,
        n0,
        i = 0;
    regex = mixinRegex(regex, 'g');

    while ((matches = regex.exec(this)) !== null) {
      n0 = matches.splice(0, 1);
      ret = cb.apply(thisObj, matches.concat(i++, n0));

      if (ret) {
        return i - 1;
      }
    }

    return -1;
  };

  String.prototype.findExec = function (regex, cb) {
    var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
    var matches,
        ret,
        n0,
        i = 0;
    regex = mixinRegex(regex, 'g');

    while ((matches = regex.exec(this)) !== null) {
      n0 = matches.splice(0, 1);
      ret = cb.apply(thisObj, matches.concat(i++, n0));

      if (ret) {
        return matches;
      }
    }

    return false;
  };

  String.prototype.filterExec = function (regex, cb) {
    var thisObj = arguments.length > 2 && arguments[2] !== undefined ? arguments[2] : null;
    var matches,
        n0,
        i = 0;
    var ret = [];
    regex = mixinRegex(regex, 'g');

    while ((matches = regex.exec(this)) !== null) {
      n0 = matches.splice(0, 1);
      matches.push(i++, n0[0]);

      if (cb.apply(thisObj, matches)) {
        ret.push(matches);
      }
    }

    return ret;
  };

})));
