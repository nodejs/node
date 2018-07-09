# Copyright (C) 2016 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html
# *   Copyright (C) 2009, International Business Machines
# *   Corporation and others.  All Rights Reserved.
# A list of txt's to build
# Note: 
#
#   If you are thinking of modifying this file, READ THIS. 
#
# Instead of changing this file [unless you want to check it back in],
# you should consider creating a 'brklocal.mk' file in this same directory.
# Then, you can have your local changes remain even if you upgrade or
# reconfigure ICU.
#
# Example 'spreplocal.mk' files:
#
#  * To add an additional locale to the list: 
#    _____________________________________________________
#    |  SPREP_SOURCE_LOCAL =   myStringPrep.txt ...
#
#  * To REPLACE the default list and only build a subset of files:
#    _____________________________________________________
#    |  SPREP_SOURCE = rfc4518.txt
#
#
SPREP_SOURCE = \
rfc3491.txt rfc3530cs.txt rfc3530csci.txt rfc3530mixp.txt rfc3722.txt \
rfc3920node.txt rfc3920res.txt rfc4011.txt rfc4013.txt rfc4505.txt \
rfc4518.txt rfc4518ci.txt
