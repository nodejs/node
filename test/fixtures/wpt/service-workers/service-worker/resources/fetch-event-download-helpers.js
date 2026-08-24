// Shared helpers for fetch-event-download-*.https.html tests.
// crbug.com/40410035.

function uniqueChannelId() {
  return 'download-' + Math.random().toString(36).slice(2);
}

// Resolves with the first message posted to a BroadcastChannel of the given
// id, or rejects via the test's timeout if none arrives.
function nextChannelMessage(t, channelId) {
  return new Promise((resolve) => {
    const channel = new BroadcastChannel(channelId);
    channel.addEventListener('message', (e) => {
      channel.close();
      resolve(e.data);
    }, {once: true});
    t.add_cleanup(() => channel.close());
  });
}

// Resolves true if a message arrived within `timeoutMs`, false otherwise.
function awaitChannelSilence(t, channelId, timeoutMs) {
  return new Promise((resolve) => {
    const channel = new BroadcastChannel(channelId);
    let received = false;
    channel.addEventListener('message', () => {
      received = true;
      channel.close();
      resolve(true);
    });
    step_timeout(() => {
      channel.close();
      resolve(received);
    }, timeoutMs);
    t.add_cleanup(() => channel.close());
  });
}

// Programmatically clicks an <a download> via testdriver so the click carries
// transient activation; without it Chromium's download path may suppress the
// fetch entirely.
async function clickDownloadAnchor(t, href) {
  const a = document.createElement('a');
  a.href = href;
  a.download = '';
  a.textContent = 'download';
  document.body.appendChild(a);
  t.add_cleanup(() => a.remove());
  await test_driver.click(a);
}
