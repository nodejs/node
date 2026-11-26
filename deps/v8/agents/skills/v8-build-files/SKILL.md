---
name: v8-build-files
description: >-
  Modifies, synchronizes, and verifies V8 build files across GN (BUILD.gn,
  gni/*.gni), Bazel (BUILD.bazel, MODULE.bazel, bazel/**), and DEPS. Use when
  adding, removing, or renaming source, header, or Torque files in build
  targets, introducing build-system flags (declare_args, v8_flag) or
  preprocessor defines, wiring or opting out of third-party dependencies, or
  updating DEPS gclient_gn_args and include_rules. Don't use for adding runtime
  flags in src/flags/flag-definitions.h, running gm.py builds, executing test
  suites, or editing C++/JS/Torque code without build file changes.
---

# Working with V8 Build Files (`v8-build-files`)

Keep V8's GN (`BUILD.gn`, `gni/*.gni`), Bazel (`BUILD.bazel`, `MODULE.bazel`,
`bazel/**`), and `DEPS` configurations synchronized and compatible across
standalone V8, Chromium, Node.js, custom-`DEPS` embedders (such as PDFium), and
downstream Copybara Bazel consumers.

## 1. Cross-Build Synchronization (`LINT.IfChange`)

Gerrit's server-side `If This Then That` analyzer enforces two `LINT.IfChange` /
`LINT.ThenChange` blocks in `BUILD.gn`:

1. **`BUILD.gn` -> `BUILD.bazel`**: Synchronize added, removed, or renamed
   `.cc`, `.h`, and `.tq` files, `declare_args()` flags, preprocessor `defines`,
   and third-party `deps` in `BUILD.bazel`. Add `NO_IFTTT=<reason>` on its own
   line *above* any `TAG=` / `CONV=` lines and git trailers only when an edit
   applies strictly to GN-only infrastructure inside root `BUILD.gn`'s
   `LINT.IfChange` blocks (such as GN cluster/split static-library plumbing or
   `v8_generate_features_json`). Placing a multi-word `NO_IFTTT=...` line below
   `TAG=` fails `PRESUBMIT.py`'s footer check.
2. **`v8_dump_build_config` (`BUILD.gn`) -> `bazel/defs.bzl`**: When adding a
   test-framework status switch to `v8_build_config.json` in `BUILD.gn`, add the
   matching key to `build_config_content` in `bazel/defs.bzl`.

## 2. Bazel CPU, OS, and Compiler Config Targets (`@v8//bazel/config`)

Downstream Bazel consumers (including Copybara-synced builds) replace the entire
`@v8//bazel/config` package with their own `BUILD` file implementing the public
`:is_*` and `:v8_target_*` targets from `bazel/config/v8-target-cpu.bzl` and
`bazel/config/BUILD.bazel`.

1. **Always use public `:is_*` and `:v8_target_*` targets**: Only reference
   `:is_*` and `:v8_target_*` targets from `BUILD.bazel` and `bazel/*.bzl`.
   Leave `:platform_cpu_*` targets internal to `bazel/config/BUILD.bazel`, as
   downstream `@v8//bazel/config` replacements do not define `:platform_cpu_*`.
2. **Map GN architecture names to Bazel config names**:
   - Supported Bazel CPU suffixes are
     `{x64, ia32, arm64, arm, s390x, riscv64, ppc64le}`.
   - Translate GN `"x86"` -> `ia32` and GN `"ppc64"` -> `ppc64le`. Bazel has no
     config targets for `loong64`, `mips64el`, `riscv32`, or `zos`.
   - **Host / compilation CPU** (matches GN `current_cpu` and C++
     `V8_HOST_ARCH_*`): Select on
     `@v8//bazel/config:is_{x64,ia32,arm64,arm,s390x,riscv64,ppc64le}` (plus
     `:is_64bit`, `:is_inline_asm_*`, and `:is_msvc_asm_*`). Use these for
     host-compiled third-party libraries (such as BoringSSL), host inline/MSVC
     assembly, and compiler flags.
   - **V8 target CPU** (matches GN `v8_current_cpu` / `v8_target_cpu` and C++
     `V8_TARGET_ARCH_*`, including simulator builds): Select on
     `@v8//bazel/config:v8_target_{x64,ia32,arm64,arm,s390x,riscv64,ppc64le}`
     (plus `:v8_arm64_simulator` and `:v8_target_is_32_bits`). Use these for
     `src/codegen/<arch>/`, `src/regexp/<arch>/`, `src/wasm/baseline/<arch>/`,
     and `V8_TARGET_ARCH_*` defines.
3. **Translate GN CPU exclusions (`!=`) into explicit `match_any` allowlists**:
   `@v8//bazel/config` has no `:is_not_<cpu>` targets, and combining `:is_<cpu>`
   with `:is_v8_<flag>` keys inside a single `select({...})` triggers an
   ambiguous-match error in Bazel. Instead, list the supported CPUs in a
   `match_any` group and intersect with `match_all`:
   ```python
   selects.config_setting_group(
       name = "foo_supported_cpu",
       match_any = [
           "@v8//bazel/config:is_x64",
           "@v8//bazel/config:is_ia32",
           "@v8//bazel/config:is_arm64",
           "@v8//bazel/config:is_arm",
           "@v8//bazel/config:is_riscv64",
       ],
   )

   selects.config_setting_group(
       name = "enable_foo",
       match_all = [
           ":is_v8_use_foo",
           ":foo_supported_cpu",
       ],
   )
   ```
4. **OS, compiler, and sanitizer settings**: Select on
   `@v8//bazel/config:is_{linux,android,macos,windows,posix,not_android}`
   (compilation OS),
   `@v8//bazel/config:v8_target_{linux,android,macos,windows,is_not_android}`
   (V8 target OS), `@v8//bazel/config:is_{clang,gcc,debug,opt,fastbuild}`, and
   `@v8//bazel/config:{asan,hwasan,msan,ubsan}_enabled`.

## 3. Adding Source, Header, Torque, and Unit-Test Files

1. **GN (`BUILD.gn`, `test/unittests/BUILD.gn`)**:
   - Keep all `sources` lists sorted alphabetically.
   - Add core V8 internal `.h` and `-inl.h` files to `v8_internal_headers` (and
     new `V8_OBJECT` headers to `src/objects/all-objects.h`), `.cc` files to the
     owning `v8_source_set` / `v8_cluster_source_set`
     (`v8_base_without_compiler`, `v8_compiler_sources`, `v8_initializers`,
     etc.), and `.tq` files to `torque_files`. Place headers from
     component-isolated subsystems (`src/base/`, `src/libplatform/`,
     `src/bigint/`, `src/heap/base/`, `src/heap/cppgc-internal/`, `src/flags/`,
     `src/inspector/`) in their owning target (`v8_libbase`, `v8_libplatform`,
     `v8_bigint`, `v8_heap_base_headers`, `cppgc_base`, `v8_flags`,
     `src/inspector:inspector`) rather than `v8_internal_headers`.
   - Add C++ unit tests to `v8_unittests_sources` in `test/unittests/BUILD.gn`.
     Annotate internal classes and functions called directly from `v8_unittests`
     with `V8_EXPORT_PRIVATE` (`src/base/macros.h`) so
     `is_component_build = true` builds link cleanly.
2. **Bazel (`BUILD.bazel`)**:
   - Mirror `.cc` and `.h` additions in the corresponding `filegroup`:
     `:v8_base_without_compiler_files` (or `:icu/v8_base_without_compiler_files`
     for Intl files), `:v8_compiler_files`, `:noicu/v8_initializers_files` (or
     `:icu/v8_initializers_files`), `:v8_libbase_files`,
     `:v8_libplatform_files`, `:v8_headers_files`, `:v8_config_headers_files`,
     `:cppgc_headers_files`, or `:wee8_files`. Mirror `.tq` additions in
     `:noicu/torque_files` (or `:icu/torque_files` for Intl `.tq` files).
   - Add shared third-party library `deps` to
     `v8_library(name = "v8_libshared")` (using `+ select({...})` when
     conditional). If a V8 header under `src/` directly `#include`s a new
     third-party header, also expose that dependency to `run_metagen`
     (`cc_compilation_context_from` / `metagen_sandbox_headers`), since
     `run_metagen` parses headers in a hermetic sandbox without `v8_libshared`'s
     `deps`.

## 4. Adding Build Flags and Preprocessor Defines

1. **Internal vs. public header boundary (`BUILD.gn`)**:
   - **Internal defines (read only under `src/`)**: Add
     `if (v8_...) { defines += [ "V8_..." ] }` to `config("features")` in
     `BUILD.gn`, and keep `V8_...` strictly as a presence macro tested with
     `#ifdef V8_...`.
     - If C++ code also needs a `bool` constant expression (for `if constexpr`
       or arithmetic, like `V8_ENABLE_SANDBOX_BOOL`), keep `V8_...` as a
       presence macro and add a
       `#ifdef V8_... / #define V8_..._BOOL true / #else / #define V8_..._BOOL false / #endif`
       companion in `src/common/globals.h` rather than defining `V8_...` to `0`
       (which breaks `#ifdef V8_...`).
     - Keep internal `config("features")` macros out of `include/v8config.h` and
       all `include/` headers, because embedder translation units do not receive
       `config("features")`.
   - **Public API defines (read in `include/`)**: Add the macro name
     unconditionally to `external_v8_defines` and conditionally append it to
     `enabled_external_v8_defines` in `BUILD.gn` (`config("v8_header_features")`
     and `gen_v8_gn` derive from those two lists automatically), and export the
     flag in `v8_generate_features_json` (`v8_features.json`, consumed by
     Node.js and Electron).
2. **Bazel flags (`BUILD.bazel` and `bazel/defs.bzl`)**:
   - Declare boolean options with `v8_flag(name = "v8_...", default = ...)`,
     which automatically generates `:is_<name>` and `:is_not_<name>`
     `config_setting` targets. Use `v8_string` or `v8_int` for non-boolean
     options.
   - Wire direct boolean-to-define mappings into `define_flags`'s
     `conditional_defines = { "v8_...": "V8_..." }`, or append
     `select({ ":enable_foo": ["V8_..."], "//conditions:default": [] })` to
     `define_flags`'s `defines` for compound conditions.

## 5. Third-Party Dependencies, `DEPS`, and Embedders

1. **`DEPS`, `.gitignore`, and `gclient_gn_args`**:
   - Add every new `DEPS` checkout path to `.gitignore` (enforced on upload by
     `PRESUBMIT.py`'s `_CheckDepsGitignored` /
     `tools/dev/verify_deps_ignored.py`).
   - Define standalone checkout flags in `vars` as plain booleans (e.g.
     `'checkout_foo': True`), list `'checkout_foo'` in `gclient_gn_args`
     (`build/config/gclient_args.gni`), and combine conditions on the `deps`
     entry (`'condition': 'checkout_foo and not build_with_chromium'`). Keeping
     `checkout_foo` as a boolean rather than `'not build_with_chromium'` avoids
     evaluating `checkout_foo` to `false` when `build_with_chromium` is `True`.
   - Account for custom-`DEPS` GN embedders (such as PDFium, where
     `build_with_chromium = false` and V8's `gclient_gn_args` are not generated)
     and Node.js GN builds (`build_with_node = true`). Default optional
     third-party GN args in `gni/v8.gni` using
     `build_with_chromium || (defined(checkout_foo) && checkout_foo)` (plus
     `(defined(build_with_node) && build_with_node)` when Node supplies an
     equivalent target such as `$node_openssl_path`), intersected with any
     `current_cpu` exclusions.
   - Guard any `"../../third_party/foo/..."` entries in `tools/gcmole/BUILD.gn`
     (`v8_gcmole_files` `data`) behind the corresponding GN flag so checkouts
     with `checkout_foo = False` do not fail isolate archival.
   - Scope `+foo` `include_rules` to the narrowest `src/<subdir>/DEPS`
     (`specific_include_rules`) rather than root `DEPS` whenever third-party
     headers are confined to an internal implementation file.
2. **Bazel external dependencies (`MODULE.bazel` and `BUILD.bazel`)**:
   - Declare external modules via `bazel_dep(...)` (plus `local_path_override`
     or `single_version_override` when sourced from `third_party/`) in
     `MODULE.bazel`.
   - Use established external target labels in `BUILD.bazel`
     (`@abseil-cpp//absl/...`, `@boringssl//:crypto`, `@highway//:hwy`,
     `//external:icu`, `//third_party/fast_float/src:fast_float`, `:lib_fp16`,
     `:lib_dragonbox`, `:simdutf`). Downstream Copybara scripts rewrite these
     exact target strings to internal equivalents, so coordinate any new `@repo`
     label with downstream Copybara configuration.

## 6. Verification Procedure

Run these checks before committing build or `DEPS` changes:

1. Format modified GN and Markdown files:
   ```bash
   gn format BUILD.gn gni/v8.gni
   git cl format
   ```
2. Verify GN generation and `#include` visibility in the default configuration,
   and test any new opt-out flag in a separate scratch directory:
   ```bash
   gn check out/x64.optdebug
   gn gen out/gn_optout_check --args='is_debug=false v8_use_foo=false' --check && rm -rf out/gn_optout_check
   ```
3. Verify `DEPS` include rules when headers or `DEPS` files change:
   ```bash
   python3 buildtools/checkdeps/checkdeps.py --root .
   ```
4. Run local upload presubmits (verifies `.gitignore` sync for `DEPS`, `DEPS`
   syntax, and commit footers; note that `LINT.IfChange` / `LINT.ThenChange` is
   checked server-side on Gerrit after upload):
   ```bash
   SKIP_GCE_AUTH_FOR_GIT=1 git cl presubmit --upload
   ```
