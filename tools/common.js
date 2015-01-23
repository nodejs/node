// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

var assert = require('assert');

/*
 * stableSort sorts the array "array" using the predicate "compare" in a
 * stable way.
 * It returns a new object that represents the sorted array.
 */
function stableSort(array, compare) {
  assert(Array.isArray(array), 'array must be an array');

  if (!compare) {
    compare = defaultCompare;
  }
  assert(typeof compare === 'function', 'compare must be a function');

  var indexedArray = array.map(function addIndex(item, index) {
    return { v: item, i: index };
  });

  function defaultCompare(a, b) {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
  }

  indexedArray.sort(function stableCompare(a, b) {
    var r = compare(a.v, b.v);
    if (r === 0) {
      return a.i - b.i;
    } else {
      return r;
    }
  });

  return indexedArray.map(function removeIndex(item) {
    return item.v;
  });
}

exports.stableSort = stableSort;

if (module === require.main) {
  /*
   * The following code is a tests suite that makes sure that:
   * 1) The default sort implementation of the Node.js JavaScript runtime (V8)
   * is not stable.
   * 2) The "stableSort" function above provides a stable sort.
   */
  function standardSort(array, compare) {
    var dupArray = array.splice(0);
    dupArray.sort(compare);
    return dupArray;
  }

  function testSortStability(count, bins, sortFunc) {
    var unstable = false;

    for (var iter = 0; iter < 10; ++iter) {
      var array = [];

      for (var i = 0; i < count; ++i)
        array.push({ u: Math.floor(Math.random() * bins), i: i });

      array = sortFunc(array, function(a, b) { return a.u - b.u });
      var u = -1, i = -1;
      for (var ii = 0; ii < count; ++ii) {
        var v = array[ii];
        if (v.u > u) {
          u = v.u; i = -1;
        } else if (v.i > i) {
          i = v.i;
        } else {
          return false;
        }
      }
    }

    return true;
  }

  var stableSortMsg = "Stable sort should be stable for all arrays";
  var standardSmallArrayMsg = "Standard sort should be stable for small arrays";
  var standardUnstableMsg = "Standard sort should NOT be stable for non-small arrays";

  var stableSorts = [
    { count: 5,     bins: 2, sort: standardSort,  msg: standardSmallArrayMsg },
    { count: 10,    bins: 3, sort: standardSort,  msg: standardSmallArrayMsg },
    { count: 5,     bins: 2, sort: stableSort,    msg: stableSortMsg         },
    { count: 10,    bins: 3, sort: stableSort,    msg: stableSortMsg         },
    { count: 11,    bins: 3, sort: stableSort,    msg: stableSortMsg         },
    { count: 12,    bins: 3, sort: stableSort,    msg: stableSortMsg         },
    { count: 13,    bins: 3, sort: stableSort,    msg: stableSortMsg         },
    { count: 14,    bins: 3, sort: stableSort,    msg: stableSortMsg         },
    { count: 15,    bins: 3, sort: stableSort,    msg: stableSortMsg         },
    { count: 20,    bins: 3, sort: stableSort,    msg: stableSortMsg         },
    { count: 50,    bins: 3, sort: stableSort,    msg: stableSortMsg         },
    { count: 100,   bins: 4, sort: stableSort,    msg: stableSortMsg         },
    { count: 1000,  bins: 4, sort: stableSort,    msg: stableSortMsg         },
    { count: 10000, bins: 4, sort: stableSort,    msg: stableSortMsg         },
  ];

  var unstableSorts = [
    { count: 11,    bins: 3, sort: standardSort, msg: standardUnstableMsg },
    { count: 12,    bins: 3, sort: standardSort, msg: standardUnstableMsg },
    { count: 13,    bins: 3, sort: standardSort, msg: standardUnstableMsg },
    { count: 14,    bins: 3, sort: standardSort, msg: standardUnstableMsg },
    { count: 15,    bins: 3, sort: standardSort, msg: standardUnstableMsg },
    { count: 20,    bins: 3, sort: standardSort, msg: standardUnstableMsg },
    { count: 50,    bins: 4, sort: standardSort, msg: standardUnstableMsg },
    { count: 100,   bins: 4, sort: standardSort, msg: standardUnstableMsg },
    { count: 1000,  bins: 4, sort: standardSort, msg: standardUnstableMsg },
    { count: 10000, bins: 4, sort: standardSort, msg: standardUnstableMsg },
  ];


  function runTests(tests, stability) {
    tests.forEach(function(test) {
      console.log('Running test with sort: %s, count: %s, bins: %s',
                  test.sort.name, test.count, test.bins);
      assert.equal(testSortStability(test.count, test.bins, test.sort),
                   stability,
                   test.msg);
    });
  }

  runTests(stableSorts, true);
  runTests(unstableSorts, false);
}
