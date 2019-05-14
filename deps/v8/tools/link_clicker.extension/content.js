// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function linkClickerContentScript() {
  // time in ms
  let minInterval;
  let maxInterval;
  let pattern;
  let enabled;
  let timeoutId;

  // Initialize variables.
  chrome.runtime.sendMessage({type:'get'}, function(msg) {
    if (msg.type == 'update') updateFromMessage(msg);
  });

  chrome.runtime.onMessage.addListener(
    function(msg, sender, sendResponse) {
      if (msg.type == 'update') updateFromMessage(msg);
    });

  function findAllLinks() {
    let links = document.links;
    let results = new Set();
    for (let i = 0; i < links.length; i++) {
      let href = links[i].href;
      if (!href) continue;
      if (href && href.match(pattern)) results.add(href);
    }
    return Array.from(results);
  }

  function updateFromMessage(msg) {
    console.log(msg);
    minInterval = Number(msg.minInterval)
    maxInterval = Number(msg.maxInterval);
    pattern = new RegExp(msg.pattern);
    enabled = Boolean(msg.enabled);
    if (enabled) schedule();
  }

  function followLink() {
    if (!enabled) return;
    let links = findAllLinks();
    if (links.length <= 5) {
      // navigate back if the page has not enough links
      window.history.back()
      console.log("navigate back");
    } else {
      let link = links[Math.round(Math.random() * (links.length-1))];
      console.log(link);
      window.location.href = link;
      // Schedule in case we just followed an anchor.
      schedule();
    }
  }

  function schedule() {
    clearTimeout(timeoutId);
    let delta = maxInterval - minInterval;
    let duration = minInterval + (Math.random() * delta);
    console.log(duration);
    timeoutId = setTimeout(followLink, duration);
  }
})();
