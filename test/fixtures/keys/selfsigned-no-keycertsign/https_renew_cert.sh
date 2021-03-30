#!/bin/bash
openssl genrsa -out rsa.pem 2048
openssl rsa -in rsa.pem -out key.pem
openssl req -sha256 -new -key key.pem -out csr.pem -subj "/CN=localhost"
openssl x509 -req -extfile cert.conf -extensions v3_req -days 365 -in csr.pem -signkey key.pem -out cert.pem

