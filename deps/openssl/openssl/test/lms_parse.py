#!/usr/bin/env python
# Copyright 2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html

# A python program written to parse (version 1.0) of the ACVP test vectors for
# LMS. The file that can be processed by this utility can be downloaded from
#  https://raw.githubusercontent.com/usnistgov/ACVP-Server/refs/heads/master/gen-val/json-files/LMS-sigVer-1.0/internalProjection.json
# and output from this utility to
#  test/recipes/30-test_evp_data/evppkey_lms.txt
#
# e.g. python3 mldsa_parse.py ~/Downloads/internalProjection.json > ./test/recipes/30-test_evp_data/evppkey_lms.txt
#
import json
import argparse
import datetime

def print_label(label, value):
    print(label + " = " + value)

def print_hexlabel(label, tag, value):
    print(label + " = hex" + tag + ":" + value)

def parse_lms_sig_ver(groups):
    for grp in groups:
        lmsmode = grp["lmsMode"]
        lmotsmode = grp["lmOtsMode"]
        name = lmsmode + "_" + str(grp["tgId"])
        pubkey = grp["publicKey"]

        if grp["testType"] != "AFT":
            continue

        print_label("Title", lmsmode + " " + lmotsmode)
        print("");
        print_label("PublicKeyRaw", name + ":" + "LMS" + ":" + pubkey)
        for tst in grp['tests']:
            testname = lmsmode + "_" + str(tst['tcId'])
            print("");
            if "reason" in tst:
                print("# " + tst['reason'])
            print_label("FIPSversion", ">=3.6.0")
            print_label("Verify-Message-Public", "LMS:" + name)
            print_label("Input", tst['message'])
            print_label("Output", tst['signature'])
            if not tst['testPassed']:
                print_label("Result", "VERIFY_ERROR")

parser = argparse.ArgumentParser(description="")
parser.add_argument('filename', type=str)
args = parser.parse_args()

# Open and read the JSON file
with open(args.filename, 'r') as file:
    data = json.load(file)

year = datetime.date.today().year
version = data['vsId']
revision = data['revision']
algorithm = data['algorithm']

print("# Copyright " + str(year) + " The OpenSSL Project Authors. All Rights Reserved.")
print("#")
print("# Licensed under the Apache License 2.0 (the \"License\").  You may not use")
print("# this file except in compliance with the License.  You can obtain a copy")
print("# in the file LICENSE in the source distribution or at")
print("# https://www.openssl.org/source/license.html\n")
print("# ACVP test data for " + algorithm + " generated from")
print("# https://raw.githubusercontent.com/usnistgov/ACVP-Server/refs/heads/master/gen-val/json-files/LMS-sigVer-1.0/internalProjection.json")
print("# [version " + str(version) + " : revision " + str(revision) + "]")
print("")

if algorithm == "LMS":
    parse_lms_sig_ver(data['testGroups'])
else:
    print("Unsupported algorithm " + algorithm)
