'use strict';
// Addons: test_pending_exception, test_pending_exception_vtable
// Addons: test_cannot_run_js, test_cannot_run_js_vtable

// Test that `napi_call_function()` returns `napi_cannot_run_js` in experimental
// mode and `napi_pending_exception` otherwise. This test calls the add-on's
// `createRef()` method, which creates a strong reference to a JS function. When
// the process exits, it calls all reference finalizers. The finalizer for the
// strong reference created herein will attempt to call `napi_get_property()` on
// a property of the global object and will abort the process if the API doesn't
// return the correct status.

const { mustNotCall } = require('../../common');
const { addonPath } = require('../../common/addon-test');
const addon = require(addonPath);

addon.createRef(mustNotCall());
