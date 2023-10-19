OpenSSL FIPS support
====================

This release of OpenSSL includes a cryptographic module that can be
FIPS validated. The module is implemented as an OpenSSL provider.
A provider is essentially a dynamically loadable module which implements
cryptographic algorithms, see the [README-PROVIDERS](README-PROVIDERS.md) file
for further details.

A cryptographic module is only FIPS validated after it has gone through the complex
FIPS 140 validation process. As this process takes a very long time, it is not
possible to validate every minor release of OpenSSL.
If you need a FIPS validated module then you must ONLY generate a FIPS provider
using OpenSSL versions that have valid FIPS certificates. A FIPS certificate
contains a link to a Security Policy, and you MUST follow the instructions
in the Security Policy in order to be FIPS compliant.
See <https://www.openssl.org/source/> for information related to OpenSSL
FIPS certificates and Security Policies.

Newer OpenSSL Releases that include security or bug fixes can be used to build
all other components (such as the core API's, TLS and the default, base and
legacy providers) without any restrictions, but the FIPS provider must be built
as specified in the Security Policy (normally with a different version of the
source code).

The OpenSSL FIPS provider is a shared library called `fips.so` (on Unix), or
resp. `fips.dll` (on Windows). The FIPS provider does not get built and
installed automatically. To enable it, you need to configure OpenSSL using
the `enable-fips` option.

Installing the FIPS provider
============================

In order to be FIPS compliant you must only use FIPS validated source code.
Refer to <https://www.openssl.org/source/> for information related to
which versions are FIPS validated. The instructions given below build OpenSSL
just using the FIPS validated source code.

If you want to use a validated FIPS provider, but also want to use the latest
OpenSSL release to build everything else, then refer to the next section.

The following is only a guide.
Please read the Security Policy for up to date installation instructions.

If the FIPS provider is enabled, it gets installed automatically during the
normal installation process. Simply follow the normal procedure (configure,
make, make test, make install) as described in the [INSTALL](INSTALL.md) file.

For example, on Unix the final command

    $ make install

effectively executes the following install targets

    $ make install_sw
    $ make install_ssldirs
    $ make install_docs
    $ make install_fips     # for `enable-fips` only

The `install_fips` make target can also be invoked explicitly to install
the FIPS provider independently, without installing the rest of OpenSSL.

The Installation of the FIPS provider consists of two steps. In the first step,
the shared library is copied to its installed location, which by default is

    /usr/local/lib/ossl-modules/fips.so                  on Unix, and
    C:\Program Files\OpenSSL\lib\ossl-modules\fips.dll   on Windows.

In the second step, the `openssl fipsinstall` command is executed, which completes
the installation by doing the following two things:

- Runs the FIPS module self tests
- Generates the so-called FIPS module configuration file containing information
  about the module such as the module checksum (and for OpenSSL 3.0 the
  self test status).

The FIPS module must have the self tests run, and the FIPS module config file
output generated on every machine that it is to be used on. For OpenSSL 3.0,
you must not copy the FIPS module config file output data from one machine to another.

On Unix the `openssl fipsinstall` command will be invoked as follows by default:

    $ openssl fipsinstall -out /usr/local/ssl/fipsmodule.cnf -module /usr/local/lib/ossl-modules/fips.so

If you configured OpenSSL to be installed to a different location, the paths will
vary accordingly. In the rare case that you need to install the fipsmodule.cnf
to a non-standard location, you can execute the `openssl fipsinstall` command manually.

Installing the FIPS provider and using it with the latest release
=================================================================

This normally requires you to download 2 copies of the OpenSSL source code.

Download and build a validated FIPS provider
--------------------------------------------

Refer to <https://www.openssl.org/source/> for information related to
which versions are FIPS validated. For this example we use OpenSSL 3.0.0.

    $ wget https://www.openssl.org/source/openssl-3.0.0.tar.gz
    $ tar -xf openssl-3.0.0.tar.gz
    $ cd openssl-3.0.0
    $ ./Configure enable-fips
    $ make
    $ cd ..

Download and build the latest release of OpenSSL
------------------------------------------------

We use OpenSSL 3.1.0 here, (but you could also use the latest 3.0.X)

    $ wget https://www.openssl.org/source/openssl-3.1.0.tar.gz
    $ tar -xf openssl-3.1.0.tar.gz
    $ cd openssl-3.1.0
    $ ./Configure enable-fips
    $ make

Use the OpenSSL FIPS provider for testing
-----------------------------------------

We do this by replacing the artifact for the OpenSSL 3.1.0 FIPS provider.
Note that the OpenSSL 3.1.0 FIPS provider has not been validated
so it must not be used for FIPS purposes.

    $ cp ../openssl-3.0.0/providers/fips.so providers/.
    $ cp ../openssl-3.0.0/providers/fipsmodule.cnf providers/.
    // Note that for OpenSSL 3.0 that the `fipsmodule.cnf` file should not
    // be copied across multiple machines if it contains an entry for
    // `install-status`. (Otherwise the self tests would be skipped).

    // Validate the output of the following to make sure we are using the
    // OpenSSL 3.0.0 FIPS provider
    $ ./util/wrap.pl -fips apps/openssl list -provider-path providers \
    -provider fips -providers

    // Now run the current tests using the OpenSSL 3.0 FIPS provider.
    $ make tests

Copy the FIPS provider artifacts (`fips.so` & `fipsmodule.cnf`) to known locations
-------------------------------------------------------------------------------------

    $ cd ../openssl-3.0.0
    $ sudo make install_fips

Check that the correct FIPS provider is being used
--------------------------------------------------

    $./util/wrap.pl -fips apps/openssl list -provider-path providers \
    -provider fips -providers

    // This should produce the following output
    Providers:
      base
        name: OpenSSL Base Provider
        version: 3.1.0
        status: active
      fips
        name: OpenSSL FIPS Provider
        version: 3.0.0
        status: active

Using the FIPS Module in applications
=====================================

Documentation about using the FIPS module is available on the [fips_module(7)]
manual page.

 [fips_module(7)]: https://www.openssl.org/docs/man3.0/man7/fips_module.html
