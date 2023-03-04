'use strict';

const common = require('../common');
common.skipIfInspectorDisabled();

const { open, url } = require("inspector");
const { Session } = require('inspector');
const assert = require('assert');
const session = new Session();
session.connect();

open();
assert.ok(url() !== undefined);
