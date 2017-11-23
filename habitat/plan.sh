
pkg_name=node
pkg_origin=nodejs
pkg_description="Node.js® is a JavaScript runtime built on Chrome's V8 JavaScript engine."
pkg_upstream_url=https://nodejs.org/
pkg_license=('MIT')
pkg_maintainer="The NodeJS Maintainers"
pkg_deps=(core/glibc core/gcc-libs)
pkg_build_deps=(core/python2 core/gcc core/grep core/make)
pkg_bin_dirs=(bin)
pkg_include_dirs=(include)
pkg_interpreters=(bin/node)
pkg_lib_dirs=(lib)

pkg_version() {
  "$(pkg_path_for core/python2)/bin/python2" ../tools/getnodeversion.py
}

do_before() {
  update_pkg_version
}

do_prepare() {
  # ./configure has a shebang of #!/usr/bin/env python2. Fix it.
  sed -e "s#/usr/bin/env python#$(pkg_path_for python2)/bin/python2#" -i configure
}

do_build() {
  ./configure \
    --prefix "${pkg_prefix}" \
    --dest-cpu "x64" \
    --dest-os "linux"

  make -j"$(nproc)"
}

do_install() {
  do_default_install

  # Node produces a lot of scripts that hardcode `/usr/bin/env`, so we need to
  # fix that everywhere to point directly at the env binary in core/coreutils.
  grep -nrlI '^\#\!/usr/bin/env' "$pkg_prefix" | while read -r target; do
    sed -e "s#\#\!/usr/bin/env node#\#\!${pkg_prefix}/bin/node#" -i "$target"
  done
}
