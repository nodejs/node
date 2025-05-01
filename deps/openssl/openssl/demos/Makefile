MODULES = bio \
          cipher \
          cms \
          digest \
          encode \
          encrypt \
          guide \
          http3 \
          kdf \
          keyexch \
          mac \
          pkey \
          signature \
          smime \
          sslecho

all:
	@set -e; for i in $(MODULES); do \
		${MAKE} -C $$i all; \
	done
clean:
	@set -e; for i in $(MODULES); do \
		${MAKE} -C $$i clean; \
	done
test:
	@set -e; for i in $(MODULES); do \
		${MAKE} -C $$i test; \
	done
