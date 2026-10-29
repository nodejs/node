// META: title=Web Locks API: API not available in non-secure context
// META: global=window,dedicatedworker,sharedworker

'use strict';

test(t => {
  assert_false(self.isSecureContext);
  assert_false('locks' in navigator,
               'navigator.locks is only present in secure contexts');
  assert_false('LockManager' in self,
               'LockManager is only present in secure contexts');
  assert_false('Lock' in self,
               'Lock interface is only present in secure contexts');
}, 'API presence in non-secure contexts');
