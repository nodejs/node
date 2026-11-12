'use strict';

// Experimental implementation of the Fetch Server API.
// https://fetch-server.proposal.wintertc.org/

const {
  ObjectFreeze,
} = primordials;

const { ServerResponse } = require('internal/http/fetch_server/response');
const { serve } = require('internal/http/fetch_server/server');

module.exports = ObjectFreeze({
  __proto__: null,
  ServerResponse,
  serve,
});
