// Licensed under the Apache License, Version 2.0 (the 'License'); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

'use strict';

var helpers = require('../../helpers/unit');
var insertAttachment = helpers.unit(['attachment', 'insert']);

var buffer = new Buffer(helpers.pixel, 'base64');

insertAttachment('pixels', 'pixel.bmp', buffer, 'image/bmp', {
  body: buffer,
  headers: {
    'content-type': 'image/bmp'
  },
  method: 'PUT',
  uri: '/mock/pixels/pixel.bmp'
});

insertAttachment('pixels', 'meta.txt', 'brown', 'text/plain', {
  body: 'brown',
  headers: {
    'content-type': 'text/plain'
  },
  method: 'PUT',
  uri: '/mock/pixels/meta.txt'
});

insertAttachment('pixels', 'meta.txt', 'white', 'text/plain', {rev: '2'}, {
  body: 'white',
  headers: {
    'content-type': 'text/plain'
  },
  method: 'PUT',
  uri: '/mock/pixels/meta.txt',
  qs: {rev: '2'}
});
