## just-diff-apply

Part of a [library](../../../../) of zero-dependency npm modules that do just do one thing.
Guilt-free utilities for every occasion.

[Try it now](http://anguscroll.com/just/just-diff-apply)

Apply a diff object to an object.
Pass converter to apply a http://jsonpatch.com standard patch

```js
  import diffApply from 'just-diff-apply';

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
  import {diffApply, jsonPatchPathConverter} from 'just-diff-apply'
  const obj2 = {a: 3, b: 5};
  diffApply(obj2, [
    { "op": "remove", "path": '/b' },
    { "op": "replace", "path": '/a', "value": 4 }
    { "op": "add", "path": '/c', "value": 5 }
  ], jsonPatchPathConverter);
  obj2; // {a: 4, c: 5}

  // arrays (array key can be string or numeric)
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
```
