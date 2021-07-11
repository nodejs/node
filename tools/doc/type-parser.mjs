const jsDocPrefix = 'https://developer.mozilla.org/en-US/docs/Web/JavaScript/';

const jsDataStructuresUrl = `${jsDocPrefix}Data_structures`;
const jsPrimitives = {
  boolean: 'Boolean',
  integer: 'Number', // Not a primitive, used for clarification.
  null: 'Null',
  number: 'Number',
  string: 'String',
  symbol: 'Symbol',
  undefined: 'Undefined'
};

const jsGlobalObjectsUrl = `${jsDocPrefix}Reference/Global_Objects/`;
const jsGlobalTypes = [
  'AggregateError', 'Array', 'ArrayBuffer', 'DataView', 'Date', 'Error',
  'EvalError', 'Function', 'Map', 'Object', 'Promise', 'RangeError',
  'ReferenceError', 'RegExp', 'Set', 'SharedArrayBuffer', 'SyntaxError',
  'TypeError', 'TypedArray', 'URIError', 'Uint8Array',
];

const customTypesMap = {
  'any': `${jsDataStructuresUrl}#Data_types`,

  'this': `${jsDocPrefix}Reference/Operators/this`,

  'AbortController': 'globals.html#globals_class_abortcontroller',
  'AbortSignal': 'globals.html#globals_class_abortsignal',

  'ArrayBufferView':
    'https://developer.mozilla.org/en-US/docs/Web/API/ArrayBufferView',

  'AsyncIterator': 'https://tc39.github.io/ecma262/#sec-asynciterator-interface',

  'AsyncIterable': 'https://tc39.github.io/ecma262/#sec-asynciterable-interface',

  'bigint': `${jsDocPrefix}Reference/Global_Objects/BigInt`,
  'WebAssembly.Instance':
    `${jsDocPrefix}Reference/Global_Objects/WebAssembly/Instance`,

  'Blob': 'buffer.html#buffer_class_blob',

  'BroadcastChannel':
    'worker_threads.html#worker_threads_class_broadcastchannel_' +
    'extends_eventtarget',

  'Iterable':
    `${jsDocPrefix}Reference/Iteration_protocols#The_iterable_protocol`,
  'Iterator':
    `${jsDocPrefix}Reference/Iteration_protocols#The_iterator_protocol`,

  'Module Namespace Object':
    'https://tc39.github.io/ecma262/#sec-module-namespace-exotic-objects',

  'AsyncHook': 'async_hooks.html#async_hooks_async_hooks_createhook_callbacks',
  'AsyncResource': 'async_hooks.html#async_hooks_class_asyncresource',

  'brotli options': 'zlib.html#zlib_class_brotlioptions',

  'Buffer': 'buffer.html#buffer_class_buffer',

  'ChildProcess': 'child_process.html#child_process_class_childprocess',

  'cluster.Worker': 'cluster.html#cluster_class_worker',

  'Cipher': 'crypto.html#crypto_class_cipher',
  'Decipher': 'crypto.html#crypto_class_decipher',
  'DiffieHellman': 'crypto.html#crypto_class_diffiehellman',
  'DiffieHellmanGroup': 'crypto.html#crypto_class_diffiehellmangroup',
  'ECDH': 'crypto.html#crypto_class_ecdh',
  'Hash': 'crypto.html#crypto_class_hash',
  'Hmac': 'crypto.html#crypto_class_hmac',
  'KeyObject': 'crypto.html#crypto_class_keyobject',
  'Sign': 'crypto.html#crypto_class_sign',
  'Verify': 'crypto.html#crypto_class_verify',
  'crypto.constants': 'crypto.html#crypto_crypto_constants_1',

  'CryptoKey': 'webcrypto.html#webcrypto_class_cryptokey',
  'CryptoKeyPair': 'webcrypto.html#webcrypto_class_cryptokeypair',
  'Crypto': 'webcrypto.html#webcrypto_class_crypto',
  'SubtleCrypto': 'webcrypto.html#webcrypto_class_subtlecrypto',
  'RsaOaepParams': 'webcrypto.html#webcrypto_class_rsaoaepparams',
  'AesCtrParams': 'webcrypto.html#webcrypto_class_aesctrparams',
  'AesCbcParams': 'webcrypto.html#webcrypto_class_aescbcparams',
  'AesGcmParams': 'webcrypto.html#webcrypto_class_aesgcmparams',
  'AesKwParams': 'webcrypto.html#webcrypto_class_aeskwparams',
  'EcdhKeyDeriveParams': 'webcrypto.html#webcrypto_class_ecdhkeyderiveparams',
  'HkdfParams': 'webcrypto.html#webcrypto_class_hkdfparams',
  'Pbkdf2Params': 'webcrypto.html#webcrypto_class_pbkdf2params',
  'HmacKeyGenParams': 'webcrypto.html#webcrypto_class_hmackeygenparams',
  'AesKeyGenParams': 'webcrypto.html#webcrypto_class_aeskeygenparams',
  'RsaHashedKeyGenParams':
    'webcrypto.html#webcrypto_class_rsahashedkeygenparams',
  'EcKeyGenParams': 'webcrypto.html#webcrypto_class_eckeygenparams',
  'RsaHashedImportParams':
    'webcrypto.html#webcrypto_class_rsahashedimportparams',
  'EcKeyImportParams': 'webcrypto.html#webcrypto_class_eckeyimportparams',
  'HmacImportParams': 'webcrypto.html#webcrypto_class_hmacimportparams',
  'AesImportParams': 'webcrypto.html#webcrypto_class_aesimportparams',
  'Pbkdf2ImportParams': 'webcrypto.html#webcrypto_class.pbkdf2importparams',
  'HmacParams': 'webcrypto.html#webcrypto_class_hmacparams',
  'EcdsaParams': 'webcrypto.html#webcrypto_class_ecdsaparams',
  'RsaPssParams': 'webcrypto.html#webcrypto_class_rsapssparams',
  'RsaSignParams': 'webcrypto.html#webcrypto_class_rsasignparams',
  'NodeDhImportParams': 'webcrypto.html#webcrypto_class_nodedhimportparams',
  'NodeDhKeyGenParams': 'webcrypto.html#webcrypto_class_nodedhkeygenparams',
  'NodeDhDeriveBitsParams':
    'webcrypto.html#webcrypto_class_nodedhderivebitsparams',
  'NodeDsaImportParams': 'webcrypto.html#webcrypto_class_nodedsaimportparams',
  'NodeDsaKeyGenParams': 'webcrypto.html#webcrypto_class_nodedsakeygenparams',
  'NodeDsaSignParams': 'webcrypto.html#webcrypto_class_nodedsasignparams',
  'NodeScryptImportParams':
    'webcrypto.html#webcrypto_class_nodescryptimportparams',
  'NodeScryptParams': 'webcrypto.html#webcrypto_class_nodescryptparams',
  'NodeEdKeyImportParams':
    'webcrypto.html#webcrypto_class_nodeedkeyimportparams',
  'NodeEdKeyGenParams':
    'webcrypto.html#webcrypto_class_nodeedkeygenparams',

  'dgram.Socket': 'dgram.html#dgram_class_dgram_socket',

  'Channel': 'diagnostics_channel.html#diagnostics_channel_class_channel',

  'Domain': 'domain.html#domain_class_domain',

  'errors.Error': 'errors.html#errors_class_error',

  'import.meta': 'esm.html#esm_import_meta',

  'EventEmitter': 'events.html#events_class_eventemitter',
  'EventTarget': 'events.html#events_class_eventtarget',
  'Event': 'events.html#events_class_event',
  'EventListener': 'events.html#events_event_listener',

  'FileHandle': 'fs.html#fs_class_filehandle',
  'fs.Dir': 'fs.html#fs_class_fs_dir',
  'fs.Dirent': 'fs.html#fs_class_fs_dirent',
  'fs.FSWatcher': 'fs.html#fs_class_fs_fswatcher',
  'fs.ReadStream': 'fs.html#fs_class_fs_readstream',
  'fs.Stats': 'fs.html#fs_class_fs_stats',
  'fs.StatWatcher': 'fs.html#fs_class_fs_statwatcher',
  'fs.WriteStream': 'fs.html#fs_class_fs_writestream',

  'http.Agent': 'http.html#http_class_http_agent',
  'http.ClientRequest': 'http.html#http_class_http_clientrequest',
  'http.IncomingMessage': 'http.html#http_class_http_incomingmessage',
  'http.Server': 'http.html#http_class_http_server',
  'http.ServerResponse': 'http.html#http_class_http_serverresponse',

  'ClientHttp2Session': 'http2.html#http2_class_clienthttp2session',
  'ClientHttp2Stream': 'http2.html#http2_class_clienthttp2stream',
  'HTTP/2 Headers Object': 'http2.html#http2_headers_object',
  'HTTP/2 Settings Object': 'http2.html#http2_settings_object',
  'http2.Http2ServerRequest': 'http2.html#http2_class_http2_http2serverrequest',
  'http2.Http2ServerResponse':
    'http2.html#http2_class_http2_http2serverresponse',
  'Http2SecureServer': 'http2.html#http2_class_http2secureserver',
  'Http2Server': 'http2.html#http2_class_http2server',
  'Http2Session': 'http2.html#http2_class_http2session',
  'Http2Stream': 'http2.html#http2_class_http2stream',
  'ServerHttp2Stream': 'http2.html#http2_class_serverhttp2stream',

  'https.Server': 'https.html#https_class_https_server',

  'module': 'modules.html#modules_the_module_object',

  'module.SourceMap':
    'module.html#module_class_module_sourcemap',

  'require': 'modules.html#modules_require_id',

  'Handle': 'net.html#net_server_listen_handle_backlog_callback',
  'net.BlockList': 'net.html#net_class_net_blocklist',
  'net.Server': 'net.html#net_class_net_server',
  'net.Socket': 'net.html#net_class_net_socket',
  'net.SocketAddress': 'net.html#net_class_net_socketaddress',

  'NodeEventTarget':
    'events.html#events_class_nodeeventtarget',

  'os.constants.dlopen': 'os.html#os_dlopen_constants',

  'Histogram': 'perf_hooks.html#perf_hooks_class_histogram',
  'IntervalHistogram':
     'perf_hooks.html#perf_hooks_class_intervalhistogram_extends_histogram',
  'RecordableHistogram':
     'perf_hooks.html#perf_hooks_class_recordablehistogram_extends_histogram',
  'PerformanceEntry': 'perf_hooks.html#perf_hooks_class_performanceentry',
  'PerformanceNodeTiming':
    'perf_hooks.html#perf_hooks_class_performancenodetiming',
  'PerformanceObserver':
    'perf_hooks.html#perf_hooks_class_perf_hooks_performanceobserver',
  'PerformanceObserverEntryList':
    'perf_hooks.html#perf_hooks_class_performanceobserverentrylist',

  'readline.Interface': 'readline.html#readline_class_interface',

  'repl.REPLServer': 'repl.html#repl_class_replserver',

  'Stream': 'stream.html#stream_stream',
  'stream.Duplex': 'stream.html#stream_class_stream_duplex',
  'stream.Readable': 'stream.html#stream_class_stream_readable',
  'stream.Transform': 'stream.html#stream_class_stream_transform',
  'stream.Writable': 'stream.html#stream_class_stream_writable',

  'Immediate': 'timers.html#timers_class_immediate',
  'Timeout': 'timers.html#timers_class_timeout',
  'Timer': 'timers.html#timers_timers',

  'tls.SecureContext': 'tls.html#tls_tls_createsecurecontext_options',
  'tls.Server': 'tls.html#tls_class_tls_server',
  'tls.TLSSocket': 'tls.html#tls_class_tls_tlssocket',

  'Tracing': 'tracing.html#tracing_tracing_object',

  'URL': 'url.html#url_the_whatwg_url_api',
  'URLSearchParams': 'url.html#url_class_urlsearchparams',

  'vm.Module': 'vm.html#vm_class_vm_module',
  'vm.Script': 'vm.html#vm_class_vm_script',
  'vm.SourceTextModule': 'vm.html#vm_class_vm_sourcetextmodule',

  'MessagePort': 'worker_threads.html#worker_threads_class_messageport',
  'Worker': 'worker_threads.html#worker_threads_class_worker',

  'X509Certificate': 'crypto.html#crypto_class_x509certificate',

  'zlib options': 'zlib.html#zlib_class_options',

  'ReadableStream':
    'webstreams.md#webstreamsapi_class_readablestream',
  'ReadableStreamDefaultReader':
    'webstreams.md#webstreamsapi_class_readablestreamdefaultreader',
  'ReadableStreamBYOBReader':
    'webstreams.md#webstreamsapi_class_readablestreambyobreader',
  'ReadableStreamDefaultController':
    'webstreams.md#webstreamsapi_class_readablestreamdefaultcontroller',
  'ReadableByteStreamController':
    'webstreams.md#webstreamsapi_class_readablebytestreamcontroller',
  'ReadableStreamBYOBRequest':
    'webstreams.md#webstreamsapi_class_readablestreambyobrequest',
  'WritableStream':
    'webstreams.md#webstreamsapi_class_writablestream',
  'WritableStreamDefaultWriter':
    'webstreams.md#webstreamsapi_class_writablestreamdefaultwriter',
  'WritableStreamDefaultController':
    'webstreams.md#webstreamsapi_class_writablestreamdefaultcontroller',
  'TransformStream':
    'webstreams.md#webstreamsapi_class_transformstream',
  'TransformStreamDefaultController':
    'webstreams.md#webstreamsapi_class_transformstreamdefaultcontroller',
  'ByteLengthQueuingStrategy':
    'webstreams.md#webstreamsapi_class_bytelengthqueuingstrategy',
  'CountQueuingStrategy':
    'webstreams.md#webstreamsapi_class_countqueuingstrategy',
  'TextEncoderStream':
    'webstreams.md#webstreamsapi_class_textencoderstream',
  'TextDecoderStream':
    'webstreams.md#webstreamsapi_class_textdecoderstream',
};

const arrayPart = /(?:\[])+$/;

export function toLink(typeInput) {
  const typeLinks = [];
  typeInput = typeInput.replace('{', '').replace('}', '');
  const typeTexts = typeInput.split('|');

  typeTexts.forEach((typeText) => {
    typeText = typeText.trim();
    if (typeText) {
      let typeUrl;

      // To support type[], type[][] etc., we store the full string
      // and use the bracket-less version to lookup the type URL.
      const typeTextFull = typeText;
      typeText = typeText.replace(arrayPart, '');

      const primitive = jsPrimitives[typeText];

      if (primitive !== undefined) {
        typeUrl = `${jsDataStructuresUrl}#${primitive}_type`;
      } else if (jsGlobalTypes.includes(typeText)) {
        typeUrl = `${jsGlobalObjectsUrl}${typeText}`;
      } else {
        typeUrl = customTypesMap[typeText];
      }

      if (typeUrl) {
        typeLinks.push(
          `<a href="${typeUrl}" class="type">&lt;${typeTextFull}&gt;</a>`);
      } else {
        throw new Error(
          `Unrecognized type: '${typeTextFull}'.\n` +
          `Please, edit the type or update '${import.meta.url}'.`
        );
      }
    } else {
      throw new Error(`Empty type slot: ${typeInput}`);
    }
  });

  return typeLinks.length ? typeLinks.join(' | ') : typeInput;
}
