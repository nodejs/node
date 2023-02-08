OpenSSL FIPS support
====================

This release of OpenSSL includes a cryptographic module that can be
FIPS 140-2 validated. The module is implemented as an OpenSSL provider.
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

Installing the FIPS module
==========================

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
  about the module such as the self test status, and the module checksum.

The FIPS module must have the self tests run, and the FIPS module config file
output generated on every machine that it is to be used on. You must not copy
the FIPS module config file output data from one machine to another.

On Unix the `openssl fipsinstall` command will be invoked as follows by default:

    $ openssl fipsinstall -out /usr/local/ssl/fipsmodule.cnf -module /usr/local/lib/ossl-modules/fips.so

If you configured OpenSSL to be installed to a different location, the paths will
vary accordingly. In the rare case that you need to install the fipsmodule.cnf
to non-standard location, you can execute the `openssl fipsinstall` command manually.

Using the FIPS Module in applications
=====================================

Documentation about using the FIPS module is available on the [fips_module(7)]
manual page.

 [fips_module(7)]: https://www.openssl.org/docs/man3.0/man7/fips_module.html
