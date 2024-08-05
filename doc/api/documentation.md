# About this documentation

<!--introduced_in=v0.10.0-->

<!-- type=misc -->

Welcome to the official API reference documentation for Node.js!

Node.js is a JavaScript runtime built on the [V8 JavaScript engine][].

## Contributing

Report errors in this documentation in [the issue tracker][]. See
[the contributing guide][] for directions on how to submit pull requests.

## Stability index

<!--type=misc-->

Throughout the documentation are indications of a section's stability. Some APIs
are so proven and so relied upon that they are unlikely to ever change at all.
Others are brand new and experimental, or known to be hazardous.

The stability indexes are as follows:

<div class="api_stability api_stability_0">
  Stability: 0 - Deprecated.
  The feature may emit warnings. Backward compatibility is not guaranteed.
</div>

<div class="api_stability api_stability_1">
  Stability: 1 - Experimental. The feature is not subject to
  <a href="https://semver.org/">semantic versioning</a> rules.
  Non-backward compatible changes or removal may occur in any future release.
  Use of the feature is not recommended in production environments.
  <p>Experimental features are subdivided into stages:</p>
  <ul>
    <li>
      1.0 - Early development. Experimental features at this stage are unfinished
      and subject to substantial change.
    </li>
    <li>
      1.1 - Active development. Experimental features at this stage are nearing
      minimum viability.
    </li>
    <li>
      1.2 - Release candidate. Experimental features at this stage are hopefully
      ready to become stable. No further breaking changes are anticipated but may
      still occur in response to user feedback. We encourage user testing and
      feedback so that we can know that this feature is ready to be marked as
      stable.
    </li>
  </ul>
</div>

<div class="api_stability api_stability_2">
  Stability: 2 - Stable. Compatibility with the npm ecosystem is a high
  priority.
</div>

<div class="api_stability api_stability_3">
  Stability: 3 - Legacy. Although this feature is unlikely to be removed and is
  still covered by semantic versioning guarantees, it is no longer actively
  maintained, and other alternatives are available.
</div>

Features are marked as legacy rather than being deprecated if their use does no
harm, and they are widely relied upon within the npm ecosystem. Bugs found in
legacy features are unlikely to be fixed.

Use caution when making use of Experimental features, particularly when
authoring libraries. Users may not be aware that experimental features are being
used. Bugs or behavior changes may surprise users when Experimental API
modifications occur. To avoid surprises, use of an Experimental feature may need
a command-line flag. Experimental features may also emit a [warning][].

## Stability overview

<!-- STABILITY_OVERVIEW_SLOT_BEGIN -->

<!-- STABILITY_OVERVIEW_SLOT_END -->

## JSON output

<!-- YAML
added: v0.6.12
-->

Every `.html` document has a corresponding `.json` document. This is for IDEs
and other utilities that consume the documentation.

## System calls and man pages

Node.js functions which wrap a system call will document that. The docs link
to the corresponding man pages which describe how the system call works.

Most Unix system calls have Windows analogues. Still, behavior differences may
be unavoidable.

[V8 JavaScript engine]: https://v8.dev/
[semantic versioning]: https://semver.org/
[the contributing guide]: https://github.com/nodejs/node/blob/HEAD/CONTRIBUTING.md
[the issue tracker]: https://github.com/nodejs/node/issues/new
[warning]: process.md#event-warning
