# Copyright 2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

# Quick transfer to the downloaded Text::Template

BEGIN {
    use File::Spec::Functions;
    use File::Basename;
    use lib catdir(dirname(__FILE__), "..", "..", "Text-Template-1.46", "lib");
    # Some unpackers on VMS convert periods in directory names to underscores
    use lib catdir(dirname(__FILE__), "..", "..", "Text-Template-1_46", "lib");
    use Text::Template;
    shift @INC;                 # Takes away the effect of use lib
    shift @INC;                 # Takes away the effect of use lib
}
1;
