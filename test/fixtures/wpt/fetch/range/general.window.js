// META: script=resources/utils.js
// META: script=/common/utils.js

const onload = new Promise(r => window.addEventListener('load', r));

// It's weird that browsers do this, but it should continue to work.
promise_test(async t => {
  await loadScript('resources/partial-script.py?pretend-offset=90000');
  assert_true(self.scriptExecuted);
}, `Script executed from partial response`);

promise_test(async () => {
  const wavURL = new URL('resources/long-wav.py', location);
  const stashTakeURL = new URL('resources/stash-take.py', location);
  const stashToken = token();
  wavURL.searchParams.set('accept-encoding-key', stashToken);
  stashTakeURL.searchParams.set('key', stashToken);

  // The testing framework waits for window onload. If the audio element
  // is appended before onload, it extends it, and the test times out.
  await onload;

  const audio = appendAudio(document, wavURL);
  await new Promise(r => audio.addEventListener('progress', r));
  audio.remove();

  const response = await fetch(stashTakeURL);
  assert_equals(await response.json(), 'identity', `Expect identity accept-encoding on media request`);
}, `Fetch with range header will be sent with Accept-Encoding: identity`);
