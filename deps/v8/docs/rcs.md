---
title: 'Runtime Call Stats'
description: 'This document explains how to use Runtime Call Stats to get detailed V8-internal metrics.'
---
[The DevTools Performance panel](https://developers.google.com/web/tools/chrome-devtools/evaluate-performance/) gives insights into your web app’s runtime performance by visualizing various Chrome-internal metrics. However, certain low-level V8 metrics aren’t currently exposed in DevTools. This article guides you through the most robust way of gathering detailed V8-internal metrics, known as Runtime Call Stats or RCS, through `chrome://tracing`.

Tracing records the behavior of the entire browser, including other tabs, windows, and extensions, so it works best when done in a clean user profile, with extensions disabled, and with no other browser tabs open:

```bash
# Start a new Chrome browser session with a clean user profile and extensions disabled
google-chrome --user-data-dir="$(mktemp -d)" --disable-extensions
```

Type the URL of the page you want to measure in the first tab, but do not load the page yet.

![](/_img/rcs/01.png)

Add a second tab and open `chrome://tracing`. Tip: you can just enter `chrome:tracing`, without the slashes.

![](/_img/rcs/02.png)

Click on the “Record” button to prepare recording a trace. First choose “Web developer” and then select “Edit categories”.

![](/_img/rcs/03.png)

Select `v8.runtime_stats` from the list. Depending on how detailed your investigation is, you may select other categories as well.

![](/_img/rcs/04.png)

Press “Record” and switch back to the first tab and load the page. The fastest way is to use <kbd>Ctrl</kbd>/<kbd>⌘</kbd>+<kbd>1</kbd> to directly jump to the first tab and then press <kbd>Enter</kbd> to accept the entered URL.

![](/_img/rcs/05.png)

Wait until your page has completed loading or the buffer is full, then “Stop” the recording.

![](/_img/rcs/06.png)

Look for a “Renderer” section that contains the web page title from the recorded tab. The easiest way to do this is by clicking “Processes”, then clicking “None” to uncheck all entries, and then selecting only the renderer you’re interested in.

![](/_img/rcs/07.png)

Select the trace events/slices by pressing <kbd>Shift</kbd> and dragging. Make sure you cover _all_ the sections, including `CrRendererMain` and any `ThreadPoolForegroundWorker`s. A table with all the selected slices appears at the bottom.

![](/_img/rcs/08.png)

Scroll to the top right of the table and click on the link next to “Runtime call stats table”.

![](/_img/rcs/09.png)

In the view that appears, scroll to the bottom to see a detailed table of where V8 spends its time.

![](/_img/rcs/10.png)

By flipping open a category you can further drill down into the data.

![](/_img/rcs/11.png)

## Command-line interface { #cli }

Run [`d8`](/docs/d8) with `--runtime-call-stats` to get RCS metrics from the command-line:

```bash
d8 --runtime-call-stats foo.js
```
