let wasm;

const isLE = new Uint8Array(new Uint16Array([1]).buffer)[0] === 1;

export function parse (source, name = '@') {
  if (!wasm)
    throw new Error('Not initialized');

  const len = source.length + 1;

  // need 2 bytes per code point plus analysis space so we double again
  const extraMem = (wasm.__heap_base.value || wasm.__heap_base) + len * 4 - wasm.memory.buffer.byteLength;
  if (extraMem > 0)
    wasm.memory.grow(Math.ceil(extraMem / 65536));
    
  const addr = wasm.sa(len);
  (isLE ? copyLE : copyBE)(source, new Uint16Array(wasm.memory.buffer, addr, len));

  const err_code = wasm.parseCJS(addr, source.length, 0, 0, 0);

  if (err_code) {
    const err = new Error(`Parse error ${name}${wasm.e()}:${source.slice(0, wasm.e()).split('\n').length}:${wasm.e() - source.lastIndexOf('\n', wasm.e() - 1)}`);
    Object.assign(err, { idx: wasm.e() });
    if (err_code === 5 || err_code === 6 || err_code === 7)
      Object.assign(err, { code: 'ERR_LEXER_ESM_SYNTAX' });
    throw err;
  }

  let exports = new Set(), reexports = new Set(), unsafeGetters = new Set();
  
  while (wasm.rre()) {
    const reexptStr = decode(source.slice(wasm.res(), wasm.ree()));
    if (reexptStr)
      reexports.add(reexptStr);
  }
  while (wasm.ru())
    unsafeGetters.add(decode(source.slice(wasm.us(), wasm.ue())));
  while (wasm.re()) {
    let exptStr = decode(source.slice(wasm.es(), wasm.ee()));
    if (exptStr !== undefined && !unsafeGetters.has(exptStr))
      exports.add(exptStr);
  }

  return { exports: [...exports], reexports: [...reexports] };
}

function decode (str) {
  if (str[0] === '"' || str[0] === '\'') {
    try {
      const decoded = scanStringLiteral(str);
      // Filter to exclude non-matching UTF-16 surrogate strings
      for (let i = 0; i < decoded.length; i++) {
        const surrogatePrefix = decoded.charCodeAt(i) & 0xFC00;
        if (surrogatePrefix < 0xD800) {
          // Not a surrogate
          continue;
        }
        else if (surrogatePrefix === 0xD800) {
          // Validate surrogate pair
          if ((decoded.charCodeAt(++i) & 0xFC00) !== 0xDC00)
            return;
        }
        else {
          // Out-of-range surrogate code (above 0xD800)
          return;
        }
      }
      return decoded;
    }
    catch {}
  }
  else {
    return str;
  }
}


function scanStringLiteral (source) {
  const quote = source[0];

  // try JSON.parse first for performance
  if (quote === '"') {
    try {
      return JSON.parse(source);
    } catch {
      // ignored
    }
  } else if (quote === "'" && source.length > 1 && source[source.length - 1] === "'" && source.indexOf('"') === -1) {
    try {
      return JSON.parse('"' + source.slice(1, -1) + '"');
    } catch {
      // ignored
    }
  }

  // fall back to doing it the hard way
  let parsed = '';
  let index = { v: 1 };

  while (index.v < source.length) {
    const char = source[index.v];
    switch (char) {
      case quote: {
        return parsed;
      }
      case '\\': {
        ++index.v;
        parsed += scanEscapeSequence(source, index);
        break;
      }
      case '\r':
      case '\n': {
        throw new SyntaxError();
      }
      default: {
        ++index.v;
        parsed += char;
      }
    }
  }

  throw new SyntaxError();
}

function scanEscapeSequence (source, index) {
  if (index.v === source.length) {
    throw new SyntaxError();
  }
  const char = source[index.v];
  ++index.v;
  switch (char) {
    case '\r': {
      if (source[index.v] === '\n') {
        ++index.v;
      }
      // fall through
    }
    case '\n':
    case '\u2028':
    case '\u2029': {
      return '';
    }
    case 'r': {
      return '\r';
    }
    case 'n': {
      return '\n';
    }
    case 't': {
      return '\t';
    }
    case 'b': {
      return '\b';
    }
    case 'f': {
      return '\f';
    }
    case 'v': {
      return '\v';
    }
    case 'x': {
      return scanHexEscapeSequence(source, index);
    }
    case 'u': {
      return scanUnicodeEscapeSequence(source, index);
    }
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7': {
      return scanOctalEscapeSequence(char, source, index);
    }
    default: {
      return char;
    }
  }
}

function scanHexEscapeSequence (source, index) {
  const a = readHex(source[index.v]);
  ++index.v;
  const b = readHex(source[index.v]);
  ++index.v;
  return String.fromCodePoint(a * 16 + b);
}

function scanUnicodeEscapeSequence (source, index) {
  let result = 0;
  if (source[index.v] === '{') {
    ++index.v;
    do {
      result = result * 16 + readHex(source[index.v]);
      if (result > 0x10FFFF) {
        throw new SyntaxError();
      }
      ++index.v;
    } while (source[index.v] !== '}');
    ++index.v;
  } else {
    for (let i = 0; i < 4; ++i) {
      result = result * 16 + readHex(source[index.v]);
      ++index.v;
    }
  }
  return String.fromCodePoint(result);
}

function scanOctalEscapeSequence (char, source, index) {
  let toRead = char <= '3' ? 2 : 1;
  let result = +char;
  do {
    char = source[index.v];
    if (char < '0' || char > '7') {
      break;
    }
    result = result * 8 + (+char);
    ++index.v;
    --toRead;
  } while (toRead > 0);
  return String.fromCodePoint(result);
}

function readHex (char) {
  if (char >= '0' && char <= '9') {
    return +char;
  } else if (char >= 'a' && char <= 'f') {
    return char.charCodeAt(0) - 87;
  } else if (char >= 'A' && char <= 'F') {
    return char.charCodeAt(0) - 55;
  }
  throw new SyntaxError();
}



function copyBE (src, outBuf16) {
  const len = src.length;
  let i = 0;
  while (i < len) {
    const ch = src.charCodeAt(i);
    outBuf16[i++] = (ch & 0xff) << 8 | ch >>> 8;
  }
}

function copyLE (src, outBuf16) {
  const len = src.length;
  let i = 0;
  while (i < len)
    outBuf16[i] = src.charCodeAt(i++);
}

function getWasmBytes() {
  const binary = 'WASM_BINARY';  // This string will be replaced by build.js.
  if (typeof Buffer !== 'undefined')
    return Buffer.from(binary, 'base64');
  return Uint8Array.from(atob(binary), x => x.charCodeAt(0));
}

let initPromise;
export function init () {
  if (initPromise)
    return initPromise;
  return initPromise = (async () => {
    const compiled = await WebAssembly.compile(getWasmBytes());
    const { exports } = await WebAssembly.instantiate(compiled);
    wasm = exports;
  })();
}

export function initSync () {
  if (wasm) {
    return;
  }
  const compiled = new WebAssembly.Module(getWasmBytes());
  const { exports } = new WebAssembly.Instance(compiled);
  wasm = exports;
  return;
}
