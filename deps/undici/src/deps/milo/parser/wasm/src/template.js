/* REPLACE: module */

const textEncoder = new TextEncoder()
const textDecoder = new TextDecoder()

function log (logger, raw) {
  const len = Number(BigInt.asUintN(32, raw))
  const ptr = Number(raw >> 32n)

  logger(textDecoder.decode(new Uint8Array(this.memory.buffer, ptr, len)))
}

function alloc (len) {
  return this.alloc(len) >>> 0
}

function dealloc (ptr, len) {
  return this.dealloc(ptr, len)
}

function create () {
  return this.create() >>> 0
}

function destroy (parser) {
  this.destroy(parser)
}

function parse (parser, data, limit) {
  return this.parse(parser, data, limit) >>> 0
}

function fail (parser, code, description) {
  const len = description.length
  const ptr = this.alloc(len)
  const buffer = new Uint8Array(this.memory.buffer, ptr, len)
  textEncoder.encodeInto(description, buffer)

  this.fail(parser, code, ptr, len)
  this.dealloc(ptr, len)
}

function hasDebug () {
  return this.milo_has_debug() !== 0
}

/* REPLACE: enums */

/* REPLACE: getters */

/* REPLACE: setters */

function simpleCreate (spans, create) {
  const parser = create()
  spans[parser] = []
  this.setActiveCallbacks(parser, this.CALLBACK_ACTIVE_ALL)
  return parser
}

function simpleDestroy (spans, destroy, parser) {
  spans[parser] = undefined
  destroy(parser)
}

export function noop () {}

export function setup (env = {}) {
  let { logger: logOption, ...instanceEnvironment } = env
  let logger = noop
  const context = {}

  if (logOption) {
    if (typeof logOption !== 'function') {
      logOption = console.log
    }

    logger = log.bind(context, logOption)
  }

  // Create the WASM instance
  /* eslint-disable-next-line no-undef */
  const instance = new WebAssembly.Instance(wasmModule, {
    env: {
      logger,
      /* REPLACE: callbacks:noop */
      ...instanceEnvironment
    }
  })

  const wasm = instance.exports
  context.memory = wasm.memory

  const milo = {
    /* REPLACE: version */
    /* REPLACE: constants */
    /* REPLACE: enums:list */
    /* REPLACE: getters:list */
    /* REPLACE: setters:list */
    memory: wasm.memory,
    alloc: alloc.bind(wasm),
    dealloc: dealloc.bind(wasm),
    create: create.bind(wasm),
    destroy: destroy.bind(wasm),
    parse: parse.bind(wasm),
    fail: fail.bind(wasm),
    hasDebug: hasDebug.bind(wasm),
    clear: wasm.clear,
    complete: wasm.complete,
    finish: wasm.finish,
    pause: wasm.pause,
    reset: wasm.reset,
    resume: wasm.resume
  }

  /* REPLACE: start */
  return milo
}

export function simple () {
  const spans = {}

  const milo = setup({/* REPLACE: callbacks:simple */})

  milo.spans = spans
  milo.create = simpleCreate.bind(milo, spans, milo.create)
  milo.destroy = simpleDestroy.bind(milo, spans, milo.destroy)

  return milo
}
