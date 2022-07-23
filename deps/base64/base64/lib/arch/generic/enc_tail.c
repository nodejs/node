		if (slen-- == 0) {
			break;
		}
		*o++ = base64_table_enc_6bit[*s >> 2];
		st.carry = (*s++ << 4) & 0x30;
		st.bytes++;
		olen += 1;

		// Deliberate fallthrough:
		BASE64_FALLTHROUGH

	case 1:	if (slen-- == 0) {
			break;
		}
		*o++ = base64_table_enc_6bit[st.carry | (*s >> 4)];
		st.carry = (*s++ << 2) & 0x3C;
		st.bytes++;
		olen += 1;

		// Deliberate fallthrough:
		BASE64_FALLTHROUGH

	case 2:	if (slen-- == 0) {
			break;
		}
		*o++ = base64_table_enc_6bit[st.carry | (*s >> 6)];
		*o++ = base64_table_enc_6bit[*s++ & 0x3F];
		st.bytes = 0;
		olen += 2;
	}
}
state->bytes = st.bytes;
state->carry = st.carry;
*outlen = olen;
