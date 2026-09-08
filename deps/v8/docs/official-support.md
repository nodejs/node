---
title: 'Officially supported configurations'
description: 'This document explains which build configurations are maintained by the V8 team.'
---
V8 supports a multitude of different build configurations across operating systems, their versions, architecture ports, build flags and so on.

The rule of thumb: If we support it, we have a bot running on one of our [continuous integration consoles](https://ci.chromium.org/p/v8/g/main/console).

Some nuances:

- Breakages on the most important builders will block code submission. A tree sheriff will usually revert the culprit.
- Breakages on roughly the same [set of builders](https://chromium.googlesource.com/infra/infra/+/main/infra/services/lkgr_finder/config/v8_cfg.pyl) block our continuous roll into Chromium.
- Some architecture ports are [handled externally](/docs/ports).
- Some configurations are [experimental](https://ci.chromium.org/p/v8/g/experiments/console). Breakages are permitted and will be handled by the owners of the configuration.

If you have a configuration that exhibits a problem, but is not covered by one of the bots above:

- Feel free to submit a CL that fixes your problem. The team will support you with a code review.
- You can use v8-dev@googlegroups.com to discuss the problem.
- If you think we should support this configuration (maybe a hole in our test matrix?), please file a bug on the [V8 issue tracker](https://bugs.chromium.org/p/v8/issues/entry) and ask.

However, we don’t have the bandwidth to support every possible configuration.
