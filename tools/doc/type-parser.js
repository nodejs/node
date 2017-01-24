'use strict';
const nodeDocUrl = '';
const jsDocPrefix = 'https://developer.mozilla.org/en-US/docs/Web/JavaScript/';
const jsDocUrl = jsDocPrefix + 'Reference/Global_Objects/';
const jsPrimitiveUrl = jsDocPrefix + 'Data_structures';
const jsPrimitives = {
  'Integer': 'Number',  // this is for extending
  'Number': 'Number',
  'String': 'String',
  'Boolean': 'Boolean',
  'Null': 'Null',
  'Symbol': 'Symbol'
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
  'Iterable': jsDocPrefix +
              'Reference/Iteration_protocols#The_iterable_protocol',
  'Iterator': jsDocPrefix +
              'Reference/Iteration_protocols#The_iterator_protocol',
  'URL': 'url.html#url_the_whatwg_url_api'
};

module.exports = {
  toLink: function(typeInput) {
    const typeLinks = [];
    typeInput = typeInput.replace('{', '').replace('}', '');
    const typeTexts = typeInput.split('|');

    typeTexts.forEach(function(typeText) {
      typeText = typeText.trim();
      if (typeText) {
        let typeUrl = null;
        const primitive = jsPrimitives[typeText];
        if (primitive !== undefined) {
          typeUrl = `${jsPrimitiveUrl}#${primitive}_type`;
        } else if (jsGlobalTypes.indexOf(typeText) !== -1) {
          typeUrl = jsDocUrl + typeText;
        } else if (typeMap[typeText]) {
          typeUrl = nodeDocUrl + typeMap[typeText];
        }

        if (typeUrl) {
          typeLinks.push('<a href="' + typeUrl + '" class="type">&lt;' +
            typeText + '&gt;</a>');
        } else {
          typeLinks.push('<span class="type">&lt;' + typeText + '&gt;</span>');
        }
      }
    });

    return typeLinks.length ? typeLinks.join(' | ') : typeInput;
  }
};
