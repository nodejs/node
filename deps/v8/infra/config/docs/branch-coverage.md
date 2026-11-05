Branches build coverage
======================

This is a description of V8 infrastructure for active release branches.

Builder coverage for active branches is organized in consoles that reflect the same schema used for the main branch:
 - [version].main
 - [version].memory
 - [version].ports

These consoles are the branch counterparts of the [main](https://ci.chromium.org/p/v8/g/main/console), [memory](https://ci.chromium.org/p/v8/g/memory/console) and [ports](https://ci.chromium.org/p/v8/g/ports/console) consoles for the main branch.


Monitoring
======================

V8 sheriffs will be notified on any failures in builders under branch main consoles.

Updating the active branch references
======================

To update branch references, update the `ACTIVE_BRANCHES` list in [definitions.star](https://chromium.googlesource.com/v8/v8/+/refs/heads/main/infra/config/definitions.star) and regenerate the lucicfg configs by running `./main.star`.


Adding new builders
======================

To propagate a new builder addition under main/memory/ports consoles you need to:
 - add a builder definition in one of the [multibranch consoles](https://chromium.googlesource.com/v8/v8/+/refs/heads/main/infra/config/builders/multibranch/) using the `multibranch_builder` abstraction.
 - specify the `first_branch_version` attribute - typically the current version on main
 - generate the configurations by running `./main.star`
 - backmerging mb_config.pyl and builders.pyl file changes to the respective branches is only required if the `first_branch_version` was not specified to the current main branch
