# Components in core

This document outlines the considerations for deciding whether a
component should be included in the Node.js core.

A component may be included in the core as a dependency, a module,
or integrated into the codebase. The same arguments for inclusion
or exclusion generally apply in all these cases.

## Strong arguments for including a component in core

1. **Standardized functionality:** The component provides functionality
   that is standardized (such as a [Web API][]) and overlaps with existing
   functionality.

2. **Core-only implementation:** The component can only be implemented in
   the core.

3. **Performance:** The component can only be implemented in a performant way
   in the core.

4. **Improved developer experience:** Developer experience is significantly
   improved if the component is in the core.

5. **Common use cases:** The component provides functionality that can be expected
   to solve at least one common use case Node.js users face.

6. **Native bindings:** The component requires native bindings. Inclusion in the
   core enables utility across operating systems and architectures without
   requiring users to have a native compilation toolchain.

7. **Reusability:** Part or all of the component will also be reused or
   duplicated in the core.

## Strong arguments against including a component in core

1. **Lack of justification:** None of the arguments listed in the previous
   section apply.

2. **Licensing issues:** The component has a license that prohibits Node.js
   from including it in the core without also changing its own license.

3. **Redundancy:** There is already similar functionality in the core, and
   adding the component will provide a second API to do the same thing.

4. **Deprecation:** The component (or the standard it is based on) is deprecated,
   and there is a non-deprecated alternative.

5. **Rapid evolution:** The component is evolving quickly, and inclusion in the core
   will require frequent API changes.

## Benefits and challenges

When it is unclear whether a component should be included in the core, consider these
additional factors.

### Benefits

1. **Frequent testing:** The component will receive more frequent testing with Node.js CI
   and CITGM.

2. **LTS integration:** The component will be integrated into the Long Term Support (LTS)
   workflow.

3. **Unified documentation:** Documentation will be integrated with the core.

4. **No npm dependency:** There is no dependency on npm.

### Challenges

1. **Reduced code merging velocity:** Inclusion in the core is likely to reduce code
   merging velocity. The Node.js process for code review and merging is more
   time-consuming than that of most separate modules.

2. **Slower patch publication:** Being bound to the Node.js release cycle makes it harder
   and slower to publish patches.

3. **Reduced user flexibility:** Users cannot update the component independently without
   also updating Node.js.

[Web API]: https://developer.mozilla.org/en-US/docs/Web/API
