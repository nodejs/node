import { strictEqual, deepEqual } from 'assert';

import m, { π } from './exports-cases.js';
import * as ns from './exports-cases.js';

deepEqual(Object.keys(ns), ['?invalid', 'default', 'invalid identifier', 'isObject', 'package', 'z', 'π', '\u{d83c}\u{df10}']);
strictEqual(π, 'yes');
strictEqual(typeof m.isObject, 'undefined');
strictEqual(m.π, 'yes');
strictEqual(m.z, 'yes');
strictEqual(m.package, 10);
strictEqual(m['invalid identifier'], 'yes');
strictEqual(m['?invalid'], 'yes');

import m2, { __esModule as __esModule2, name as name2 } from './exports-cases2.js';
import * as ns2 from './exports-cases2.js';

strictEqual(__esModule2, true);
strictEqual(name2, 'name');
strictEqual(typeof m2, 'object');
strictEqual(m2.default, 'the default');
strictEqual(ns2.__esModule, true);
strictEqual(ns2.name, 'name');
deepEqual(Object.keys(ns2), ['__esModule', 'case2', 'default', 'name', 'pi']);

import m3, { __esModule as __esModule3, name as name3 } from './exports-cases3.js';
import * as ns3 from './exports-cases3.js';

strictEqual(__esModule3, true);
strictEqual(name3, 'name');
deepEqual(Object.keys(m3), ['name', 'default', 'pi', 'case2']);
strictEqual(ns3.__esModule, true);
strictEqual(ns3.name, 'name');
strictEqual(ns3.case2, 'case2');

console.log('ok');
