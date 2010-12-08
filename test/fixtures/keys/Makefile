all: agent1-cert.pem agent2-cert.pem agent3-cert.pem


#
# Create Certificate Authority: ca1
# ('password' is used for the CA password.)
#
ca1-cert.pem: ca1.cnf
	openssl req -new -x509 -config ca1.cnf -keyout ca1-key.pem -out ca1-cert.pem

#
# Create Certificate Authority: ca2
# ('password' is used for the CA password.)
#
ca2-cert.pem: ca2.cnf
	openssl req -new -x509 -config ca2.cnf -keyout ca2-key.pem -out ca2-cert.pem


#
# agent1 is signed by ca1.
#

agent1-key.pem:
	openssl genrsa -out agent1-key.pem

agent1-csr.pem: agent1.cnf agent1-key.pem
	openssl req -new -config agent1.cnf -key agent1-key.pem -out agent1-csr.pem

agent1-cert.pem: agent1-csr.pem ca1-cert.pem ca1-key.pem
	openssl x509 -req \
		-passin "pass:password" \
		-in agent1-csr.pem \
		-CA ca1-cert.pem \
		-CAkey ca1-key.pem \
		-CAcreateserial \
		-out agent1-cert.pem

agent1-verify: agent1-cert.pem ca1-cert.pem
	openssl verify -CAfile ca1-cert.pem agent1-cert.pem


#
# agent2 has a self signed cert
#
# Generate new private key
agent2-key.pem:
	openssl genrsa -out agent2-key.pem

# Create a Certificate Signing Request for the key
agent2-csr.pem: agent2-key.pem agent2.cnf
	openssl req -new -config agent2.cnf -key agent2-key.pem -out agent2-csr.pem

# Create a Certificate for the agent.
agent2-cert.pem: agent2-csr.pem agent2-key.pem
	openssl x509 -req \
		-in agent2-csr.pem \
		-signkey agent2-key.pem \
		-out agent2-cert.pem

agent2-verify: agent2-cert.pem
	openssl verify -CAfile agent2-cert.pem agent2-cert.pem

#
# agent3 is signed by ca2.
#

agent3-key.pem:
	openssl genrsa -out agent3-key.pem

agent3-csr.pem: agent3.cnf agent3-key.pem
	openssl req -new -config agent3.cnf -key agent3-key.pem -out agent3-csr.pem

agent3-cert.pem: agent3-csr.pem ca2-cert.pem ca2-key.pem
	openssl x509 -req \
		-passin "pass:password" \
		-in agent3-csr.pem \
		-CA ca2-cert.pem \
		-CAkey ca2-key.pem \
		-CAcreateserial \
		-out agent3-cert.pem

agent3-verify: agent3-cert.pem ca2-cert.pem
	openssl verify -CAfile ca2-cert.pem agent3-cert.pem

# TODO: agent on CRL


clean:
	rm -f *.pem *.srl

test: agent1-verify agent2-verify agent3-verify


.PHONY: all clean test agent1-verify agent2-verify agent3-verify
