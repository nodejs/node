const SOURCES = {
  __proto__: null,
  'test:Array': ['1', '2'], // both `1,2` and `12` are valid ESM
  'test:ArrayBuffer': new ArrayBuffer(0),
  'test:null': null,
  'test:Object': {},
  'test:SharedArrayBuffer': new SharedArrayBuffer(0),
  'test:string': '',
  'test:String': new String(''),
  'test:Uint8Array': new Uint8Array(0),
  'test:undefined': undefined,
}
export function resolve(specifier, context, next) {
  if (specifier.startsWith('test:')) {
    return { url: specifier };
  }
  return next(specifier, context);
}

export function load(href, context, next) {
  if (href.startsWith('test:')) {
    return {
      format: 'module',
      source: SOURCES[href],
    };
  }
  return next(href, context);
}
