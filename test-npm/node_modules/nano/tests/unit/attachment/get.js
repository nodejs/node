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

var getAttachment = require('../../helpers/unit').unit([
  'attachment',
  'get'
]);

getAttachment('airplane-design', 'wings.pdf', {rev: 'rev-3'}, {
  encoding: null,
  headers: {},
  method: 'GET',
  qs: {rev: 'rev-3'},
  uri: '/mock/airplane-design/wings.pdf'
});

getAttachment('airplane-design', 'wings.pdf', {
  encoding: null,
  headers: {},
  method: 'GET',
  uri: '/mock/airplane-design/wings.pdf'
});
