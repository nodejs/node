MODIFYING OPENSSL SOURCE
========================

This document describes the way to add custom modifications to OpenSSL sources.

 If you are adding new public functions to the custom library build, you need to
 either add a prototype in one of the existing OpenSSL header files;
 or provide a new header file and edit
 [Configurations/unix-Makefile.tmpl](Configurations/unix-Makefile.tmpl)
 to pick up that file.

 After that perform the following steps:

    ./Configure -Werror --strict-warnings [your-options]
    make update
    make
    make test

 `make update` ensures that your functions declarations are added to
 `util/libcrypto.num` or `util/libssl.num`.
 If you plan to submit the changes you made to OpenSSL
 (see [CONTRIBUTING.md](CONTRIBUTING.md)), it's worth running:

    make doc-nits

 after running `make update` to ensure that documentation has correct format.

 `make update` also generates files related to OIDs (in the `crypto/objects/`
 folder) and errors.
 If a merge error occurs in one of these generated files then the
 generated files need to be removed and regenerated using `make update`.
 To aid in this process the generated files can be committed separately
 so they can be removed easily.
