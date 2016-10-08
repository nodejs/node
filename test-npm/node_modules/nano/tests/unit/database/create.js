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

var createDatabase = require('../../helpers/unit').unit([
  'database',
  'create'
]);

createDatabase('mock', {
  headers: {
    accept: 'application/json',
    'content-type': 'application/json'
  },
  method: 'PUT',
  uri: '/mock'
});

createDatabase('az09_$()+-/', {
  headers: {
    accept: 'application/json',
    'content-type': 'application/json'
  },
  method: 'PUT',
  uri: '/az09_%24()%2B-%2F'
});
