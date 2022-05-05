import * as diffObj from "./index";

const { diffApply, jsonPatchPathConverter } = diffObj;
const obj1 = {
  a: 2,
  b: 3,
  c: {
    d: 5
  }
};
const arr1 = [1, "bee"];

const objOps: diffObj.DiffOps = [
  {
    op: "replace",
    path: ["a"],
    value: 10
  },
  {
    op: "remove",
    path: ["b"]
  },
  {
    op: "add",
    path: ["e"],
    value: 15
  },
  {
    op: "remove",
    path: ["c", "d"]
  }
];

const arrOps: diffObj.DiffOps = [
  {
    op: "replace",
    path: [1],
    value: 10
  },
  {
    op: "remove",
    path: [2]
  },
  {
    op: "add",
    path: [7],
    value: 15
  }
];

//OK
diffApply(obj1, objOps);
diffApply(obj1, []);
diffApply(arr1, arrOps);
diffApply(arr1, []);
diffApply(obj1, objOps, jsonPatchPathConverter);
diffApply(arr1, arrOps, jsonPatchPathConverter);

// not OK
// @ts-expect-error
diffApply(obj1);
// @ts-expect-error
diffApply(arr2);
// @ts-expect-error
diffApply("a");
// @ts-expect-error
diffApply(true);

// @ts-expect-error
diffApply(obj1, 1);
// @ts-expect-error
diffApply(3, arr2);
// @ts-expect-error
diffApply(obj1, "a");
// @ts-expect-error
diffApply("b", arr2);

// @ts-expect-error
diffApply(obj1, [{ op: "delete", path: ["a"] }]);
// @ts-expect-error
diffApply(obj1, [{ op: "delete", path: ["a"] }], jsonPatchPathConverter);
// @ts-expect-error
diffApply(obj1, "a", jsonPatchPathConverter);
// @ts-expect-error
diffApply(obj1, ["a", "b", "c"], jsonPatchPathConverter);

// @ts-expect-error
diff("a", jsonPatchPathConverter);
// @ts-expect-error
diff(true, jsonPatchPathConverter);

// @ts-expect-error
diff(obj1, 1, jsonPatchPathConverter);
// @ts-expect-error
diff(3, arr2, jsonPatchPathConverter);
// @ts-expect-error
diff(obj1, "a", jsonPatchPathConverter);
// @ts-expect-error
diff("b", arr2, jsonPatchPathConverter);

// @ts-expect-error
diff(obj1, obj2, "a");
// @ts-expect-error
diff(arr1, arr2, 1);
// @ts-expect-error
diff(obj1, arr1, "bee");
// @ts-expect-error
diff(obj2, arr2, "nope");
