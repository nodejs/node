THIS CODE IS IN THE PUBLIC DOMAIN.

Work-in progress.  DANE support for OpenSSL.

This generalizes the SMTP specific DANE support in Postfix by
supporting all certificate usages and more versions of OpenSSL.

Documentation for now consists of the header file, and the example
connected.c program.  Real applications will get TLSA data from
DNS, rather than certificate files in the file-system.

On the other hand, certificate files make testing much easier, since
one can test TLSA records not found in the wild.  The offline.c
programs takes this idea further and verifies saved (or hand-crafted)
peer certificate chains without actually connecting a real peer.
This is used to test expected success/fail cases in the test-offline.sh
script.  The success test cases are easy to make reasonably
comprehensive, a comprehensive set of failure cases is a long-term
project.
