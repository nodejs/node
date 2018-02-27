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
  'Error', 'Object', 'Function', 'Array', 'TypedArray', 'Uint8Array',
  'Uint16Array', 'Uint32Array', 'Int8Array', 'Int16Array', 'Int32Array',
  'Uint8ClampedArray', 'Float32Array', 'Float64Array', 'Date', 'RegExp',
  'ArrayBuffer', 'DataView', 'Promise', 'EvalError', 'RangeError',
  'ReferenceError', 'SyntaxError', 'TypeError', 'URIError'
];
const typeMap = {
  'Buffer': 'buffer.html#buffer_class_buffer',
  'Handle': 'net.html#net_server_listen_handle_backlog_callback',
  'Stream': 'stream.html#stream_stream',
  'stream.Writable': 'stream.html#stream_class_stream_writable',
  'stream.Readable': 'stream.html#stream_class_stream_readable',
  'stream.Duplex': 'stream.html#stream_class_stream_duplex',
  'ChildProcess': 'child_process.html#child_process_class_childprocess',
  'cluster.Worker': 'cluster.html#cluster_class_worker',
  'dgram.Socket': 'dgram.html#dgram_class_dgram_socket',
  'net.Socket': 'net.html#net_class_net_socket',
  'tls.TLSSocket': 'tls.html#tls_class_tls_tlssocket',
  'EventEmitter': 'events.html#events_class_eventemitter',
  'Timer': 'timers.html#timers_timers',
  'http.Agent': 'http.html#http_class_http_agent',
  'http.ClientRequest': 'http.html#http_class_http_clientrequest',
  'http.IncomingMessage': 'http.html#http_class_http_incomingmessage',
  'http.Server': 'http.html#http_class_http_server',
  'http.ServerResponse': 'http.html#http_class_http_serverresponse',
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
