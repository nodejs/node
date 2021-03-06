#!/bin/sh
# Copyright 2013-2016 The OpenSSL Project Authors. All Rights Reserved.
#
# Licensed under the OpenSSL license (the "License").  You may not use
# this file except in compliance with the License.  You can obtain a copy
# in the file LICENSE in the source distribution or at
# https://www.openssl.org/source/license.html


# Utility to recreate S/MIME certificates

OPENSSL=../../apps/openssl
OPENSSL_CONF=./ca.cnf
export OPENSSL_CONF

# Root CA: create certificate directly
CN="Test S/MIME RSA Root" $OPENSSL req -config ca.cnf -x509 -nodes \
	-keyout smroot.pem -out smroot.pem -newkey rsa:2048 -days 3650

# EE RSA certificates: create request first
CN="Test S/MIME EE RSA #1" $OPENSSL req -config ca.cnf -nodes \
	-keyout smrsa1.pem -out req.pem -newkey rsa:2048
# Sign request: end entity extensions
$OPENSSL x509 -req -in req.pem -CA smroot.pem -days 3600 \
	-extfile ca.cnf -extensions usr_cert -CAcreateserial >>smrsa1.pem

CN="Test S/MIME EE RSA #2" $OPENSSL req -config ca.cnf -nodes \
	-keyout smrsa2.pem -out req.pem -newkey rsa:2048
$OPENSSL x509 -req -in req.pem -CA smroot.pem -days 3600 \
	-extfile ca.cnf -extensions usr_cert -CAcreateserial >>smrsa2.pem

CN="Test S/MIME EE RSA #3" $OPENSSL req -config ca.cnf -nodes \
	-keyout smrsa3.pem -out req.pem -newkey rsa:2048
$OPENSSL x509 -req -in req.pem -CA smroot.pem -days 3600 \
	-extfile ca.cnf -extensions usr_cert -CAcreateserial >>smrsa3.pem

# Create DSA parameters

$OPENSSL dsaparam -out dsap.pem 2048

CN="Test S/MIME EE DSA #1" $OPENSSL req -config ca.cnf -nodes \
	-keyout smdsa1.pem -out req.pem -newkey dsa:dsap.pem
$OPENSSL x509 -req -in req.pem -CA smroot.pem -days 3600 \
	-extfile ca.cnf -extensions usr_cert -CAcreateserial >>smdsa1.pem
CN="Test S/MIME EE DSA #2" $OPENSSL req -config ca.cnf -nodes \
	-keyout smdsa2.pem -out req.pem -newkey dsa:dsap.pem
$OPENSSL x509 -req -in req.pem -CA smroot.pem -days 3600 \
	-extfile ca.cnf -extensions usr_cert -CAcreateserial >>smdsa2.pem
CN="Test S/MIME EE DSA #3" $OPENSSL req -config ca.cnf -nodes \
	-keyout smdsa3.pem -out req.pem -newkey dsa:dsap.pem
$OPENSSL x509 -req -in req.pem -CA smroot.pem -days 3600 \
	-extfile ca.cnf -extensions usr_cert -CAcreateserial >>smdsa3.pem

# Create EC parameters

$OPENSSL ecparam -out ecp.pem -name P-256
$OPENSSL ecparam -out ecp2.pem -name K-283

CN="Test S/MIME EE EC #1" $OPENSSL req -config ca.cnf -nodes \
	-keyout smec1.pem -out req.pem -newkey ec:ecp.pem
$OPENSSL x509 -req -in req.pem -CA smroot.pem -days 3600 \
	-extfile ca.cnf -extensions usr_cert -CAcreateserial >>smec1.pem
CN="Test S/MIME EE EC #2" $OPENSSL req -config ca.cnf -nodes \
	-keyout smec2.pem -out req.pem -newkey ec:ecp2.pem
$OPENSSL x509 -req -in req.pem -CA smroot.pem -days 3600 \
	-extfile ca.cnf -extensions usr_cert -CAcreateserial >>smec2.pem
CN="Test S/MIME EE EC #3" $OPENSSL req -config ca.cnf -nodes \
	-keyout smec3.pem -out req.pem -newkey ec:ecp.pem
$OPENSSL x509 -req -in req.pem -CA smroot.pem -days 3600 \
	-extfile ca.cnf -extensions usr_cert -CAcreateserial >>smec3.pem
# Create X9.42 DH parameters.
$OPENSSL genpkey -genparam -algorithm DH -pkeyopt dh_paramgen_type:2 \
	-out dhp.pem
# Generate X9.42 DH key.
$OPENSSL genpkey -paramfile dhp.pem -out smdh.pem
$OPENSSL pkey -pubout -in smdh.pem -out dhpub.pem
# Generate dummy request.
CN="Test S/MIME EE DH #1" $OPENSSL req -config ca.cnf -nodes \
	-keyout smtmp.pem -out req.pem -newkey rsa:2048
# Sign request but force public key to DH
$OPENSSL x509 -req -in req.pem -CA smroot.pem -days 3600 \
	-force_pubkey dhpub.pem \
	-extfile ca.cnf -extensions usr_cert -CAcreateserial >>smdh.pem
# Remove temp files.
rm -f req.pem ecp.pem ecp2.pem dsap.pem dhp.pem dhpub.pem smtmp.pem smroot.srl
