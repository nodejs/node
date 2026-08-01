// META: global=serviceworker

test((t) => {
  assert_false('targetClientId' in FetchEvent.prototype)
}, 'targetClientId should not be on FetchEvent');
