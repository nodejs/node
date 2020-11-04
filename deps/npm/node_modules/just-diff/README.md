## just-diff

Part of a [library](../../../../) of zero-dependency npm modules that do just do one thing.  
Guilt-free utilities for every occasion.

[Try it now](http://anguscroll.com/just/just-diff)

Return an object representing the difference between two other objects  
Pass converter to format as http://jsonpatch.com

```js
import {diff} from 'just-diff';

const obj1 = {a: 4, b: 5};
const obj2 = {a: 3, b: 5};
const obj3 = {a: 4, c: 5};

diff(obj1, obj2);
[
  { "op": "replace", "path": ['a'], "value": 3 }
]

diff(obj2, obj3);
[
  { "op": "remove", "path": ['b'] },
  { "op": "replace", "path": ['a'], "value": 4 }
  { "op": "add", "path": ['c'], "value": 5 }
]

// using converter to generate jsPatch standard paths
import {diff, jsonPatchPathConverter} from 'just-diff'
diff(obj1, obj2, jsonPatchPathConverter);
[
  { "op": "replace", "path": '/a', "value": 3 }
]

diff(obj2, obj3, jsonPatchPathConverter);
[
  { "op": "remove", "path": '/b' },
  { "op": "replace", "path": '/a', "value": 4 }
  { "op": "add", "path": '/c', "value": 5 }
]

// arrays
const obj4 = {a: 4, b: [1, 2, 3]};
const obj5 = {a: 3, b: [1, 2, 4]};
const obj6 = {a: 3, b: [1, 2, 4, 5]};

diff(obj4, obj5);
[
  { "op": "replace", "path": ['a'], "value": 3 }
  { "op": "replace", "path": ['b', 2], "value": 4 }
]

diff(obj5, obj6);
[
  { "op": "add", "path": ['b', 3], "value": 5 }
]

// nested paths
const obj7 = {a: 4, b: {c: 3}};
const obj8 = {a: 4, b: {c: 4}};
const obj9 = {a: 5, b: {d: 4}};

diff(obj7, obj8);
[
  { "op": "replace", "path": ['b', 'c'], "value": 4 }
]

diff(obj8, obj9);
[
  { "op": "replace", "path": ['a'], "value": 5 }
  { "op": "remove", "path": ['b', 'c']}
  { "op": "add", "path": ['b', 'd'], "value": 4 }
]
```
