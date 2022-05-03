# -*- mode: perl; -*-
# Copyright 2016-2022 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


## Test version negotiation

package ssltests;

use strict;
use warnings;

use List::Util qw/max min/;

use OpenSSL::Test;
use OpenSSL::Test::Utils qw/anydisabled alldisabled disabled/;
setup("no_test_here");

my @tls_protocols = ("SSLv3", "TLSv1", "TLSv1.1", "TLSv1.2", "TLSv1.3");
# undef stands for "no limit".
my @min_tls_protocols = (undef, "SSLv3", "TLSv1", "TLSv1.1", "TLSv1.2", "TLSv1.3");
my @max_tls_protocols = ("SSLv3", "TLSv1", "TLSv1.1", "TLSv1.2", "TLSv1.3", undef);

my @is_tls_disabled = anydisabled("ssl3", "tls1", "tls1_1", "tls1_2", "tls1_3");

my $min_tls_enabled; my $max_tls_enabled;

# Protocol configuration works in cascades, i.e.,
# $no_tls1_1 disables TLSv1.1 and below.
#
# $min_enabled and $max_enabled will be correct if there is at least one
# protocol enabled.
foreach my $i (0..$#tls_protocols) {
    if (!$is_tls_disabled[$i]) {
        $min_tls_enabled = $i;
        last;
    }
}

foreach my $i (0..$#tls_protocols) {
    if (!$is_tls_disabled[$i]) {
        $max_tls_enabled = $i;
    }
}

my @dtls_protocols = ("DTLSv1", "DTLSv1.2");
# undef stands for "no limit".
my @min_dtls_protocols = (undef, "DTLSv1", "DTLSv1.2");
my @max_dtls_protocols = ("DTLSv1", "DTLSv1.2", undef);

my @is_dtls_disabled = anydisabled("dtls1", "dtls1_2");

my $min_dtls_enabled; my $max_dtls_enabled;

# $min_enabled and $max_enabled will be correct if there is at least one
# protocol enabled.
foreach my $i (0..$#dtls_protocols) {
    if (!$is_dtls_disabled[$i]) {
        $min_dtls_enabled = $i;
        last;
    }
}

foreach my $i (0..$#dtls_protocols) {
    if (!$is_dtls_disabled[$i]) {
        $max_dtls_enabled = $i;
    }
}

sub no_tests {
    my ($dtls) = @_;
    return $dtls ? alldisabled("dtls1", "dtls1_2") :
      alldisabled("ssl3", "tls1", "tls1_1", "tls1_2", "tls1_3");
}

sub generate_version_tests {
    my ($method) = @_;

    my $dtls = $method eq "DTLS";
    # Don't write the redundant "Method = TLS" into the configuration.
    undef $method if !$dtls;

    my @protocols = $dtls ? @dtls_protocols : @tls_protocols;
    my @min_protocols = $dtls ? @min_dtls_protocols : @min_tls_protocols;
    my @max_protocols = $dtls ? @max_dtls_protocols : @max_tls_protocols;
    my $min_enabled  = $dtls ? $min_dtls_enabled : $min_tls_enabled;
    my $max_enabled  = $dtls ? $max_dtls_enabled : $max_tls_enabled;

    if (no_tests($dtls)) {
        return;
    }

    my @tests = ();

    for (my $sctp = 0; $sctp < ($dtls && !disabled("sctp") ? 2 : 1); $sctp++) {
        foreach my $c_min (0..$#min_protocols) {
            my $c_max_min = $c_min == 0 ? 0 : $c_min - 1;
            foreach my $c_max ($c_max_min..$#max_protocols) {
                foreach my $s_min (0..$#min_protocols) {
                    my $s_max_min = $s_min == 0 ? 0 : $s_min - 1;
                    foreach my $s_max ($s_max_min..$#max_protocols) {
                        my ($result, $protocol) =
                            expected_result($c_min, $c_max, $s_min, $s_max,
                                            $min_enabled, $max_enabled,
                                            \@protocols);
                        push @tests, {
                            "name" => "version-negotiation",
                            "client" => {
                                "MinProtocol" => $min_protocols[$c_min],
                                "MaxProtocol" => $max_protocols[$c_max],
                            },
                            "server" => {
                                "MinProtocol" => $min_protocols[$s_min],
                                "MaxProtocol" => $max_protocols[$s_max],
                            },
                            "test" => {
                                "ExpectedResult" => $result,
                                "ExpectedProtocol" => $protocol,
                                "Method" => $method,
                            }
                        };
                        $tests[-1]{"test"}{"UseSCTP"} = "Yes" if $sctp;
                    }
                }
            }
        }
    }
    return @tests if disabled("tls1_3") || disabled("tls1_2") || $dtls;

    #Add some version/ciphersuite sanity check tests
    push @tests, {
        "name" => "ciphersuite-sanity-check-client",
        "client" => {
            #Offering only <=TLSv1.2 ciphersuites with TLSv1.3 should fail
            "CipherString" => "AES128-SHA",
            "Ciphersuites" => "",
        },
        "server" => {
            "MaxProtocol" => "TLSv1.2"
        },
        "test" => {
            "ExpectedResult" => "ClientFail",
        }
    };
    push @tests, {
        "name" => "ciphersuite-sanity-check-server",
        "client" => {
            "CipherString" => "AES128-SHA",
            "MaxProtocol" => "TLSv1.2"
        },
        "server" => {
            #Allowing only <=TLSv1.2 ciphersuites with TLSv1.3 should fail
            "CipherString" => "AES128-SHA",
            "Ciphersuites" => "",
        },
        "test" => {
            "ExpectedResult" => "ServerFail",
        }
    };

    return @tests;
}

sub generate_resumption_tests {
    my ($method) = @_;

    my $dtls = $method eq "DTLS";
    # Don't write the redundant "Method = TLS" into the configuration.
    undef $method if !$dtls;

    my @protocols = $dtls ? @dtls_protocols : @tls_protocols;
    my $min_enabled  = $dtls ? $min_dtls_enabled : $min_tls_enabled;
    my $max_enabled = $dtls ? $max_dtls_enabled : $max_tls_enabled;

    if (no_tests($dtls)) {
        return;
    }

    my @server_tests = ();
    my @client_tests = ();

    # Obtain the first session against a fixed-version server/client.
    foreach my $original_protocol($min_enabled..$max_enabled) {
        # Upgrade or downgrade the server/client max version support and test
        # that it upgrades, downgrades or resumes the session as well.
        foreach my $resume_protocol($min_enabled..$max_enabled) {
            my $resumption_expected;
            # We should only resume on exact version match.
            if ($original_protocol eq $resume_protocol) {
                $resumption_expected = "Yes";
            } else {
                $resumption_expected = "No";
            }

            for (my $sctp = 0; $sctp < ($dtls && !disabled("sctp") ? 2 : 1);
                 $sctp++) {
                foreach my $ticket ("SessionTicket", "-SessionTicket") {
                    # Client is flexible, server upgrades/downgrades.
                    push @server_tests, {
                        "name" => "resumption",
                        "client" => { },
                        "server" => {
                            "MinProtocol" => $protocols[$original_protocol],
                            "MaxProtocol" => $protocols[$original_protocol],
                            "Options" => $ticket,
                        },
                        "resume_server" => {
                            "MaxProtocol" => $protocols[$resume_protocol],
                            "Options" => $ticket,
                        },
                        "test" => {
                            "ExpectedProtocol" => $protocols[$resume_protocol],
                            "Method" => $method,
                            "HandshakeMode" => "Resume",
                            "ResumptionExpected" => $resumption_expected,
                        }
                    };
                    $server_tests[-1]{"test"}{"UseSCTP"} = "Yes" if $sctp;
                    # Server is flexible, client upgrades/downgrades.
                    push @client_tests, {
                        "name" => "resumption",
                        "client" => {
                            "MinProtocol" => $protocols[$original_protocol],
                            "MaxProtocol" => $protocols[$original_protocol],
                        },
                        "server" => {
                            "Options" => $ticket,
                        },
                        "resume_client" => {
                            "MaxProtocol" => $protocols[$resume_protocol],
                        },
                        "test" => {
                            "ExpectedProtocol" => $protocols[$resume_protocol],
                            "Method" => $method,
                            "HandshakeMode" => "Resume",
                            "ResumptionExpected" => $resumption_expected,
                        }
                    };
                    $client_tests[-1]{"test"}{"UseSCTP"} = "Yes" if $sctp;
                }
            }
        }
    }

    if (!disabled("tls1_3") && !$dtls) {
        push @client_tests, {
            "name" => "resumption-with-hrr",
            "client" => {
            },
            "server" => {
                "Curves" => "P-256"
            },
            "resume_client" => {
            },
            "test" => {
                "ExpectedProtocol" => "TLSv1.3",
                "Method" => "TLS",
                "HandshakeMode" => "Resume",
                "ResumptionExpected" => "Yes",
            }
        };
    }

    push @client_tests, {
        "name" => "resumption-when-mfl-ext-is-missing",
        "server" => {
        },
        "client" => {
            "extra" => {
                "MaxFragmentLenExt" => 512,
            },
        },
        "resume_client" => {
        },
        "test" => {
            "Method" => $method,
            "HandshakeMode" => "Resume",
            "ResumptionExpected" => "No",
            "ExpectedResult" => "ServerFail",
        }
    };

    push @client_tests, {
        "name" => "resumption-when-mfl-ext-is-different",
        "server" => {
        },
        "client" => {
            "extra" => {
                "MaxFragmentLenExt" => 512,
            },
        },
        "resume_client" => {
            "extra" => {
                "MaxFragmentLenExt" => 1024,
            },
        },
        "test" => {
            "Method" => $method,
            "HandshakeMode" => "Resume",
            "ResumptionExpected" => "No",
            "ExpectedResult" => "ServerFail",
        }
    };

    push @client_tests, {
        "name" => "resumption-when-mfl-ext-is-correct",
        "server" => {
        },
        "client" => {
            "extra" => {
                "MaxFragmentLenExt" => 512,
            },
        },
        "resume_client" => {
            "extra" => {
                "MaxFragmentLenExt" => 512,
            },
        },
        "test" => {
            "Method" => $method,
            "HandshakeMode" => "Resume",
            "ResumptionExpected" => "Yes",
            "ExpectedResult" => "Success",
        }
    };

    return (@server_tests, @client_tests);
}

sub expected_result {
    my ($c_min, $c_max, $s_min, $s_max, $min_enabled, $max_enabled,
        $protocols) = @_;

    # Adjust for "undef" (no limit).
    $c_min = $c_min == 0 ? 0 : $c_min - 1;
    $c_max = $c_max == scalar @$protocols ? $c_max - 1 : $c_max;
    $s_min = $s_min == 0 ? 0 : $s_min - 1;
    $s_max = $s_max == scalar @$protocols ? $s_max - 1 : $s_max;

    # We now have at least one protocol enabled, so $min_enabled and
    # $max_enabled are well-defined.
    $c_min = max $c_min, $min_enabled;
    $s_min = max $s_min, $min_enabled;
    $c_max = min $c_max, $max_enabled;
    $s_max = min $s_max, $max_enabled;

    if ($c_min > $c_max) {
        # Client should fail to even send a hello.
        return ("ClientFail", undef);
    } elsif ($s_min > $s_max) {
        # Server has no protocols, should always fail.
        return ("ServerFail", undef);
    } elsif ($s_min > $c_max) {
        # Server doesn't support the client range.
        return ("ServerFail", undef);
    } elsif ($c_min > $s_max) {
        my @prots = @$protocols;
        if ($prots[$c_max] eq "TLSv1.3") {
            # Client will have sent supported_versions, so server will know
            # that there are no overlapping versions.
            return ("ServerFail", undef);
        } else {
            # Server will try with a version that is lower than the lowest
            # supported client version.
            return ("ClientFail", undef);
        }
    } else {
        # Server and client ranges overlap.
        my $max_common = $s_max < $c_max ? $s_max : $c_max;
        return ("Success", $protocols->[$max_common]);
    }
}

1;
