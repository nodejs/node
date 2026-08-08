var classicBinding = 'classic';

postMessage({
  binding: globalThis.classicBinding,
  requireType: typeof require,
  thisIsGlobal: this === globalThis,
});
