# To be used as an overlay
(final: prev: {
  R =
    let
      inherit (final)
        lib
        stdenv
        stdenvNoCC
        fetchurl
        blas
        bzip2
        curlMinimal
        gfortran
        lapack
        ncurses
        pcre2
        readline
        removeReferencesTo
        runCommand
        tzdata
        which
        xz
        zlib
        ;
    in
    prev.R.overrideAttrs {
      buildInputs = [
        blas
        bzip2
        gfortran
        lapack
        ncurses
        pcre2
        readline
        which
        xz
        zlib
      ];

      nativeBuildInputs = prev.R.nativeBuildInputs ++ [ removeReferencesTo ];

      preConfigure = ''
        configureFlagsArray=(
          --disable-lto
          --without-recommended-packages
          --with-blas="-L${blas}/lib -lblas"
          --with-lapack="-L${lapack}/lib -llapack"
          --with-readline
          --without-aqua
          --without-tcltk
          --without-cairo
          --without-libpng
          --without-jpeglib
          --without-libtiff
          --without-ICU
          --without-x
          --disable-R-framework
          --disable-java
          AR=$(type -p ar)
          AWK=$(type -p gawk)
          CC=$(type -p cc)
          CXX=$(type -p c++)
          FC="${gfortran}/bin/gfortran" F77="${gfortran}/bin/gfortran"
          RANLIB=$(type -p ranlib)
          CURL_CONFIG="${lib.getExe' (lib.getDev curlMinimal) "curl-config"}"
          r_cv_have_curl728=yes
          R_SHELL="${stdenvNoCC.shell}"
      ''
      + lib.optionalString stdenv.hostPlatform.isDarwin ''
        OBJC="clang"
        CPPFLAGS="-isystem ${lib.getInclude stdenv.cc.libcxx}/include/c++/v1"
        LDFLAGS="-L${lib.getLib stdenv.cc.libcxx}/lib"
      ''
      + ''
        )
        echo >>etc/Renviron.in "TZDIR=${tzdata}/share/zoneinfo"
      '';

      # Upstream postInstall link to the `tex` output, which we don't need
      postInstall = "mv -T \"$out/lib/R/share/texmf\" \"$tex\"";
      postFixup =
        prev.R.postFixup
        # Keep the C/C++ compiler out of R's runtime closure (enforced by
        # outputChecks.disallowedReferences below). R records the absolute path of the
        # compiler it was built with in Makeconf (and a couple of launchers); rewrite
        # those to bare command names so packages are compiled with the toolchain
        # provided by their own build environment. Only the compiler's runtime
        # libraries (…-lib) are needed at run time, so repoint the recorded library
        # search paths, and strip any residual paths recorded in compiled objects
        # (e.g. debug/.comment sections).
        + ''
          compilers='cc|gcc|g\+\+|c\+\+|cpp|clang|clang\+\+|gccgo|gfortran|g77|ld|ld\.gold|ld\.bfd|ld\.lld|ar|ranlib|nm|as|strip|dsymutil|install_name_tool|libtool|lipo|otool'
          for f in \
            $out/lib/R/etc/Makeconf $out/lib/R/etc/Renviron \
            $out/lib/R/bin/R $out/bin/R \
            $out/lib/R/bin/libtool $out/lib/R/bin/javareconf \
          ; do
            [ -f "$f" ] && sed -i -E "s#/nix/store/[a-z0-9]{32}-[^/ \"')]*/bin/($compilers)#\1#g" "$f"
          done

          substituteInPlace \
              $out/lib/R/etc/Makeconf \
          ${
            if stdenv.hostPlatform.isDarwin then
              # On Darwin the toolchain is clang. After bare-naming, CC/CXX are plain
              # `cc`/`c++`, which resolve via PATH to the gfortran cc-wrapper's GCC
              # drivers in an R-package build env; GCC's libstdc++ headers are
              # incompatible with the SDK libcxx (e.g. "'abort' has not been declared in
              # 'std'"). Pin them to clang/clang++, which the gfortran wrapper does not
              # provide (so PATH resolves to the real clang) and which are not store
              # paths (so the disallowedReferences check still passes).
              ''
                --replace-fail 'CC = cc' 'CC = clang' \
                --replace-fail 'CXX = c++' 'CXX = clang++' \
                --replace-fail 'CXX17 = c++' 'CXX17 = clang++' \
                --replace-fail 'CXX20 = c++' 'CXX20 = clang++' \
                --replace-fail 'CXX23 = c++' 'CXX23 = clang++'
              ''
            else
              # On Linux gfortran.cc is the same derivation as stdenv.cc.cc (the full
              # gcc), which the disallowedReferences check forbids, so repoint its
              # recorded library search paths to the -lib output. This must NOT run on
              # Darwin: there gfortran is a separate derivation from clang (so the check
              # already passes) and its -lib output lacks lib/gcc/<triple>/<ver> and the
              # libemutls_w/libheapt_w archives that Fortran packages link against.
              ''
                  $out/lib/R/etc/ldpaths \
                --replace-fail "${gfortran.cc}" "${lib.getLib gfortran.cc}"
              ''
          }

          ${lib.optionalString (!stdenv.hostPlatform.isDarwin) ''
            substituteInPlace $out/lib/R/bin/libtool \
                  --replace-fail "${stdenv.cc.cc}" "${lib.getLib stdenv.cc.cc}"''}

          # Neutralise any residual references that are not a plain /bin/<tool> path,
          # e.g. the compiler resource dir baked into libtool's library search path.
          for f in $out/lib/R/etc/Makeconf $out/lib/R/bin/libtool; do
            [ -f "$f" ] && remove-references-to -t ${stdenv.cc} "$f"
          done

          rm $out/lib/R/bin/javareconf

          find $out -type f -name '*.${stdenv.hostPlatform.extensions.sharedLibrary}' -exec \
            remove-references-to -t ${stdenv.cc} -t ${stdenv.cc.cc} {} +

          find $out -type f -exec remove-references-to -t "$man" {} +

          # 1) Drop recorded `# configure …` / comment lines that pin build-time
          #    store paths (R-man via --mandir, the *-dev pkgconfig set, …).
          #    Pure metadata — never used at run time.
          sed -i -E '\|^#.*/nix/store/[a-z0-9]{32}|d' $out/lib/R/etc/Makeconf

          # 2) Bare-name the external tool commands R records for
          #    install.packages()/vignettes (gzip, bzip2, sed, tar, …). R uses
          #    libz/libbz2 internally to *read* compressed data, so dropping the
          #    store path (keeping the bare name, resolved from PATH if present)
          #    doesn't affect running scripts or loading packages.
          tools='gzip|bzip2|bunzip2|sed|tar|gtar|make|gmake|unzip|zip|xz|gawk|awk'
          for f in $out/lib/R/etc/Renviron $out/lib/R/etc/Makeconf; do
            [ -f "$f" ] && sed -i -E "s#/nix/store/[a-z0-9]{32}-[^/ \"')]*/bin/($tools)#\1#g" "$f"
          done

          # 3) full gfortran (its lib/gcc/<triple>/<ver> crt dir) baked into libtool.
          [ -f $out/lib/R/bin/libtool ] && \
            remove-references-to -t ${gfortran.cc} -t ${final.coreutils} -t ${final.gnugrep} -t ${final.gnused} $out/lib/R/bin/libtool
        '';

      # Enforce that the compiler wrapper and the unwrapped compiler do not end up in
      # R's runtime closure. On Linux the postFixup above scrubs the references that
      # would otherwise pull them in.
      __structuredAttrs = true;
      outputChecks.out.disallowedReferences = [
        "man"
        "tex"
        stdenv.cc
        stdenv.cc.cc
      ];
    };
})
