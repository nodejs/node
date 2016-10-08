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

var changesDatabase = require('../../helpers/unit').unit([
  'database',
  'changes'
]);

changesDatabase('mock', {since: '10'}, {
  headers: {
    accept: 'application/json',
    'content-type': 'application/json'
  },
  method: 'GET',
  qs: {since: '10'},
  uri: '/mock/_changes'
});

changesDatabase('mock', {
  headers: {
    accept: 'application/json',
    'content-type': 'application/json'
  },
  method: 'GET',
  uri: '/mock/_changes'
});
