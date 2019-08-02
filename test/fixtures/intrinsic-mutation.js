Object.defineProperty(
  String.prototype,
  Symbol('fake-polyfill-property'), {
    enumerable: false,
    value: null
  }
);
