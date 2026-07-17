#!/usr/bin/env python3
import argparse
import re
import shutil
import sys
import tempfile
import zipfile
from pathlib import Path

def ensure_dir(p: Path):
    p.mkdir(parents=True, exist_ok=True)


def extract_zip(prefix: str, zip_path: Path, dst_third_party: Path):
    """Extract a third-party archive into dst_third_party/prefix.

    Typical structure inside the archive is a single top-level directory like
    "mbedtls-<ver>/". We extract under dst_third_party, then rename that
    directory to a stable name dst_third_party/prefix.
    """
    ensure_dir(dst_third_party)
    with zipfile.ZipFile(zip_path, 'r') as zf:
        # Determine the top-level directory inside the archive
        names = zf.namelist()
        top_candidates = {n.split('/', 1)[0] for n in names if '/' in n}
        top_dir = next((n for n in sorted(top_candidates) if n.startswith(f"{prefix}-")), None)
        if top_dir is None:  # There is no versioned top-level dir, create one
            top_dir = prefix
            extract_parent_dir = dst_third_party / prefix
            if not extract_parent_dir.exists():
                extract_parent_dir.mkdir()
        else:
            extract_parent_dir = dst_third_party
        print(f"Extracting {zip_path} -> {extract_parent_dir} (top_dir: {top_dir})")
        zf.extractall(extract_parent_dir)

    extracted_dir = dst_third_party / top_dir
    target_dir = dst_third_party / prefix
    if extracted_dir.exists() and extracted_dir != target_dir:
        if target_dir.exists():
            shutil.rmtree(target_dir)
        extracted_dir.rename(target_dir)

def find_single_subdir(root: Path) -> Path:
    entries = [p for p in root.iterdir() if p.is_dir()]
    if len(entries) == 1:
        return entries[0]
    # Try to pick the most likely one by name length (zip usually extracts into a single dir)
    if entries:
        return sorted(entries, key=lambda p: len(p.name))[0]
    return root


def cmake_configure_in(template: str, values: dict) -> str:
    # Handle @VAR@ replacement and #cmakedefine VAR @VAR@
    def repl_at(m):
        var = m.group(1)
        return str(values.get(var, ""))

    # Replace @VAR@
    s = re.sub(r"@([A-Za-z0-9_]+)@", repl_at, template)

    # Process #cmakedefine lines
    out_lines = []
    for line in s.splitlines():
        m = re.match(r"\s*#cmakedefine\s+([A-Za-z0-9_]+)\s+(\S+)", line)
        if m:
            name = m.group(1)
            var_token = m.group(2)
            # var_token was already replaced above (may still be a token if not found)
            # try to look it up from values if it's still in @VAR@ format
            val = 0
            if var_token.startswith("@") and var_token.endswith("@"):
                key = var_token.strip("@")
                val = int(values.get(key, 0) or 0)
            else:
                try:
                    val = int(var_token)
                except Exception:
                    val = 0
            if val:
                out_lines.append(f"#define {name} {val}")
            else:
                out_lines.append(f"/* #undef {name} */")
        else:
            out_lines.append(line)
    return "\n".join(out_lines) + "\n"


def write_file(path: Path, content: str):
    ensure_dir(path.parent)
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)


def parse_version_tuple(version: str):
    m = re.match(r"^(\d+)\.(\d+)\.(\d+)$", version)
    if not m:
        return (0, 0, 0)
    return tuple(int(x) for x in m.groups())


def main():
    ap = argparse.ArgumentParser(description='Prepare LIEF third-party and configured headers for checked-in sources')
    ap.add_argument('--source-dir', required=True, help='Path to the unzipped LIEF source directory (e.g. /tmp/LIEF-<ver>/)')
    ap.add_argument('--lief-dir', required=True, help='Target path to deps/LIEF (the vendored destination in this repo)')
    ap.add_argument('--version', help='LIEF version (X.Y.Z). If omitted, try to infer a fallback (0.17.0).')
    ap.add_argument('--commit', default='', help='Commit hash to embed into version.h (optional)')
    ap.add_argument('--tag', default='', help='Git tag to embed into version.h (optional)')
    args = ap.parse_args()

    source_dir = Path(args.source_dir).resolve()
    lief_dir = Path(args.lief_dir).resolve()
    if not source_dir.exists():
        print(f"source-dir does not exist: {source_dir}", file=sys.stderr)
        return 1
    if not lief_dir.exists():
        ensure_dir(lief_dir)

    # 1) Copy only the relevant files from source_dir to lief_dir
    included_dirs = ['src', 'include', 'config', 'README.md', 'LICENSE']
    for item in included_dirs:
        src = source_dir / item
        dst = lief_dir / item
        if not src.exists():
            print(f"Warning: expected item missing: {src}", file=sys.stderr)
            continue
        print(f"Copying {src} -> {dst}")
        if src.is_dir():
            if dst.exists():
                shutil.rmtree(dst)
            shutil.copytree(src, dst)
        else:
            shutil.copy2(src, dst)

    # 2) Extract selected third-party zips into third-party/<base>
    # Keep this generic via a small allowlist so we can extend later if needed.
    # For now, we only extract 'mbedtls'.
    src_third_party = source_dir / 'third-party'
    dst_third_party = lief_dir / 'third-party'
    ensure_dir(dst_third_party)

    # Use a temporary third party directory, then move needed files to lief_dir
    tmp_third_party = Path(tempfile.mkdtemp())
    # For example, src_third_party/mbedtls-<ver>.zip contains mbedtls-<ver>/,
    # we extract that to tmp_third_party/mbedtls, and in the extracted directory, we
    # only want to copy the 'library' folder, so tmp_third_party/mbedtls/library ->
    # dst_third_party/mbedtls/library. Any other files under the extracted mbedtls
    # directory will be ignored.
    libs_to_extract = {
        'mbedtls': ['library', 'include', 'README.md', 'LICENSE'],  # #include "mbedtls/private_access.h"
        'spdlog': ['include', 'src', 'LICENSE', 'README.md'],  # #include "spdlog/fmt/..."
        'frozen': ['include', 'LICENSE', 'README.rst'],      # #include <frozen/map.h>
        'expected': ['include', 'README.md', 'COPYING'],  # #include <LIEF/third-party/internal/expected.hpp>
        'utfcpp': ['source', 'README.md', 'LICENSE'],     # #include <internal/utfcpp/utf8/unchecked.h> | #include "utf8/unchecked.h"
        'tcb-span': ['span.hpp'],  # #include <LIEF/third-party/internal/span.hpp>
    }
    extracted_libs = []
    with tempfile.TemporaryDirectory() as _tmp_dir:
        tmp_third_party = Path(_tmp_dir)
        zip_files = sorted(src_third_party.glob('*.zip'))
        for lib, included in libs_to_extract.items():
            # Find the matching zip file that starts with lib name
            zip_path = next((z for z in zip_files if z.name.startswith(f"{lib}-")), None)
            if zip_path is None:
                print(f"Warning: No archive found for '{lib}' in {src_third_party}", file=sys.stderr)
                continue

            # Extract archive into a tmp location under tmp_third_party/{lib}
            extract_zip(lib, zip_path, tmp_third_party)
            extracted_libs.append(lib)

            # Move the allow-listed directories from each lib in tmp_third_party to dst_third_party
            src = tmp_third_party / lib
            dst = dst_third_party / lib
            for subpath in included:
                src_item = src / subpath
                dst_item = dst / subpath
                if not src_item.exists():
                    print(f"Warning: expected item missing: {src_item}", file=sys.stderr)
                    continue
                ensure_dir(dst_item.parent)
                if dst_item.exists():
                    if dst_item.is_dir():
                        shutil.rmtree(dst_item)
                    else:
                        dst_item.unlink()
                print(f"Moving {src_item} -> {dst_item}")
                shutil.move(str(src_item), str(dst_item))

    # 2) Place internal headers expected by the project layout under src/
    #    - third-party/expected/include/tl/expected.hpp
    #       -> include/LIEF/third-party/internal/expected.hpp
    #    - third-party/tcb-span/span.hpp
    #       -> include/LIEF/third-party/internal/span.hpp
    #    - third-party/utfcpp/source/utf8
    #       -> include/LIEF/third-party/internal/utfcpp/utf8
    internal_mappings = [
        ['third-party/expected/include/tl/expected.hpp', 'include/LIEF/third-party/internal/expected.hpp'],
        ['third-party/tcb-span/span.hpp', 'include/LIEF/third-party/internal/span.hpp'],
        ['third-party/utfcpp/source/utf8', 'include/LIEF/third-party/internal/utfcpp/utf8'],
    ]
    for src_rel, dst_rel in internal_mappings:
        src_path = lief_dir / src_rel
        dst_path = lief_dir / dst_rel
        if not src_path.exists():
            print(f"Warning: expected internal header missing: {src_path}", file=sys.stderr)
            continue
        print(f"Copying internal header {src_path} -> {dst_path}")
        ensure_dir(dst_path.parent)
        if dst_path.exists():
            if dst_path.is_dir():
                shutil.rmtree(dst_path)
            else:
                dst_path.unlink()
        if src_path.is_dir():
            shutil.copytree(src_path, dst_path)
        else:
            shutil.copy2(src_path, dst_path)

    # 3) Generate configured headers into src/LIEF/
    # Prefer templates from target include/, fall back to source include/ if missing.
    configs = [
        ['include/LIEF/config.h.in', 'include/LIEF/config.h'],
        ['include/LIEF/version.h.in', 'include/LIEF/version.h'],
        ['src/compiler_support.h.in', 'src/compiler_support.h'],
    ]

    # The CMakeLists.txt usually contains the default version values like this. Parse them.
    # set(LIEF_VERSION_MAJOR 0)
    # set(LIEF_VERSION_MINOR 17)
    # set(LIEF_VERSION_PATCH 0)
    cmakelists_path = source_dir / 'CMakeLists.txt'
    major_ver = minor_ver = patch_ver = 0
    if cmakelists_path.exists():
        with open(cmakelists_path, 'r', encoding='utf-8') as f:
            cmake_contents = f.read()
        m_major = re.search(r'set\s*\(\s*LIEF_VERSION_MAJOR\s+(\d+)\s*\)', cmake_contents)
        m_minor = re.search(r'set\s*\(\s*LIEF_VERSION_MINOR\s+(\d+)\s*\)', cmake_contents)
        m_patch = re.search(r'set\s*\(\s*LIEF_VERSION_PATCH\s+(\d+)\s*\)', cmake_contents)
        if m_major:
            major_ver = int(m_major.group(1))
        if m_minor:
            minor_ver = int(m_minor.group(1))
        if m_patch:
            patch_ver = int(m_patch.group(1))

    vals = {
        'PROJECT_NAME': 'LIEF',
        'LIEF_VERSION_MAJOR': major_ver,
        'LIEF_VERSION_MINOR': minor_ver,
        'LIEF_VERSION_PATCH': patch_ver,
        'LIEF_COMMIT_HASH': args.commit,
        'LIEF_IS_TAGGED': 0 if not args.tag else 1,
        'LIEF_GIT_TAG': args.tag or '',

        'LIEF_PE_SUPPORT': 1,
        'LIEF_ELF_SUPPORT': 1,
        'LIEF_MACHO_SUPPORT': 1,
        'LIEF_COFF_SUPPORT': 1,

        'LIEF_OAT_SUPPORT': 0,
        'LIEF_DEX_SUPPORT': 0,
        'LIEF_VDEX_SUPPORT': 0,
        'LIEF_ART_SUPPORT': 0,

        'LIEF_DEBUG_INFO_SUPPORT': 0,
        'LIEF_OBJC_SUPPORT': 0,
        'LIEF_DYLD_SHARED_CACHE_SUPPORT': 0,
        'LIEF_ASM_SUPPORT': 0,
        'LIEF_EXTENDED': 0,

        'ENABLE_JSON_SUPPORT': 0,
        'LIEF_JSON_SUPPORT': 0,
        'LIEF_LOGGING_SUPPORT': 0,
        'LIEF_LOGGING_DEBUG_SUPPORT': 0,
        'LIEF_FROZEN_ENABLED': 1,

        'LIEF_EXTERNAL_EXPECTED': 0,
        'LIEF_EXTERNAL_UTF8CPP': 0,
        'LIEF_EXTERNAL_MBEDTLS': 0,
        'LIEF_EXTERNAL_FROZEN': 0,
        'LIEF_EXTERNAL_SPAN': 0,

        'LIEF_NLOHMANN_JSON_EXTERNAL': 0,

        'LIEF_SUPPORT_CXX11': 1,
        'LIEF_SUPPORT_CXX14': 1,
        'LIEF_SUPPORT_CXX17': 1,
    }

    for cfg_in_rel, cfg_out_rel in configs:
        cfg_in_path = lief_dir / cfg_in_rel
        cfg_out_path = lief_dir / cfg_out_rel
        if not cfg_in_path.exists():
            print(f"Warning: config template missing: {cfg_in_path}", file=sys.stderr)
            continue
        with open(cfg_in_path, 'r', encoding='utf-8') as f:
            template = f.read()
        configured = cmake_configure_in(template, vals)
        ensure_dir(cfg_out_path.parent)
        write_file(cfg_out_path, configured)

    extracted_summary = ', '.join(sorted(set(extracted_libs))) if extracted_libs else 'none'
    print(f'LIEF preparation complete: third-party extracted from {source_dir} into {lief_dir} [{extracted_summary}] and headers configured into src/')
    return 0


if __name__ == '__main__':
    sys.exit(main())
