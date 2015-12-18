all: agent1-cert.pem agent2-cert.pem agent3-cert.pem agent4-cert.pem agent5-cert.pem ca2-crl.pem ec-cert.pem dh512.pem dh1024.pem dh2048.pem dsa1025.pem dsa_private_1025.pem dsa_public_1025.pem rsa_private_1024.pem rsa_private_2048.pem rsa_private_4096.pem rsa_public_1024.pem rsa_public_2048.pem rsa_public_4096.pem


#
# Create Certificate Authority: ca1
# ('password' is used for the CA password.)
#
ca1-cert.pem: ca1.cnf
	openssl req -new -x509 -days 9999 -config ca1.cnf -keyout ca1-key.pem -out ca1-cert.pem

#
# Create Certificate Authority: ca2
# ('password' is used for the CA password.)
#
ca2-cert.pem: ca2.cnf
	openssl req -new -x509 -days 9999 -config ca2.cnf -keyout ca2-key.pem -out ca2-cert.pem
	echo '01' > ca2-serial
	touch ca2-database.txt

#
# Create Subordinate Certificate Authority: ca3
# ('password' is used for the CA password.)
#
ca3-key.pem:
	openssl genrsa -out ca3-key.pem 1024

ca3-csr.pem: ca3.cnf ca3-key.pem
	openssl req -new \
		-extensions v3_ca \
		-config ca3.cnf \
		-key ca3-key.pem \
		-out ca3-csr.pem

ca3-cert.pem: ca3-csr.pem ca3-key.pem ca3.cnf ca1-cert.pem ca1-key.pem
	openssl x509 -req \
		-extfile ca3.cnf \
		-extensions v3_ca \
		-days 9999 \
		-passin "pass:password" \
		-in ca3-csr.pem \
		-CA ca1-cert.pem \
		-CAkey ca1-key.pem \
		-CAcreateserial \
		-out ca3-cert.pem

#
# Create Fake CNNIC Root Certificate Authority: fake-cnnic-root
#

fake-cnnic-root-key.pem:
	openssl genrsa -out fake-cnnic-root-key.pem 2048

fake-cnnic-root-cert.pem: fake-cnnic-root.cnf fake-cnnic-root-key.pem
	openssl req -x509 -new \
	        -key fake-cnnic-root-key.pem \
	        -days 1024 \
	        -out fake-cnnic-root-cert.pem \
	        -config fake-cnnic-root.cnf

#
# agent1 is signed by ca1.
#

agent1-key.pem:
	openssl genrsa -out agent1-key.pem 1024

agent1-csr.pem: agent1.cnf agent1-key.pem
	openssl req -new -config agent1.cnf -key agent1-key.pem -out agent1-csr.pem

agent1-cert.pem: agent1-csr.pem ca1-cert.pem ca1-key.pem
	openssl x509 -req \
		-extfile agent1.cnf \
		-extensions v3_ca \
		-days 9999 \
		-passin "pass:password" \
		-in agent1-csr.pem \
		-CA ca1-cert.pem \
		-CAkey ca1-key.pem \
		-CAcreateserial \
		-out agent1-cert.pem

agent1-pfx.pem: agent1-cert.pem agent1-key.pem ca1-cert.pem
	openssl pkcs12 -export \
		-in agent1-cert.pem \
		-inkey agent1-key.pem \
		-certfile ca1-cert.pem \
		-out agent1-pfx.pem \
		-password pass:sample

agent1-verify: agent1-cert.pem ca1-cert.pem
	openssl verify -CAfile ca1-cert.pem agent1-cert.pem


#
# agent2 has a self signed cert
#
# Generate new private key
agent2-key.pem:
	openssl genrsa -out agent2-key.pem 1024

# Create a Certificate Signing Request for the key
agent2-csr.pem: agent2-key.pem agent2.cnf
	openssl req -new -config agent2.cnf -key agent2-key.pem -out agent2-csr.pem

# Create a Certificate for the agent.
agent2-cert.pem: agent2-csr.pem agent2-key.pem
	openssl x509 -req \
		-days 9999 \
		-in agent2-csr.pem \
		-signkey agent2-key.pem \
		-out agent2-cert.pem

agent2-verify: agent2-cert.pem
	openssl verify -CAfile agent2-cert.pem agent2-cert.pem

#
# agent3 is signed by ca2.
#

agent3-key.pem:
	openssl genrsa -out agent3-key.pem 1024

agent3-csr.pem: agent3.cnf agent3-key.pem
	openssl req -new -config agent3.cnf -key agent3-key.pem -out agent3-csr.pem

agent3-cert.pem: agent3-csr.pem ca2-cert.pem ca2-key.pem
	openssl x509 -req \
		-days 9999 \
		-passin "pass:password" \
		-in agent3-csr.pem \
		-CA ca2-cert.pem \
		-CAkey ca2-key.pem \
		-CAcreateserial \
		-out agent3-cert.pem

agent3-verify: agent3-cert.pem ca2-cert.pem
	openssl verify -CAfile ca2-cert.pem agent3-cert.pem


#
# agent4 is signed by ca2 (client cert)
#

agent4-key.pem:
	openssl genrsa -out agent4-key.pem 1024

agent4-csr.pem: agent4.cnf agent4-key.pem
	openssl req -new -config agent4.cnf -key agent4-key.pem -out agent4-csr.pem

agent4-cert.pem: agent4-csr.pem ca2-cert.pem ca2-key.pem
	openssl x509 -req \
		-days 9999 \
		-passin "pass:password" \
		-in agent4-csr.pem \
		-CA ca2-cert.pem \
		-CAkey ca2-key.pem \
		-CAcreateserial \
		-extfile agent4.cnf \
		-extensions ext_key_usage \
		-out agent4-cert.pem

agent4-verify: agent4-cert.pem ca2-cert.pem
	openssl verify -CAfile ca2-cert.pem agent4-cert.pem

#
# Make CRL with agent4 being rejected
#
ca2-crl.pem: ca2-key.pem ca2-cert.pem ca2.cnf
	openssl ca -revoke agent4-cert.pem \
		-keyfile ca2-key.pem \
		-cert ca2-cert.pem \
		-config ca2.cnf \
		-passin 'pass:password'
	openssl ca \
		-keyfile ca2-key.pem \
		-cert ca2-cert.pem \
		-config ca2.cnf \
		-gencrl \
		-out ca2-crl.pem \
		-passin 'pass:password'

#
# agent5 is signed by ca2 (client cert)
#

agent5-key.pem:
	openssl genrsa -out agent5-key.pem 1024

agent5-csr.pem: agent5.cnf agent5-key.pem
	openssl req -new -config agent5.cnf -key agent5-key.pem -out agent5-csr.pem

agent5-cert.pem: agent5-csr.pem ca2-cert.pem ca2-key.pem
	openssl x509 -req \
		-days 9999 \
		-passin "pass:password" \
		-in agent5-csr.pem \
		-CA ca2-cert.pem \
		-CAkey ca2-key.pem \
		-CAcreateserial \
		-extfile agent5.cnf \
		-extensions ext_key_usage \
		-out agent5-cert.pem

agent5-verify: agent5-cert.pem ca2-cert.pem
	openssl verify -CAfile ca2-cert.pem agent5-cert.pem

#
# agent6 is signed by ca3
#

agent6-key.pem:
	openssl genrsa -out agent6-key.pem 1024

agent6-csr.pem: agent6.cnf agent6-key.pem
	openssl req -new -config agent6.cnf -key agent6-key.pem -out agent6-csr.pem

agent6-cert.pem: agent6-csr.pem ca3-cert.pem ca3-key.pem
	openssl x509 -req \
		-days 9999 \
		-passin "pass:password" \
		-in agent6-csr.pem \
		-CA ca3-cert.pem \
		-CAkey ca3-key.pem \
		-CAcreateserial \
		-extfile agent6.cnf \
		-out agent6-cert.pem
	cat ca3-cert.pem >> agent6-cert.pem

agent6-verify: agent6-cert.pem ca3-cert.pem
	openssl verify -CAfile ca3-cert.pem agent6-cert.pem

#
# agent7 is signed by fake-cnnic-root.
#

agent7-key.pem:
	openssl genrsa -out agent7-key.pem 2048

agent7-csr.pem: agent1.cnf agent7-key.pem
	openssl req -new -config agent7.cnf -key agent7-key.pem -out agent7-csr.pem

agent7-cert.pem: agent7-csr.pem fake-cnnic-root-cert.pem fake-cnnic-root-key.pem
	openssl x509 -req \
		-extfile agent7.cnf \
		-days 9999 \
		-passin "pass:password" \
		-in agent7-csr.pem \
		-CA fake-cnnic-root-cert.pem \
		-CAkey fake-cnnic-root-key.pem \
		-CAcreateserial \
		-out agent7-cert.pem

agent7-verify: agent7-cert.pem fake-cnnic-root-cert.pem
	openssl verify -CAfile fake-cnnic-root-cert.pem agent7-cert.pem

ec-key.pem:
	openssl ecparam -genkey -out ec-key.pem -name prime256v1

ec-csr.pem: ec-key.pem
	openssl req -new -config ec.cnf -key ec-key.pem -out ec-csr.pem

ec-cert.pem: ec-csr.pem ec-key.pem
	openssl x509 -req \
		-days 9999 \
		-in ec-csr.pem \
		-signkey ec-key.pem \
		-out ec-cert.pem

dh512.pem:
	openssl dhparam -out dh512.pem 512

dh1024.pem:
	openssl dhparam -out dh1024.pem 1024

dh2048.pem:
	openssl dhparam -out dh2048.pem 2048

dsa1025.pem:
	openssl dsaparam -out dsa1025.pem 1025

dsa_private_1025.pem:
	openssl gendsa -out dsa_private_1025.pem dsa1025.pem

dsa_public_1025.pem:
	openssl dsa -in dsa_private_1025.pem -pubout -out dsa_public_1025.pem

rsa_private_1024.pem:
	openssl genrsa -out rsa_private_1024.pem 1024

rsa_private_2048.pem:
	openssl genrsa -out rsa_private_2048.pem 2048

rsa_private_4096.pem:
	openssl genrsa -out rsa_private_4096.pem 4096

rsa_public_1024.pem: rsa_private_1024.pem
	openssl rsa -in rsa_private_1024.pem -out rsa_public_1024.pem

rsa_public_2048.pem: rsa_private_2048.pem
	openssl rsa -in rsa_private_2048.pem -out rsa_public_2048.pem

rsa_public_4096.pem: rsa_private_4096.pem
	openssl rsa -in rsa_private_4096.pem -out rsa_public_4096.pem

clean:
	rm -f *.pem *.srl ca2-database.txt ca2-serial

test: agent1-verify agent2-verify agent3-verify agent4-verify agent5-verify


.PHONY: all clean test agent1-verify agent2-verify agent3-verify agent4-verify agent5-verify
