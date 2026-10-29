export function call1 (func, thisObj, arg0) {
  return func.call(thisObj, arg0);
}

export function call2 (func, thisObj, arg0, arg1) {
  return func.call(thisObj, arg0, arg1);
}

export function call3 (func, thisObj, arg0, arg1, arg2) {
  return func.call(thisObj, arg0, arg1, arg2);
}
