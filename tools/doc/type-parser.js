'use strict';

const jsDocPrefix = 'https://developer.mozilla.org/en-US/docs/Web/JavaScript/';

const jsPrimitiveUrl = `${jsDocPrefix}Data_structures`;
const jsPrimitives = {
  'boolean': 'Boolean',
  'integer': 'Number', // not a primitive, used for clarification
  'null': 'Null',
  'number': 'Number',
  'string': 'String',
  'symbol': 'Symbol',
  'undefined': 'Undefined'
};

const jsGlobalObjectsUrl = `${jsDocPrefix}Reference/Global_Objects/`;
const jsGlobalTypes = [
  'Array', 'ArrayBuffer', 'AsyncFunction', 'DataView', 'Date', 'Error',
  'EvalError', 'Float32Array', 'Float64Array', 'Function', 'Generator',
  'GeneratorFunction', 'Int16Array', 'Int32Array', 'Int8Array', 'Map', 'Object',
  'Promise', 'Proxy', 'RangeError', 'ReferenceError', 'RegExp', 'Set',
  'SharedArrayBuffer', 'SyntaxError', 'TypeError', 'TypedArray', 'URIError',
  'Uint16Array', 'Uint32Array', 'Uint8Array', 'Uint8ClampedArray', 'WeakMap',
  'WeakSet'
];

const customTypesMap = {
  'Iterable':
    `${jsDocPrefix}Reference/Iteration_protocols#The_iterable_protocol`,
  'Iterator':
    `${jsDocPrefix}Reference/Iteration_protocols#The_iterator_protocol`,

  'this': `${jsDocPrefix}Reference/Operators/this`,

  'AsyncHook': 'async_hooks.html#async_hooks_async_hooks_createhook_callbacks',

  'Buffer': 'buffer.html#buffer_class_buffer',

  'ChildProcess': 'child_process.html#child_process_class_childprocess',

  'cluster.Worker': 'cluster.html#cluster_class_worker',

  'crypto.constants': 'crypto.html#crypto_crypto_constants_1',

  'dgram.Socket': 'dgram.html#dgram_class_dgram_socket',

  'Domain': 'domain.html#domain_class_domain',

  'EventEmitter': 'events.html#events_class_eventemitter',

  'fs.Stats': 'fs.html#fs_class_fs_stats',

  'http.Agent': 'http.html#http_class_http_agent',
  'http.ClientRequest': 'http.html#http_class_http_clientrequest',
  'http.IncomingMessage': 'http.html#http_class_http_incomingmessage',
  'http.Server': 'http.html#http_class_http_server',
  'http.ServerResponse': 'http.html#http_class_http_serverresponse',

  'ClientHttp2Stream': 'http2.html#http2_class_clienthttp2stream',
  'HTTP/2 Headers Object': 'http2.html#http2_headers_object',
  'HTTP/2 Settings Object': 'http2.html#http2_settings_object',
  'http2.Http2ServerRequest': 'http2.html#http2_class_http2_http2serverrequest',
  'http2.Http2ServerResponse':
    'http2.html#http2_class_http2_http2serverresponse',
  'Http2Server': 'http2.html#http2_class_http2server',
  'Http2Session': 'http2.html#http2_class_http2session',
  'Http2Stream': 'http2.html#http2_class_http2stream',
  'ServerHttp2Stream': 'http2.html#http2_class_serverhttp2stream',

  'Handle': 'net.html#net_server_listen_handle_backlog_callback',
  'net.Server': 'net.html#net_class_net_server',
  'net.Socket': 'net.html#net_class_net_socket',

  'os.constants.dlopen': 'os.html#os_dlopen_constants',

  'PerformanceObserver':
    'perf_hooks.html#perf_hooks_class_performanceobserver_callback',
  'PerformanceObserverEntryList':
    'perf_hooks.html#perf_hooks_class_performanceobserverentrylist',

  'readline.Interface': 'readline.html#readline_class_interface',

  'Stream': 'stream.html#stream_stream',
  'stream.Duplex': 'stream.html#stream_class_stream_duplex',
  'stream.Readable': 'stream.html#stream_class_stream_readable',
  'stream.Writable': 'stream.html#stream_class_stream_writable',

  'Immediate': 'timers.html#timers_class_immediate',
  'Timeout': 'timers.html#timers_class_timeout',
  'Timer': 'timers.html#timers_timers',

  'tls.Server': 'tls.html#tls_class_tls_server',
  'tls.TLSSocket': 'tls.html#tls_class_tls_tlssocket',

  'URL': 'url.html#url_the_whatwg_url_api',
  'URLSearchParams': 'url.html#url_class_urlsearchparams'
};

const arrayPart = /(?:\[])+$/;

function toLink(typeInput) {
  const typeLinks = [];
  typeInput = typeInput.replace('{', '').replace('}', '');
  const typeTexts = typeInput.split('|');

  typeTexts.forEach((typeText) => {
    typeText = typeText.trim();
    if (typeText) {
      let typeUrl = null;

      // To support type[], type[][] etc., we store the full string
      // and use the bracket-less version to lookup the type URL
      const typeTextFull = typeText;
      typeText = typeText.replace(arrayPart, '');

      const primitive = jsPrimitives[typeText.toLowerCase()];

      if (primitive !== undefined) {
        typeUrl = `${jsPrimitiveUrl}#${primitive}_type`;
      } else if (jsGlobalTypes.includes(typeText)) {
        typeUrl = `${jsGlobalObjectsUrl}${typeText}`;
      } else if (customTypesMap[typeText]) {
        typeUrl = customTypesMap[typeText];
      }

      if (typeUrl) {
        typeLinks.push(
          `<a href="${typeUrl}" class="type">&lt;${typeTextFull}&gt;</a>`);
      } else {
        typeLinks.push(`<span class="type">&lt;${typeTextFull}&gt;</span>`);
      }
    } else {
      throw new Error(`Empty type slot: ${typeInput}`);
    }
  });

  return typeLinks.length ? typeLinks.join(' | ') : typeInput;
}

module.exports = { toLink };
