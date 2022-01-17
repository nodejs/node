/*
  const obj1 = {a: 3, b: 5};
  diffApply(obj1,
    [
      { "op": "remove", "path": ['b'] },
      { "op": "replace", "path": ['a'], "value": 4 },
      { "op": "add", "path": ['c'], "value": 5 }
    ]
  );
  obj1; // {a: 4, c: 5}

  // using converter to apply jsPatch standard paths
  // see http://jsonpatch.com
  import {diff, jsonPatchPathConverter} from 'just-diff'
  const obj2 = {a: 3, b: 5};
  diffApply(obj2, [
    { "op": "remove", "path": '/b' },
    { "op": "replace", "path": '/a', "value": 4 }
    { "op": "add", "path": '/c', "value": 5 }
  ], jsonPatchPathConverter);
  obj2; // {a: 4, c: 5}

  // arrays
  const obj3 = {a: 4, b: [1, 2, 3]};
  diffApply(obj3, [
    { "op": "replace", "path": ['a'], "value": 3 }
    { "op": "replace", "path": ['b', 2], "value": 4 }
    { "op": "add", "path": ['b', 3], "value": 9 }
  ]);
  obj3; // {a: 3, b: [1, 2, 4, 9]}

  // nested paths
  const obj4 = {a: 4, b: {c: 3}};
  diffApply(obj4, [
    { "op": "replace", "path": ['a'], "value": 5 }
    { "op": "remove", "path": ['b', 'c']}
    { "op": "add", "path": ['b', 'd'], "value": 4 }
  ]);
  obj4; // {a: 5, b: {d: 4}}
*/

var REMOVE = 'remove';
var REPLACE = 'replace';
var ADD = 'add';

function diffApply(obj, diff, pathConverter) {
  if (!obj || typeof obj != 'object') {
    throw new Error('base object must be an object or an array');
  }

  if (!Array.isArray(diff)) {
    throw new Error('diff must be an array');
  }

  var diffLength = diff.length;
  for (var i = 0; i < diffLength; i++) {
    var thisDiff = diff[i];
    var subObject = obj;
    var thisOp = thisDiff.op;
    var thisPath = thisDiff.path;
    if (pathConverter) {
      thisPath = pathConverter(thisPath);
      if (!Array.isArray(thisPath)) {
        throw new Error('pathConverter must return an array');
      }
    } else {
      if (!Array.isArray(thisPath)) {
        throw new Error(
          'diff path must be an array, consider supplying a path converter'
        );
      }
    }
    var pathCopy = thisPath.slice();
    var lastProp = pathCopy.pop();
    if (lastProp == null) {
      return false;
    }
    var thisProp;
    while ((thisProp = pathCopy.shift()) != null) {
      if (!(thisProp in subObject)) {
        subObject[thisProp] = {};
      }
      subObject = subObject[thisProp];
    }
    if (thisOp === REMOVE || thisOp === REPLACE) {
      if (!subObject.hasOwnProperty(lastProp)) {
        throw new Error(
          ['expected to find property', thisDiff.path, 'in object', obj].join(
            ' '
          )
        );
      }
    }
    if (thisOp === REMOVE) {
      Array.isArray(subObject)
        ? subObject.splice(lastProp, 1)
        : delete subObject[lastProp];
    }
    if (thisOp === REPLACE || thisOp === ADD) {
      subObject[lastProp] = thisDiff.value;
    }
  }
  return subObject;
}

function jsonPatchPathConverter(stringPath) {
  return stringPath.split('/').slice(1);
}

export {diffApply, jsonPatchPathConverter};
