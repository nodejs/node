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

var insertMultipart = require('../../helpers/unit').unit([
  'multipart',
  'insert'
]);

insertMultipart({hey: 1}, [{
    name: 'att',
    data: 'some',
    'content_type': 'text/plain'
  }], {extra: 'stuff'}, {
  headers: {
    'content-type': 'multipart/related'
  },
  method: 'PUT',
  multipart: [
    {
      body:
      '{"_attachments":' +
        '{"att":{"follows":true,"length":4,"content_type":"text/plain"}}' +
        ',"hey":1}',
      'content-type': 'application/json'
    },
    {body: 'some'}
  ],
  qs: {extra: 'stuff'},
  uri: '/mock'
});
