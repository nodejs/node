extern int int_rsa_verify(int dtype, const unsigned char *m, unsigned int m_len,
		unsigned char *rm, size_t *prm_len,
		const unsigned char *sigbuf, size_t siglen,
		RSA *rsa);
