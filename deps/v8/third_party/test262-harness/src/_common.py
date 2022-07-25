# Copyright (c) 2012 Ecma International.  All rights reserved.
# This code is governed by the BSD license found in the LICENSE file.

#--Imports---------------------------------------------------------------------
import parseTestRecord

#--Stubs-----------------------------------------------------------------------

#--Globals---------------------------------------------------------------------

#--Helpers--------------------------------------------------------------------#

def convertDocString(docString):
    envelope = parseTestRecord.parseTestRecord(docString, '')
    envelope.pop('header', None)
    envelope.pop('test', None)

    return envelope
