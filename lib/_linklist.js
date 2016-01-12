'use strict';

const I18N = require('internal/messages');
const msg = require('internal/util').printDeprecationMessage;

module.exports = require('internal/linkedlist');
msg(I18N(I18N.LINKLIST_DEPRECATED));
