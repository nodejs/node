// META: title=Web Locks API: API requires secure context
// META: global=window,dedicatedworker,sharedworker,serviceworker

'use strict';

test(t => {
  assert_true(self.isSecureContext);
  assert_idl_attribute(navigator, 'locks',
                       'navigator.locks exists in secure context');
  assert_true('LockManager' in self,
              'LockManager is present in secure contexts');
  assert_true('Lock' in self,
              'Lock interface is present in secure contexts');
}, 'API presence in secure contexts');
