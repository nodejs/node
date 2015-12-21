'use strict';
const nodeDocUrl = '';
const jsDocUrl = 'https://developer.mozilla.org/en-US/docs/Web/JavaScript/' +
  'Reference/Global_Objects/';
const jsGlobalTypes = [
  'Error', 'Number', 'Object', 'Boolean', 'String', 'Function', 'Symbol',
  'Array', 'Uint8Array', 'Uint16Array', 'Uint32Array', 'Int8Array',
  'Int16Array', 'Int32Array', 'Uint8ClampedArray', 'Float32Array',
  'Float64Array', 'Date', 'RegExp', 'ArrayBuffer', 'DataView', 'Promise',
  'Null'
];
const typeMap = {
  'Buffer': 'buffer.html#buffer_class_buffer',
  'Handle': 'net.html#net_server_listen_handle_callback',
  'Stream': 'stream.html#stream_stream',
  'stream.Writable': 'stream.html#stream_class_stream_writable',
  'stream.Readable': 'stream.html#stream_class_stream_readable',
  'ChildProcess': 'child_process.html#child_process_class_childprocess',
  'Worker': 'cluster.html#cluster_class_worker',
  'dgram.Socket': 'dgram.html#dgram_class_dgram_socket',
  'net.Socket': 'net.html#net_class_net_socket',
  'EventEmitter': 'events.html#events_class_events_eventemitter',
  'Timer': 'timers.html#timers_timers'
};

module.exports = {
  toLink: function (typeInput) {
    let typeLinks = [];
    typeInput = typeInput.replace('{', '').replace('}', '');
    let typeTexts = typeInput.split('|');

    typeTexts.forEach(function (typeText) {
      typeText = typeText.trim();
      if (typeText) {
        let typeUrl = null;
        if (jsGlobalTypes.indexOf(typeText) !== -1) {
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
}
