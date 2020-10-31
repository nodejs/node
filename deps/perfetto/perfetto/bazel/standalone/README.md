# Perfetto standalone Bazel config

This directory is only used in standalone builds.
The WORKSPACE aliases this directory to @perfetto_cfg.

Bazel-based embedders are supposed to:

### 1. Have a (modified) copy of perfetto_cfg.bzl in their repo

```
myproject/
  build/
    perfetto_overrides/
      perfetto_cfg.bzl
```

### 2. Have a repository rule that maps the directory to @perfetto_cfg

E.g in myproject/WORKSPACE
```
local_repository(
    name = "perfetto_cfg",
    path = "build/perfetto_overrides",
)
```
