# Jujutsu (jj) in V8

> [!NOTE]
> **WIP Solution**: This is a temporary setup adapted from Chromium (`//tools/jj`)
> until the shared tooling is finalized in `depot_tools/jj`.

---

## 1. Setup

In your colocated V8 repository root:

```bash
# Initialize jj if not already done
jj git init --colocate .

# Link this config to the repo-level jj configuration
ln -sf "$(pwd)/tools/jj/config.toml" "$(jj config path --repo)"
```

---

## 2. How to use it

### Working with commits
Most commands for working locally with commits work just fine. Git submodules (DEPS) are not yet supported by `jj`: whenever you see `jj` say "ignoring git submodule" (or switch across commits with dependency changes), run `gclient sync`.

To start a new change off trunk:
```bash
jj git fetch
jj new trunk()
```

### Syncing code
```bash
jj sync
```
Uses the alias in `tools/jj/config.toml` (fetches `origin` and rebases current stack onto `trunk()`).

If a sync results in a conflict, resolve it directly in the files or use `jj resolve`. Use `jj evolog -r <revision>` to inspect the pre-conflict state.

### Uploading code
```bash
jj upload
```
Uses the alias in `tools/jj/config.toml` (runs `jj fix` on mutable revisions in the stack and uploads to Gerrit).

### Running formatters
```bash
jj fix
```
Uses the configuration in `tools/jj/config.toml` (`clang_format.py` for C++/JS/TS and `gn format` for GN/GNI).
