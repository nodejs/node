#! /usr/bin/env perl
# Copyright 2015-2025 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the Apache License 2.0 (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


use strict;
use warnings;

use File::Spec;
use OpenSSL::Test::Utils;
use OpenSSL::Test qw/:DEFAULT srctop_file result_file/;
use File::Compare qw/compare_text/;

setup("test_x509");

plan tests => 140;

# Prevent MSys2 filename munging for arguments that look like file paths but
# aren't
$ENV{MSYS2_ARG_CONV_EXCL} = "/CN=";

require_ok(srctop_file("test", "recipes", "tconversion.pl"));

my @certs = qw(test certs);
my $pem = srctop_file(@certs, "cyrillic.pem");
my $out_msb = "out-cyrillic.msb";
my $out_utf8 = "out-cyrillic.utf8";
my $der = "cyrillic.der";
my $der2 = "cyrillic.der";
my $msb = srctop_file(@certs, "cyrillic.msb");
my $utf = srctop_file(@certs, "cyrillic.utf8");

ok(run(app(["openssl", "x509", "-text", "-in", $pem, "-out", $out_msb,
            "-nameopt", "esc_msb"])));
is(cmp_text($out_msb, $msb),
   0, 'Comparing esc_msb output with cyrillic.msb');
ok(run(app(["openssl", "x509", "-text", "-in", $pem, "-out", $out_utf8,
            "-nameopt", "utf8"])));
is(cmp_text($out_utf8, $utf),
   0, 'Comparing utf8 output with cyrillic.utf8');

SKIP: {
    skip "EdDSA disabled", 2 if disabled("ecx");

    $pem = srctop_file(@certs, "tab-in-dn.pem");
    my $out_text = "out-tab-in-dn.text";
    my $text = srctop_file(@certs, "tab-in-dn.text");
    ok(run(app(["openssl", "x509", "-text", "-noout",
            "-in", $pem, "-out", $out_text])));
    is(cmp_text($out_text, $text),
       0, 'Comparing default output with tab-in-dn.text');
}

SKIP: {
    skip "DES disabled", 1 if disabled("des");
    skip "Platform doesn't support command line UTF-8", 1 if $^O =~ /^(VMS|msys)$/;

    my $p12 = srctop_file("test", "shibboleth.pfx");
    my $p12pass = "σύνθημα γνώρισμα";
    my $out_pem = "out.pem";
    ok(run(app(["openssl", "x509", "-text", "-in", $p12, "-out", $out_pem,
                "-passin", "pass:$p12pass"])));
    # not unlinking $out_pem
}

ok(!run(app(["openssl", "x509", "-in", $pem, "-inform", "DER",
             "-out", $der, "-outform", "DER"])),
   "Checking failure of mismatching -inform DER");
ok(run(app(["openssl", "x509", "-in", $pem, "-inform", "PEM",
            "-out", $der, "-outform", "DER"])),
   "Conversion to DER");
ok(!run(app(["openssl", "x509", "-in", $der, "-inform", "PEM",
             "-out", $der2, "-outform", "DER"])),
   "Checking failure of mismatching -inform PEM");

# producing and checking self-issued (but not self-signed) cert
my $subj = "/CN=CA"; # using same DN as in issuer of ee-cert.pem
my $extfile = srctop_file("test", "v3_ca_exts.cnf");
my $pkey = srctop_file(@certs, "ca-key.pem"); # issuer private key
my $pubkey = "ca-pubkey.pem"; # the corresponding issuer public key
# use any (different) key for signing our self-issued cert:
my $key = srctop_file(@certs, "serverkey.pem");
my $selfout = "self-issued.out";
my $testcert = srctop_file(@certs, "ee-cert.pem");
ok(run(app(["openssl", "pkey", "-in", $pkey, "-pubout", "-out", $pubkey]))
&& run(app(["openssl", "x509", "-new", "-force_pubkey", $pubkey, "-subj", $subj,
            "-extfile", $extfile, "-key", $key, "-out", $selfout]))
&& run(app(["openssl", "verify", "-no_check_time",
            "-trusted", $selfout, "-partial_chain", $testcert])));
# not unlinking $pubkey
# not unlinking $selfout

# test -set_issuer option
my $ca_issu = srctop_file(@certs, "ca-cert.pem"); # issuer cert
my $caout_issu = "ca-issu.out";
ok(run(app(["openssl", "x509", "-new", "-force_pubkey", $key, "-subj", "/CN=EE",
            "-set_issuer", "/CN=TEST-CA", "-extfile", $extfile, "-CA", $ca_issu,
            "-CAkey", $pkey, "-text", "-out", $caout_issu])));
ok(get_issuer($caout_issu) =~ /CN=TEST-CA/);
# not unlinking $caout

# simple way of directly producing a CA-signed cert with private/pubkey input
my $ca = srctop_file(@certs, "ca-cert.pem"); # issuer cert
my $caout = "ca-issued.out";
ok(run(app(["openssl", "x509", "-new", "-force_pubkey", $key, "-subj", "/CN=EE",
            "-extfile", $extfile, "-CA", $ca, "-CAkey", $pkey, "-out", $caout]))
&& run(app(["openssl", "verify", "-no_check_time",
            "-trusted", $ca, "-partial_chain", $caout])));

# test trust decoration
ok(run(app(["openssl", "x509", "-in", $ca, "-addtrust", "emailProtection",
            "-out", "ca-trusted.pem"])));
cert_contains("ca-trusted.pem", "Trusted Uses: E-mail Protection",
              1, 'trusted use - E-mail Protection');
ok(run(app(["openssl", "x509", "-in", $ca, "-addreject", "emailProtection",
            "-out", "ca-rejected.pem"])));
cert_contains("ca-rejected.pem", "Rejected Uses: E-mail Protection",
              1, 'rejected use - E-mail Protection');

subtest 'x509 -- x.509 v1 certificate' => sub {
    tconversion( -type => 'x509', -prefix => 'x509v1',
                 -in => srctop_file("test", "testx509.pem") );
};
subtest 'x509 -- first x.509 v3 certificate' => sub {
    tconversion( -type => 'x509', -prefix => 'x509v3-1',
                 -in => srctop_file("test", "v3-cert1.pem") );
};
subtest 'x509 -- second x.509 v3 certificate' => sub {
    tconversion( -type => 'x509', -prefix => 'x509v3-2',
                 -in => srctop_file("test", "v3-cert2.pem") );
};

subtest 'x509 -- pathlen' => sub {
    ok(run(test(["v3ext", srctop_file(@certs, "pathlen.pem")])));
};

cert_contains(srctop_file(@certs, "fake-gp.pem"),
              "2.16.528.1.1003.1.3.5.5.2-1-0000006666-Z-12345678-01.015-12345678",
              1, 'x500 -- subjectAltName');

cert_contains(srctop_file(@certs, "ext-noAssertion.pem"),
              "No Assertion",
              1, 'X.509 Not Assertion Extension');

cert_contains(srctop_file(@certs, "ext-groupAC.pem"),
              "Group Attribute Certificate",
              1, 'X.509 Group Attribute Certificate Extension');

cert_contains(srctop_file(@certs, "ext-sOAIdentifier.pem"),
              "Source of Authority",
              1, 'X.509 Source of Authority Extension');

cert_contains(srctop_file(@certs, "ext-noRevAvail.pem"),
              "No Revocation Available",
              1, 'X.509 No Revocation Available');

cert_contains(srctop_file(@certs, "ext-singleUse.pem"),
              "Single Use",
              1, 'X509v3 Single Use');

cert_contains(srctop_file(@certs, "ext-indirectIssuer.pem"),
              "Indirect Issuer",
              1, 'X.509 Indirect Issuer');

my $tgt_info_cert = srctop_file(@certs, "ext-targetingInformation.pem");
cert_contains($tgt_info_cert,
              "AC Targeting",
              1, 'X.509 Targeting Information Extension');
cert_contains($tgt_info_cert,
              "Targets:",
              1, 'X.509 Targeting Information Targets');
cert_contains($tgt_info_cert,
              "Target:",
              1, 'X.509 Targeting Information Target');
cert_contains($tgt_info_cert,
              "Target Name: DirName:CN = W",
              1, 'X.509 Targeting Information Target Name');
cert_contains($tgt_info_cert,
              "Target Group: DNS:wildboarsoftware.com",
              1, 'X.509 Targeting Information Target Name');
cert_contains($tgt_info_cert,
              "Issuer Names:",
              1, 'X.509 Targeting Information Issuer Names');
cert_contains($tgt_info_cert,
              "Issuer Serial: 01020304",
              1, 'X.509 Targeting Information Issuer Serial');
cert_contains($tgt_info_cert,
              "Issuer UID: B0",
              1, 'X.509 Targeting Information Issuer UID');
cert_contains($tgt_info_cert,
              "Digest Type: Public Key",
              1, 'X.509 Targeting Information Object Digest Type');

my $hnc_cert = srctop_file(@certs, "ext-holderNameConstraints.pem");
cert_contains($hnc_cert,
              "X509v3 Holder Name Constraints",
              1, 'X.509 Holder Name Constraints');
cert_contains($hnc_cert,
              "Permitted:",
              1, 'X.509 Holder Name Constraints Permitted');
cert_contains($hnc_cert,
              "DirName:CN = Wildboar",
              1, 'X.509 Holder Name Constraint');

my $dnc_cert = srctop_file(@certs, "ext-delegatedNameConstraints.pem");
cert_contains($dnc_cert,
              "X509v3 Delegated Name Constraints",
              1, 'X.509 Delegated Name Constraints');
cert_contains($dnc_cert,
              "Permitted:",
              1, 'X.509 Delegated Name Constraints Permitted');
cert_contains($dnc_cert,
              "DirName:CN = Wildboar",
              1, 'X.509 Delegated Name Constraint');
my $sda_cert = srctop_file(@certs, "ext-subjectDirectoryAttributes.pem");
cert_contains($sda_cert,
              "Steve Brule",
              1, 'X.509 Subject Directory Attributes');
cert_contains($sda_cert,
              "CN=Hi mom",
              1, 'X.509 Subject Directory Attributes');
cert_contains($sda_cert,
              "<No Values>",
              1, 'X.509 Subject Directory Attributes');
cert_contains($sda_cert,
              "Funkytown",
              1, 'X.509 Subject Directory Attributes');
cert_contains($sda_cert,
              "commonName",
              1, 'X.509 Subject Directory Attributes');
cert_contains($sda_cert,
              "owner",
              1, 'X.509 Subject Directory Attributes');
cert_contains($sda_cert,
              "givenName",
              1, 'X.509 Subject Directory Attributes');
cert_contains($sda_cert,
              "localityName",
              1, 'X.509 Subject Directory Attributes');

my $ass_info_cert = srctop_file(@certs, "ext-associatedInformation.pem");
cert_contains($ass_info_cert,
              "Steve Brule",
              1, 'X509v3 Associated Information');
cert_contains($ass_info_cert,
              "CN=Hi mom",
              1, 'X509v3 Associated Information');
cert_contains($ass_info_cert,
              "<No Values>",
              1, 'X509v3 Associated Information');
cert_contains($ass_info_cert,
              "Funkytown",
              1, 'X509v3 Associated Information');
cert_contains($ass_info_cert,
              "commonName",
              1, 'X509v3 Associated Information');
cert_contains($ass_info_cert,
              "owner",
              1, 'X509v3 Associated Information');
cert_contains($sda_cert,
              "givenName",
              1, 'X509v3 Associated Information');
cert_contains($ass_info_cert,
              "localityName",
              1, 'X509v3 Associated Information');

my $acc_cert_pol = srctop_file(@certs, "ext-acceptableCertPolicies.pem");
cert_contains($acc_cert_pol,
              "X509v3 Acceptable Certification Policies",
              1, 'X509v3 Acceptable Certification Policies');
# Yes, I know these OIDs make no sense in a policies extension. It's just a test.
cert_contains($acc_cert_pol,
              "organizationalUnitName",
              1, 'X509v3 Acceptable Certification Policies');
cert_contains($acc_cert_pol,
              "description",
              1, 'X509v3 Acceptable Certification Policies');

my $acc_priv_pol = srctop_file(@certs, "ext-acceptablePrivilegePolicies.pem");
cert_contains($acc_priv_pol,
              "X509v3 Acceptable Privilege Policies",
              1, 'X509v3 Acceptable Privilege Policies');
# Yes, I know these OIDs make no sense in a policies extension. It's just a test.
cert_contains($acc_priv_pol,
              "commonName",
              1, 'X509v3 Acceptable Certification Policies');
cert_contains($acc_priv_pol,
              "organizationName",
              1, 'X509v3 Acceptable Certification Policies');

my $user_notice_cert = srctop_file(@certs, "ext-userNotice.pem");
cert_contains($user_notice_cert,
              "Organization: Wildboar Software",
              1, 'X509v3 User Notice');
cert_contains($user_notice_cert,
              "Numbers: 123, 456",
              1, 'X509v3 User Notice');
cert_contains($user_notice_cert,
              "Explicit Text: Hey there big boi",
              1, 'X509v3 User Notice');
cert_contains($user_notice_cert,
              "Number: 50505",
              1, 'X509v3 User Notice');
cert_contains($user_notice_cert,
              "Explicit Text: Ice ice baby",
              1, 'X509v3 User Notice');

my $battcons_cert = srctop_file(@certs, "ext-basicAttConstraints.pem");
cert_contains($battcons_cert,
              "authority:TRUE",
              1, 'X.509 Basic Attribute Constraints Authority');
cert_contains($battcons_cert,
              "pathlen:3",
              1, 'X.509 Basic Attribute Constraints Path Length');

my $audit_id_cert = srctop_file(@certs, "ext-auditIdentity.pem");
cert_contains($audit_id_cert,
              "09:08:07",
              1, 'X509v3 Audit Identity');

my $iobo_cert = srctop_file(@certs, "ext-issuedOnBehalfOf.pem");
cert_contains($iobo_cert,
              "DirName:CN = Wildboar",
              1, 'X.509 Issued On Behalf Of');

my $auth_att_id_cert = srctop_file(@certs, "ext-authorityAttributeIdentifier.pem");
cert_contains($auth_att_id_cert,
              "DirName:CN = Wildboar",
              1, 'X.509 Authority Attribute Identifier');
cert_contains($auth_att_id_cert,
              "Issuer Serial: 01030507",
              1, 'X.509 Authority Attribute Identifier');
cert_contains($auth_att_id_cert,
              "Issuer UID: B2",
              1, 'X.509 Authority Attribute Identifier');

my $role_spec_cert = srctop_file(@certs, "ext-roleSpecCertIdentifier.pem");
cert_contains($role_spec_cert,
              "Role Name: DirName:CN = Wildboar",
              1, 'X.509 Role Spec Certificate Identifier');
cert_contains($role_spec_cert,
              "Role Certificate Issuer: DirName:CN",
              1, 'X.509 Role Spec Certificate Identifier');
cert_contains($role_spec_cert,
              "Role Certificate Serial Number: 33818120 \\(0x2040608\\)",
              1, 'X.509 Role Spec Certificate Identifier');
cert_contains($role_spec_cert,
              "DNS:wildboarsoftware.com",
              1, 'X.509 Role Spec Certificate Identifier');
cert_contains($role_spec_cert,
              "Registered ID:description",
              1, 'X.509 Role Spec Certificate Identifier');

my $attr_desc_cert = srctop_file(@certs, "ext-attributeDescriptor.pem");
cert_contains($attr_desc_cert,
              "Identifier: 2.5.4.3",
              1, 'X.509 Attribute Descriptor');
# This comes from the syntax field, which starts on the next line.
cert_contains($attr_desc_cert,
              "UnboundedDirectoryString",
              1, 'X.509 Attribute Descriptor');
cert_contains($attr_desc_cert,
              "Name: commonName",
              1, 'X.509 Attribute Descriptor');
# These comes from the dominationRule field.
cert_contains($attr_desc_cert,
              "Privilege Policy Identifier: 2.5.4.10",
              1, 'X.509 Attribute Descriptor');
cert_contains($attr_desc_cert,
              "DirName:CN = Wildboar",
              1, 'X.509 Attribute Descriptor');
cert_contains($attr_desc_cert,
              "Algorithm: sha256",
              1, 'X.509 Attribute Descriptor');

my $time_spec_abs_cert = srctop_file(@certs, "ext-timeSpecification-absolute.pem");
cert_contains($time_spec_abs_cert,
              "Timezone: UTC-05:00",
              1, 'X.509 Time Specification (Absolute)');
cert_contains($time_spec_abs_cert,
              "Absolute: Any time between Dec 20 13:07:21 2022 GMT and Dec 20 13:07:21 2022 GMT",
              1, 'X.509 Time Specification (Absolute)');

my $time_spec_per_cert = srctop_file(@certs, "ext-timeSpecification-periodic.pem");
cert_contains($time_spec_per_cert,
              "Timezone: UTC-05:00",
              1, 'X.509 Time Specification (Periodic)');
cert_contains($time_spec_per_cert,
              "NOT this time:",
              1, 'X.509 Time Specification (Periodic)');
cert_contains($time_spec_per_cert,
              "05:43:21 - 12:34:56",
              1, 'X.509 Time Specification (Periodic)');
cert_contains($time_spec_per_cert,
              "Days of the week: SUN, MON",
              1, 'X.509 Time Specification (Periodic)');
cert_contains($time_spec_per_cert,
              "Weeks of the month: 3, 4",
              1, 'X.509 Time Specification (Periodic)');
cert_contains($time_spec_per_cert,
              "Months: MAY, JUN",
              1, 'X.509 Time Specification (Periodic)');
cert_contains($time_spec_per_cert,
              "Years: 2022, 2023",
              1, 'X.509 Time Specification (Periodic)');
cert_contains($time_spec_per_cert,
              "Months: JUL, AUG",
              1, 'X.509 Time Specification (Periodic)');
cert_contains($time_spec_per_cert,
              "Years: 2023, 2024",
              1, 'X.509 Time Specification (Periodic)');

my $attr_map_cert = srctop_file(@certs, "ext-attributeMappings.pem");
cert_contains($attr_map_cert,
              "commonName == localityName",
              1, 'X.509 Attribute Mappings');
# localityName has an INTEGER value here, which was intentional to test the
# display of non-string values.
cert_contains($attr_map_cert,
              "commonName:asdf == localityName:03:3E",
              1, 'X.509 Attribute Mappings');

my $aaa_cert = srctop_file(@certs, "ext-allowedAttributeAssignments.pem");
cert_contains($aaa_cert,
              "Attribute Type: commonName",
              1, 'X.509 Allowed Attribute Assignments');
cert_contains($aaa_cert,
              "Holder Domain: email:jonathan.wilbur",
              1, 'X.509 Allowed Attribute Assignments');

my $aa_idp_cert = srctop_file(@certs, "ext-aAissuingDistributionPoint.pem");
cert_contains($aa_idp_cert,
              "DirName:CN = Wildboar",
              1, 'X.509 Attribute Authority Issuing Distribution Point');
cert_contains($aa_idp_cert,
              "CA Compromise",
              1, 'X.509 Attribute Authority Issuing Distribution Point');
cert_contains($aa_idp_cert,
              "Indirect CRL: TRUE",
              1, 'X.509 Attribute Authority Issuing Distribution Point');
cert_contains($aa_idp_cert,
              "Contains User Attribute Certificates: TRUE",
              1, 'X.509 Attribute Authority Issuing Distribution Point');
cert_contains($aa_idp_cert,
              "Contains Attribute Authority \\(AA\\) Certificates: TRUE",
              1, 'X.509 Attribute Authority Issuing Distribution Point');
cert_contains($aa_idp_cert,
              "Contains Source Of Authority \\(SOA\\) Public Key Certificates: TRUE",
              1, 'X.509 Attribute Authority Issuing Distribution Point');

sub test_errors { # actually tests diagnostics of OSSL_STORE
    my ($expected, $cert, @opts) = @_;
    my $infile = srctop_file(@certs, $cert);
    my @args = qw(openssl x509 -in);
    push(@args, $infile, @opts);
    my $tmpfile = 'out.txt';
    my $res =  grep(/-text/, @opts) ? run(app([@args], stdout => $tmpfile))
                                    : !run(app([@args], stderr => $tmpfile));
    my $found = 0;
    open(my $in, '<', $tmpfile) or die "Could not open file $tmpfile";
    while(<$in>) {
        print; # this may help debugging
        $res &&= !m/asn1 encoding/; # output must not include ASN.1 parse errors
        $found = 1 if m/$expected/; # output must include $expected
    }
    close $in;
    # $tmpfile is kept to help with investigation in case of failure
    return $res && $found;
}

# 3 tests for non-existence of spurious OSSL_STORE ASN.1 parse error output.
# This requires provoking a failure exit of the app after reading input files.
ok(test_errors("Bad output format", "root-cert.pem", '-outform', 'http'),
   "load root-cert errors");
ok(test_errors("RC2-40-CBC", "v3-certs-RC2.p12", '-passin', 'pass:v3-certs'),
   "load v3-certs-RC2 no asn1 errors"); # error msg should mention "RC2-40-CBC"
SKIP: {
    skip "sm2 not disabled", 1 if !disabled("sm2");

    ok(test_errors("Unable to load Public Key", "sm2.pem", '-text'),
       "error loading unsupported sm2 cert");
}

# 3 tests for -dateopts formats
ok(run(app(["openssl", "x509", "-noout", "-dates", "-dateopt", "rfc_822",
	     "-in", srctop_file("test/certs", "ca-cert.pem")])),
   "Run with rfc_8222 -dateopt format");
ok(run(app(["openssl", "x509", "-noout", "-dates", "-dateopt", "iso_8601",
	     "-in", srctop_file("test/certs", "ca-cert.pem")])),
   "Run with iso_8601 -dateopt format");
ok(!run(app(["openssl", "x509", "-noout", "-dates", "-dateopt", "invalid_format",
	     "-in", srctop_file("test/certs", "ca-cert.pem")])),
   "Run with invalid -dateopt format");

my $ca_cert = srctop_file(@certs, "ca-cert.pem");
my $goodcn2_chain = srctop_file(@certs, "goodcn2-chain.pem");

# -multi test with single cert
ok(run(app(["openssl", "x509", "-multi", "-in", $ca_cert])),
   "Run with -multi (single cert)");

# -multi test with multiple certs
my $outfile = result_file("multi.out");
ok(run(app(["openssl", "x509", "-multi", "-in", $goodcn2_chain, "-out", $outfile]))
   && compare_text($outfile, $goodcn2_chain) == 0,
   "Run with -multi (multiple certs)");

# Tests for signing certs (broken in 1.1.1o)
my $a_key = "a-key.pem";
my $a_cert = "a-cert.pem";
my $a2_cert = "a2-cert.pem";
my $ca_key = "ca-key.pem";
my $ca_cert = "ca-cert.pem";
my $cnf = srctop_file('apps', 'openssl.cnf');

# Create cert A
ok(run(app(["openssl", "req", "-x509", "-newkey", "rsa:2048",
            "-config", $cnf,
            "-keyout", $a_key, "-out", $a_cert, "-days", "365",
            "-nodes", "-subj", "/CN=test.example.com"])));
# Create cert CA - note key size
ok(run(app(["openssl", "req", "-x509", "-newkey", "rsa:4096",
            "-config", $cnf,
            "-keyout", $ca_key, "-out", $ca_cert, "-days", "3650",
            "-nodes", "-subj", "/CN=ca.example.com"])));
# Sign cert A with CA (errors on 1.1.1o)
ok(run(app(["openssl", "x509", "-in", $a_cert, "-CA", $ca_cert,
            "-CAkey", $ca_key, "-set_serial", "1234567890",
            "-preserve_dates", "-sha256", "-text", "-out", $a2_cert])));
# verify issuer is CA
ok(get_issuer($a2_cert) =~ /CN=ca.example.com/);

my $in_csr = srctop_file('test', 'certs', 'x509-check.csr');
my $in_key = srctop_file('test', 'certs', 'x509-check-key.pem');
my $invextfile = srctop_file('test', 'invalid-x509.cnf');
# Test that invalid extensions settings fail
ok(!run(app(["openssl", "x509", "-req", "-in", $in_csr, "-signkey", $in_key,
            "-out", "/dev/null", "-days", "3650" , "-extensions", "ext",
            "-extfile", $invextfile])));

# Tests for issue #16080 (fixed in 1.1.1o)
my $b_key = "b-key.pem";
my $b_csr = "b-cert.csr";
my $b_cert = "b-cert.pem";
# Create the CSR
ok(run(app(["openssl", "req", "-new", "-newkey", "rsa:4096",
            "-keyout", $b_key, "-out", $b_csr, "-nodes",
            "-config", $cnf,
            "-subj", "/CN=b.example.com"])));
# Sign it - position of "-text" matters!
ok(run(app(["openssl", "x509", "-req", "-text", "-CAcreateserial",
            "-CA", $ca_cert, "-CAkey", $ca_key,
            "-in", $b_csr, "-out", $b_cert])));
# Verify issuer is CA
ok(get_issuer($b_cert) =~ /CN=ca.example.com/);

# although no explicit extensions given:
has_version($b_cert, 3);
has_SKID($b_cert, 1);
has_AKID($b_cert, 1);

# Tests for https://github.com/openssl/openssl/issues/10442 (fixed in 1.1.1a)
# (incorrect default `-CAcreateserial` if `-CA` path has a dot in it)
my $folder_with_dot = "test_x509.folder";
ok(mkdir $folder_with_dot);
my $ca_cert_dot_in_dir = File::Spec->catfile($folder_with_dot, "ca-cert.pem");
ok(copy($ca_cert,$ca_cert_dot_in_dir));
my $ca_serial_dot_in_dir = File::Spec->catfile($folder_with_dot, "ca-cert.srl");

ok(run(app(["openssl", "x509", "-req", "-text", "-CAcreateserial",
            "-CA", $ca_cert_dot_in_dir, "-CAkey", $ca_key,
            "-in", $b_csr])));
ok(-e $ca_serial_dot_in_dir);

# Tests for explicit start and end dates of certificates
my %today = (strftime("%Y-%m-%d", gmtime) => 1);
my $enddate;
ok(run(app(["openssl", "x509", "-req", "-text",
	    "-key", $b_key,
	    "-not_before", "20231031000000Z",
	    "-not_after", "today",
            "-in", $b_csr, "-out", $b_cert]))
&& get_not_before($b_cert) =~ /Oct 31 00:00:00 2023 GMT/
&& ++$today{strftime("%Y-%m-%d", gmtime)}
&& (grep { defined $today{$_} } get_not_after_date($b_cert)));
# explicit start and end dates
ok(run(app(["openssl", "x509", "-req", "-text",
	    "-key", $b_key,
	    "-not_before", "20231031000000Z",
	    "-not_after", "20231231000000Z",
	    "-days", "99",
            "-in", $b_csr, "-out", $b_cert]))
&& get_not_before($b_cert) =~ /Oct 31 00:00:00 2023 GMT/
&& get_not_after($b_cert) =~ /Dec 31 00:00:00 2023 GMT/);
# start date today and days
%today = (strftime("%Y-%m-%d", gmtime) => 1);
$enddate = strftime("%Y-%m-%d", gmtime(time + 99 * 24 * 60 * 60));
ok(run(app(["openssl", "x509", "-req", "-text",
	    "-key", $b_key,
	    "-not_before", "today",
	    "-days", "99",
            "-in", $b_csr, "-out", $b_cert]))
&& ++$today{strftime("%Y-%m-%d", gmtime)}
&& (grep { defined $today{$_} } get_not_before_date($b_cert))
&& get_not_after_date($b_cert) eq $enddate);
# end date before start date
ok(!run(app(["openssl", "x509", "-req", "-text",
	      "-key", $b_key,
	      "-not_before", "today",
	      "-not_after", "20231031000000Z",
              "-in", $b_csr, "-out", $b_cert])));
# default days option
%today = (strftime("%Y-%m-%d", gmtime) => 1);
$enddate = strftime("%Y-%m-%d", gmtime(time + 30 * 24 * 60 * 60));
ok(run(app(["openssl", "x509", "-req", "-text",
	    "-key", $b_key,
            "-in", $b_csr, "-out", $b_cert]))
&& ++$today{strftime("%Y-%m-%d", gmtime)}
&& (grep { defined $today{$_} } get_not_before_date($b_cert))
&& get_not_after_date($b_cert) eq $enddate);

SKIP: {
    skip "EC is not supported by this OpenSSL build", 1
        if disabled("ec");
    my $psscert = srctop_file(@certs, "ee-self-signed-pss.pem");

    ok(run(test(["x509_test", $psscert])), "running x509_test");
}
