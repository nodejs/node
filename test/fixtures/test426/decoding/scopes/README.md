## Test cases

- **empty-scopes-field**: Empty original scopes field
- **nil-scopes**: Multiple null original scopes
- **close-start-end-position-scopes**: Scopes with very close start and end
  positions
- **single-root-original-scope**: A single global root original scope
- **nested-scopes**: Nested original scopes representing functions and blocks
- **sibling-scopes**: Multiple sibling top-level functions
- **scope-variables**: Scopes containing variable declarations
- **multiple-root-original-scopes-with-nil**: Multiple global root scopes with a
  null scope in between
- **sibling-ranges**: Multiple sibling root ranges
- **nested-ranges**: A root range with a nested function range
- **range-values**: A root range with a non-function range with bindings
- **sub-range-values**: A root range with sub-range bindings
- **range-call-site**: A function inlined into the root scope
- **hidden-ranges**: A function range corresponding to an original block scope
