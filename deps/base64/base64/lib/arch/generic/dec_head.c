int ret = 0;
const uint8_t *s = (const uint8_t *) src;
uint8_t *o = (uint8_t *) out;
uint8_t q;

// Use local temporaries to avoid cache thrashing:
size_t olen = 0;
size_t slen = srclen;
struct base64_state st;
st.eof = state->eof;
st.bytes = state->bytes;
st.carry = state->carry;

// If we previously saw an EOF or an invalid character, bail out:
if (st.eof) {
	*outlen = 0;
	ret = 0;
	// If there was a trailing '=' to check, check it:
	if (slen && (st.eof == BASE64_AEOF)) {
		state->bytes = 0;
		state->eof = BASE64_EOF;
		ret = ((base64_table_dec_8bit[*s++] == 254) && (slen == 1)) ? 1 : 0;
	}
	return ret;
}

// Turn four 6-bit numbers into three bytes:
// out[0] = 11111122
// out[1] = 22223333
// out[2] = 33444444

// Duff's device again:
switch (st.bytes)
{
	for (;;)
	{
	case 0:
