import * as diffObj from './index'

const {diff, jsonPatchPathConverter} = diffObj;
const obj1 = {a: 2, b: 3};
const obj2 = {a: 2, c: 1};
const arr1 = [1, 'bee'];
const arr2 = [2, 'bee'];


//OK
diff(obj1, obj2);
diff(arr1, arr2);
diff(obj1, arr1);
diff(obj2, arr2);
diff(/yes/, arr1);
diff(new Date(), arr2);


diff(obj1, obj2, jsonPatchPathConverter);
diff(arr1, arr2, jsonPatchPathConverter);
diff(obj1, arr1, jsonPatchPathConverter);
diff(obj2, arr2, jsonPatchPathConverter);

// not OK
// @ts-expect-error
diff(obj1);
// @ts-expect-error
diff(arr2);
// @ts-expect-error
diff('a');
// @ts-expect-error
diff(true);

// @ts-expect-error
diff(obj1, 1);
// @ts-expect-error
diff(3, arr2);
// @ts-expect-error
diff(obj1, 'a');
// @ts-expect-error
diff('b', arr2);

// @ts-expect-error
diff('a', jsonPatchPathConverter);
// @ts-expect-error
diff(true, jsonPatchPathConverter);

// @ts-expect-error
diff(obj1, 1, jsonPatchPathConverter);
// @ts-expect-error
diff(3, arr2, jsonPatchPathConverter);
// @ts-expect-error
diff(obj1, 'a', jsonPatchPathConverter);
// @ts-expect-error
diff('b', arr2, jsonPatchPathConverter);

// @ts-expect-error
diff(obj1, obj2, 'a');
// @ts-expect-error
diff(arr1, arr2, 1);
// @ts-expect-error
diff(obj1, arr1, 'bee');
// @ts-expect-error
diff(obj2, arr2, 'nope');

