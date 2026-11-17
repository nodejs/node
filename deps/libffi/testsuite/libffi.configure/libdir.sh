#!/bin/sh

set -eu

srcdir=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
test_root=$(mktemp -d "${TMPDIR:-/tmp}/libffi-libdir.XXXXXX")
trap 'rm -rf "$test_root"' EXIT HUP INT TERM

cat > "$test_root/cc" <<'EOF'
#!/bin/sh
for arg do
  if test "$arg" = -print-multi-os-directory; then
    echo ../lib64
    exit 0
  fi
done
exec gcc "$@"
EOF
chmod +x "$test_root/cc"

mkdir "$test_root/default" "$test_root/disabled"

(
  cd "$test_root/default"
  CC="$test_root/cc" "$srcdir/configure" --disable-builddir >/dev/null
)
grep -Fqx 'toolexeclibdir = ${libdir}/../lib64' \
  "$test_root/default/Makefile"

(
  cd "$test_root/disabled"
  CC="$test_root/cc" "$srcdir/configure" --disable-builddir \
    --disable-multi-os-directory >/dev/null
)
grep -Fqx 'toolexeclibdir = ${libdir}' "$test_root/disabled/Makefile"

check_explicit_libdir()
{
  name=$1
  shift
  builddir="$test_root/$name"
  mkdir "$builddir"
  (
    cd "$builddir"
    CC="$test_root/cc" "$srcdir/configure" --disable-builddir "$@" \
      >/dev/null
  )
  grep -Fqx 'libdir = /test/custom lib' "$builddir/Makefile"
  grep -Fqx 'toolexeclibdir = ${libdir}' "$builddir/Makefile"
}

# Keep this list synchronized with the libdir spellings accepted by the
# generated configure option parser.
check_explicit_libdir long-equals       '--libdir=/test/custom lib'
check_explicit_libdir long-separate     --libdir '/test/custom lib'
check_explicit_libdir libdi-equals      '--libdi=/test/custom lib'
check_explicit_libdir libdi-separate    --libdi '/test/custom lib'
check_explicit_libdir libd-equals       '--libd=/test/custom lib'
check_explicit_libdir libd-separate     --libd '/test/custom lib'
check_explicit_libdir single-equals     '-libdir=/test/custom lib'
check_explicit_libdir single-separate   -libdir '/test/custom lib'
