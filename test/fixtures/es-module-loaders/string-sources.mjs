const SOURCES = {
  __proto__: null,
  'test:Array': ['1', '2'], // both `1,2` and `12` are valid ESM
  'test:ArrayBuffer': new ArrayBuffer(0),
  'test:BigInt64Array': new BigInt64Array(0),
  'test:BigUint64Array': new BigUint64Array(0),
  'test:Float32Array': new Float32Array(0),
  'test:Float64Array': new Float64Array(0),
  'test:Int8Array': new Int8Array(0),
  'test:Int16Array': new Int16Array(0),
  'test:Int32Array': new Int32Array(0),
  'test:null': null,
  'test:Object': {},
  'test:SharedArrayBuffer': new SharedArrayBuffer(0),
  'test:string': '',
  'test:String': new String(''),
  'test:Uint8Array': new Uint8Array(0),
  'test:Uint8ClampedArray': new Uint8ClampedArray(0),
  'test:Uint16Array': new Uint16Array(0),
  'test:Uint32Array': new Uint32Array(0),
  'test:undefined': undefined,
}
export function resolve(specifier, context, next) {
  if (specifier.startsWith('test:')) {
    return {
      importAssertions: context.importAssertions,
      shortCircuit: true,
      url: specifier,
    };
  }
  return next(specifier, context);
}

export function load(href, context, next) {
  if (href.startsWith('test:')) {
    return {
      format: 'module',
      shortCircuit: true,
      source: SOURCES[href],
    };
  }
  return next(href, context);
}
