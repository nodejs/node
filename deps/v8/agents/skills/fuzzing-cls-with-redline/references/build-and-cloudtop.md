# Cloudtop Execution, Build Matrix, and Plan Schema Reference

Use `scripts/prepare_run.py` for host inspection, GN builds, binary isolation,
`nodump.so`, `selftest`, and clean shutdown. Run it locally when `HOST=local`,
or pipe it over SSH (`python3 - < ...`) when `HOST` is a remote Cloudtop.

## 1. Host Connection and Capacity Probe

### Local Cloudtop (`HOST=local`)

If `python3 "$SKILL_DIR/scripts/prepare_run.py" probe` reports
`"is_cloudtop": true` and the user gave no remote hostname, run all commands
locally without SSH:

```shell
TARGET_HOME="$HOME"
RUN_ROOT="$TARGET_HOME/cl-fuzz/run-$(date +%Y%m%d-%H%M%S)"
TARGET_V8="$TARGET_HOME/cl-fuzz/v8-wt"
mkdir -p "$RUN_ROOT"
```

### Remote Cloudtop (`HOST=user.c.googlers.com`)

Open one 24 h persistent SSH `ControlMaster`. Redirect standard streams so
non-interactive agent shells do not hang:

```shell
CTL="/tmp/rl-cl-fuzz-%C"
SOCK=$(ssh -G -o ControlPath="$CTL" "$HOST" | awk '$1=="controlpath"{print $2}')
if [ -S "$SOCK" ] && ! ssh -o ControlPath="$CTL" -O check "$HOST" 2>/dev/null; then
  rm -f "$SOCK"
fi
ssh -o ControlPath="$CTL" -O check "$HOST" 2>/dev/null || \
  ssh -fN -o ControlMaster=yes -o ControlPersist=24h -o ServerAliveInterval=60 \
    -o ControlPath="$CTL" "$HOST" </dev/null >/dev/null 2>&1
```

If SSH prompts for a security-key touch or `gcert`, tell the user to authorize
the connection before you retry. For all later SSH calls, pass
`-o BatchMode=yes -o ConnectTimeout=10 -o ControlPath="$CTL"` so that an expired
socket fails immediately.

Probe remote capacity and active runs:

```shell
ssh -o BatchMode=yes -o ConnectTimeout=10 -o ControlPath="$CTL" "$HOST" \
  "python3 - probe" < "$SKILL_DIR/scripts/prepare_run.py"
```

Capture `TARGET_HOME` from the JSON `home` field and set:

```shell
RUN_ROOT="$TARGET_HOME/cl-fuzz/run-$(date +%Y%m%d-%H%M%S)"
TARGET_V8="$TARGET_HOME/cl-fuzz/v8"
ssh -o BatchMode=yes -o ConnectTimeout=10 -o ControlPath="$CTL" "$HOST" \
  "mkdir -p '$RUN_ROOT'"
```

If `linger` is `no`, run `loginctl enable-linger "$USER"` on the Cloudtop.
CAUTION: If polkit blocks linger, warn the user that systemd can delete
`/dev/shm` when SSH closes. Copy `launch_arms.py` to `$RUN_ROOT/launch_arms.py`
and start it inside a persistent `tmux` session on the Cloudtop.

______________________________________________________________________

## 2. Sync the Target Change Into an Isolated Checkout

### Export the Patch File

Set `SRC_REPO` and `HEAD_REF` from Step 1 (`SRC_REPO="$V8_ROOT"` and
`HEAD_REF="<ref>"` for a local branch, or `SRC_REPO="$WORK_DIR/wt"` and
`HEAD_REF="HEAD"` for a Gerrit CL).

For committed changes, run:

```shell
BASE=$(git -C "$SRC_REPO" merge-base origin/main "$HEAD_REF")
git -C "$SRC_REPO" diff --binary "$BASE" "$HEAD_REF" > "$WORK_DIR/cl_fuzz.patch"
```

To include uncommitted tracked edits on `HEAD`, run:

```shell
BASE=$(git -C "$SRC_REPO" merge-base origin/main HEAD)
git -C "$SRC_REPO" diff --binary "$BASE" > "$WORK_DIR/cl_fuzz.patch"
```

Note: `git diff` includes tracked files only. To include new untracked files,
run `git -C "$SRC_REPO" add -N <file>` first.

### Local Cloudtop (`HOST=local`)

Reuse the main V8 checkout through a dedicated git worktree and symlinked
`gclient` dependencies (this avoids a 20 GB second `fetch v8`):

```shell
MAIN_V8=$(cd "$V8_ROOT" && cd "$(git rev-parse --git-common-dir)/.." && pwd)
git -C "$MAIN_V8" fetch origin main && \
  git -C "$MAIN_V8" worktree prune
if [ ! -d "$TARGET_V8/.git" ] && [ ! -f "$TARGET_V8/.git" ]; then
  mkdir -p "$TARGET_HOME/cl-fuzz" && \
    git -C "$MAIN_V8" worktree add --detach "$TARGET_V8" origin/main
fi
git -C "$TARGET_V8" reset --hard && \
  git -C "$TARGET_V8" clean -fd && \
  git -C "$TARGET_V8" checkout -B cl-fuzz-target origin/main
```

Apply the patch file when it is not empty, commit it so that `change_targets.py`
sees the diff at `HEAD`, and update the `gclient` symlinks:

```shell
if [ -s "$WORK_DIR/cl_fuzz.patch" ]; then
  git -C "$TARGET_V8" apply --3way "$WORK_DIR/cl_fuzz.patch" && \
    git -C "$TARGET_V8" add -A && \
    git -C "$TARGET_V8" -c user.name=cl-fuzz -c user.email=cl-fuzz@localhost \
      commit -m "cl-fuzz target patch"
fi
python3 "$MAIN_V8/tools/dev/setup_worktree_build.py" "$MAIN_V8" "$TARGET_V8"
```

### Remote Cloudtop (`HOST != local`)

First-time setup (only when `$TARGET_V8` does not exist on the Cloudtop):

```shell
export PATH="$HOME/depot_tools:$PATH"
mkdir -p "$HOME/cl-fuzz" && \
  cd "$HOME/cl-fuzz" && fetch --nohooks v8 && cd v8 && gclient sync -D
```

Copy the exported patch file to the Cloudtop:

```shell
scp -o BatchMode=yes -o ControlPath="$CTL" \
  "$WORK_DIR/cl_fuzz.patch" "$HOST:$RUN_ROOT/cl_fuzz.patch"
```

Reset, sync, and apply cleanly on the Cloudtop inside `$TARGET_V8`:

```shell
export PATH="$HOME/depot_tools:$PATH"
git reset --hard && git clean -fd && \
  git fetch origin main && git checkout -B cl-fuzz-target origin/main && \
  gclient sync -D
if [ -s "$RUN_ROOT/cl_fuzz.patch" ]; then
  git apply --3way "$RUN_ROOT/cl_fuzz.patch" && \
    git add -A && \
    git -c user.name=cl-fuzz -c user.email=cl-fuzz@localhost \
      commit -m "cl-fuzz target patch"
fi
```

If `git apply --3way` has merge conflicts, stop immediately and report the files
with conflicts to the user.

### Compatibility Gate

Make sure that `$TARGET_V8` supports `--bundle-reprl` and `v8_redline`:

```shell
grep -q -- '--bundle-reprl' "$TARGET_V8/src/d8/d8.cc" && \
  grep -q 'v8_redline' "$TARGET_V8/gni/v8.gni"
```

______________________________________________________________________

## 3. Build, Stage, Selftest, Start, and Stop Commands

`scripts/prepare_run.py` owns the exact `build_variant -> out/rl_*` mapping:

| `build_variant`        | Output Directory       | Key GN Arguments                                                  |
| ---------------------- | ---------------------- | ----------------------------------------------------------------- |
| `asan_slow_dcheck`     | `out/rl_asan_slow`     | `is_asan=true`, `v8_enable_slow_dchecks=true`, `inline-bool-flag` |
| `counters_slow_dcheck` | `out/rl_counters`      | `v8_enable_slow_dchecks=true`, `inline-8bit-counters,pc-table`    |
| `counters_dcheck`      | `out/rl_counters_fast` | `dcheck_always_on=true`, `inline-8bit-counters,pc-table`          |
| `slow_dcheck`          | `out/rl_slow_dcheck`   | `v8_enable_slow_dchecks=true`, `inline-bool-flag`                 |
| `verif`                | `out/rl_verif`         | `v8_enable_verification_features=true`, `inline-bool-flag`        |
| `arm64sim`             | `out/rl_arm64sim`      | `v8_target_cpu="arm64"`, `inline-bool-flag` (or counters)         |

CAUTION: Do not set `v8_enable_sandbox_hardware_support=true` with
`inline-bool-flag` or `inline-8bit-counters`. The GN build fails.

### Copy `plan.json` to `$RUN_ROOT/plan.json`

```shell
# Local:
cp -f "$WORK_DIR/plan.json" "$RUN_ROOT/plan.json"
# Remote:
scp -o BatchMode=yes -o ControlPath="$CTL" \
  "$WORK_DIR/plan.json" "$HOST:$RUN_ROOT/plan.json"
```

### Build (`out/rl_bin/redline` and planned `d8` variants)

`prepare_run.py build` tries `use_remoteexec=true` first. If reclient is
unavailable, it retries locally with `-j "$(nproc)" -l "$(nproc)"`. If C++
compilation fails, it stops immediately and prints the compiler errors.

```shell
# Local:
python3 "$SKILL_DIR/scripts/prepare_run.py" build \
  --v8-dir "$TARGET_V8" --plan "$RUN_ROOT/plan.json"
# Remote:
ssh -o BatchMode=yes -o ConnectTimeout=10 -o ControlPath="$CTL" "$HOST" \
  "python3 - build --v8-dir '$TARGET_V8' --plan '$RUN_ROOT/plan.json'" \
  < "$SKILL_DIR/scripts/prepare_run.py"
```

### Stage Isolated Binaries, Compile `nodump.so`, and Run `selftest`

```shell
# Local:
python3 "$SKILL_DIR/scripts/prepare_run.py" stage \
  --v8-dir "$TARGET_V8" --run-root "$RUN_ROOT" --plan "$RUN_ROOT/plan.json"
# Remote:
ssh -o BatchMode=yes -o ConnectTimeout=10 -o ControlPath="$CTL" "$HOST" \
  "python3 - stage --v8-dir '$TARGET_V8' --run-root '$RUN_ROOT' \
   --plan '$RUN_ROOT/plan.json'" < "$SKILL_DIR/scripts/prepare_run.py"
```

### Start Fuzzing Arms

`launch_arms.py` checks `plan.json`, computes `targets_*.json` (reusing the
cached edge map in `out/rl_*/redline/edges.jsonl.gz`), and writes
`arm_meta.json`. It then starts each arm detached (`start_new_session=True`) and
makes sure that every arm survives startup:

```shell
# Local:
python3 "$SKILL_DIR/scripts/launch_arms.py" \
  --v8-dir "$TARGET_V8" --run-root "$RUN_ROOT" --plan "$RUN_ROOT/plan.json"
# Remote:
ssh -o BatchMode=yes -o ConnectTimeout=10 -o ControlPath="$CTL" "$HOST" \
  "python3 - --v8-dir '$TARGET_V8' --run-root '$RUN_ROOT' \
   --plan '$RUN_ROOT/plan.json'" < "$SKILL_DIR/scripts/launch_arms.py"
```

Pass `--only-stopped` to restart only arms that stopped without affecting
running arms.

### Stop a Fuzzing Run Cleanly

`prepare_run.py stop` sends one `SIGINT` to every active `redline.pid`, waits up
to 60 s for clean `end` events, and stops any leftover `d8` workers whose
command line points inside `$RUN_ROOT/bin/`:

```shell
# Local:
python3 "$SKILL_DIR/scripts/prepare_run.py" stop --run-root "$RUN_ROOT"
# Remote:
ssh -o BatchMode=yes -o ConnectTimeout=10 -o ControlPath="$CTL" "$HOST" \
  "python3 - stop --run-root '$RUN_ROOT'" < "$SKILL_DIR/scripts/prepare_run.py"
```

______________________________________________________________________

## 4. `plan.json` Input Schema (Read by `prepare_run.py` and `launch_arms.py`)

Edit only valid `plan.json` keys. Use `d8_extra_args` (never `extra_flags`):

```json
{
  "arms": [
    {
      "name": "arm-asan-slow",
      "build_variant": "asan_slow_dcheck",
      "builtins_coverage": true,
      "change_based": false,
      "preset": "maglev",
      "d8_extra_args": ["--maglev-assert", "--single-threaded"],
      "excluded_flags": [],
      "extra_gn_args": [],
      "generator_weights": {"TypedArrayOps": 5},
      "randomize_d8_flags": true,
      "d8_flag_profile": "maglev",
      "jobs": 8,
      "duration_sec": 43200,
      "timeout_ms": 800,
      "seeds": "test/mjsunit/maglev,test/mjsunit/compiler"
    }
  ]
}
```
