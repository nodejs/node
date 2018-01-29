'use strict';
const nodeDocUrl = '';
const jsDocUrl = 'https://developer.mozilla.org/en-US/docs/Web/JavaScript/' +
                 'Reference/Global_Objects/';
const jsPrimitiveUrl = 'https://developer.mozilla.org/en-US/docs/Web/' +
                       'JavaScript/Data_structures';
const jsPrimitives = {
  'boolean': 'Boolean',
  'integer': 'Number', // not a primitive, used for clarification
  'null': 'Null',
  'number': 'Number',
  'string': 'String',
  'symbol': 'Symbol',
  'undefined': 'Undefined'
};
const jsGlobalTypes = [
  'Array', 'ArrayBuffer', 'DataView', 'Date', 'Error', 'EvalError',
  'Float32Array', 'Float64Array', 'Function', 'Int16Array', 'Int32Array',
  'Int8Array', 'Object', 'Promise', 'RangeError', 'ReferenceError', 'RegExp',
  'SharedArrayBuffer', 'SyntaxError', 'TypeError', 'TypedArray', 'URIError',
  'Uint16Array', 'Uint32Array', 'Uint8Array', 'Uint8ClampedArray'
];
const typeMap = {
  'Iterable':
    'https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Iteration_protocols#The_iterable_protocol',
  'Iterator':
    'https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Iteration_protocols#The_iterator_protocol',

  'this':
    'https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/this',

  'Buffer': 'buffer.html#buffer_class_buffer',

  'ChildProcess': 'child_process.html#child_process_class_childprocess',

  'cluster.Worker': 'cluster.html#cluster_class_worker',

  'dgram.Socket': 'dgram.html#dgram_class_dgram_socket',

  'Domain': 'domain.html#domain_class_domain',

  'EventEmitter': 'events.html#events_class_eventemitter',

  'fs.Stats': 'fs.html#fs_class_fs_stats',

  'http.Agent': 'http.html#http_class_http_agent',
  'http.ClientRequest': 'http.html#http_class_http_clientrequest',
  'http.IncomingMessage': 'http.html#http_class_http_incomingmessage',
  'http.Server': 'http.html#http_class_http_server',
  'http.ServerResponse': 'http.html#http_class_http_serverresponse',

  'Handle': 'net.html#net_server_listen_handle_backlog_callback',
  'net.Server': 'net.html#net_class_net_server',
  'net.Socket': 'net.html#net_class_net_socket',

  'readline.Interface': 'readline.html#readline_class_interface',

  'Stream': 'stream.html#stream_stream',
  'stream.Duplex': 'stream.html#stream_class_stream_duplex',
  'stream.Readable': 'stream.html#stream_class_stream_readable',
  'stream.Writable': 'stream.html#stream_class_stream_writable',

  'Immediate': 'timers.html#timers_class_immediate',
  'Timeout': 'timers.html#timers_class_timeout',
  'Timer': 'timers.html#timers_timers',

  'tls.TLSSocket': 'tls.html#tls_class_tls_tlssocket',

  'URL': 'url.html#url_the_whatwg_url_api',
  'URLSearchParams': 'url.html#url_class_urlsearchparams'
};

const arrayPart = /(?:\[])+$/;

module.exports = {
  toLink: function(typeInput) {
    const typeLinks = [];
    typeInput = typeInput.replace('{', '').replace('}', '');
    const typeTexts = typeInput.split('|');

    typeTexts.forEach(function(typeText) {
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
        } else if (jsGlobalTypes.indexOf(typeText) !== -1) {
          typeUrl = jsDocUrl + typeText;
        } else if (typeMap[typeText]) {
          typeUrl = nodeDocUrl + typeMap[typeText];
        }

        if (typeUrl) {
          typeLinks.push('<a href="' + typeUrl + '" class="type">&lt;' +
            typeTextFull + '&gt;</a>');
        } else {
          typeLinks.push('<span class="type">&lt;' + typeTextFull +
                         '&gt;</span>');
        }
      }
    });

    return typeLinks.length ? typeLinks.join(' | ') : typeInput;
  }
};
