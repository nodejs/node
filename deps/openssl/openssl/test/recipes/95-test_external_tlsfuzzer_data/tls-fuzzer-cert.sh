#!/bin/bash

tls_fuzzer_prepare() {

sed -e "s|@SERVER@|$SERV|g" -e "s/@PORT@/$PORT/g" -e "s/@PRIORITY@/$PRIORITY/g" ${TESTDATADIR}/cert.json.in >${TMPFILE}
}

. "${TESTDATADIR}/tlsfuzzer.sh"

