#include "tunala.h"

#ifndef NO_BUFFER

void buffer_init(buffer_t *buf)
{
	buf->used = 0;
	buf->total_in = buf->total_out = 0;
}

void buffer_close(buffer_t *buf)
{
	/* Our data is static - nothing needs "release", just reset it */
	buf->used = 0;
}

/* Code these simple ones in compact form */
unsigned int buffer_used(buffer_t *buf) {
	return buf->used; }
unsigned int buffer_unused(buffer_t *buf) {
	return (MAX_DATA_SIZE - buf->used); }
int buffer_full(buffer_t *buf) {
	return (buf->used == MAX_DATA_SIZE ? 1 : 0); }
int buffer_notfull(buffer_t *buf) {
	return (buf->used < MAX_DATA_SIZE ? 1 : 0); }
int buffer_empty(buffer_t *buf) {
	return (buf->used == 0 ? 1 : 0); }
int buffer_notempty(buffer_t *buf) {
	return (buf->used > 0 ? 1 : 0); }
unsigned long buffer_total_in(buffer_t *buf) {
	return buf->total_in; }
unsigned long buffer_total_out(buffer_t *buf) {
	return buf->total_out; }

/* These 3 static (internal) functions don't adjust the "total" variables as
 * it's not sure when they're called how it should be interpreted. Only the
 * higher-level "buffer_[to|from]_[fd|SSL|BIO]" functions should alter these
 * values. */
#if 0 /* To avoid "unused" warnings */
static unsigned int buffer_adddata(buffer_t *buf, const unsigned char *ptr,
		unsigned int size)
{
	unsigned int added = MAX_DATA_SIZE - buf->used;
	if(added > size)
		added = size;
	if(added == 0)
		return 0;
	memcpy(buf->data + buf->used, ptr, added);
	buf->used += added;
	buf->total_in += added;
	return added;
}

static unsigned int buffer_tobuffer(buffer_t *to, buffer_t *from, int cap)
{
	unsigned int moved, tomove = from->used;
	if((int)tomove > cap)
		tomove = cap;
	if(tomove == 0)
		return 0;
	moved = buffer_adddata(to, from->data, tomove);
	if(moved == 0)
		return 0;
	buffer_takedata(from, NULL, moved);
	return moved;
}
#endif

static unsigned int buffer_takedata(buffer_t *buf, unsigned char *ptr,
		unsigned int size)
{
	unsigned int taken = buf->used;
	if(taken > size)
		taken = size;
	if(taken == 0)
		return 0;
	if(ptr)
		memcpy(ptr, buf->data, taken);
	buf->used -= taken;
	/* Do we have to scroll? */
	if(buf->used > 0)
		memmove(buf->data, buf->data + taken, buf->used);
	return taken;
}

#ifndef NO_IP

int buffer_from_fd(buffer_t *buf, int fd)
{
	int toread = buffer_unused(buf);
	if(toread == 0)
		/* Shouldn't be called in this case! */
		abort();
	toread = read(fd, buf->data + buf->used, toread);
	if(toread > 0) {
		buf->used += toread;
		buf->total_in += toread;
	}
	return toread;
}

int buffer_to_fd(buffer_t *buf, int fd)
{
	int towrite = buffer_used(buf);
	if(towrite == 0)
		/* Shouldn't be called in this case! */
		abort();
	towrite = write(fd, buf->data, towrite);
	if(towrite > 0) {
		buffer_takedata(buf, NULL, towrite);
		buf->total_out += towrite;
	}
	return towrite;
}

#endif /* !defined(NO_IP) */

#ifndef NO_OPENSSL

static void int_ssl_check(SSL *s, int ret)
{
	int e = SSL_get_error(s, ret);
	switch(e) {
		/* These seem to be harmless and already "dealt with" by our
		 * non-blocking environment. NB: "ZERO_RETURN" is the clean
		 * "error" indicating a successfully closed SSL tunnel. We let
		 * this happen because our IO loop should not appear to have
		 * broken on this condition - and outside the IO loop, the
		 * "shutdown" state is checked. */
	case SSL_ERROR_NONE:
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
	case SSL_ERROR_WANT_X509_LOOKUP:
	case SSL_ERROR_ZERO_RETURN:
		return;
		/* These seem to be indications of a genuine error that should
		 * result in the SSL tunnel being regarded as "dead". */
	case SSL_ERROR_SYSCALL:
	case SSL_ERROR_SSL:
		SSL_set_app_data(s, (char *)1);
		return;
	default:
		break;
	}
	/* For any other errors that (a) exist, and (b) crop up - we need to
	 * interpret what to do with them - so "politely inform" the caller that
	 * the code needs updating here. */
	abort();
}

void buffer_from_SSL(buffer_t *buf, SSL *ssl)
{
	int ret;
	if(!ssl || buffer_full(buf))
		return;
	ret = SSL_read(ssl, buf->data + buf->used, buffer_unused(buf));
	if(ret > 0) {
		buf->used += ret;
		buf->total_in += ret;
	}
	if(ret < 0)
		int_ssl_check(ssl, ret);
}

void buffer_to_SSL(buffer_t *buf, SSL *ssl)
{
	int ret;
	if(!ssl || buffer_empty(buf))
		return;
	ret = SSL_write(ssl, buf->data, buf->used);
	if(ret > 0) {
		buffer_takedata(buf, NULL, ret);
		buf->total_out += ret;
	}
	if(ret < 0)
		int_ssl_check(ssl, ret);
}

void buffer_from_BIO(buffer_t *buf, BIO *bio)
{
	int ret;
	if(!bio || buffer_full(buf))
		return;
	ret = BIO_read(bio, buf->data + buf->used, buffer_unused(buf));
	if(ret > 0) {
		buf->used += ret;
		buf->total_in += ret;
	}
}

void buffer_to_BIO(buffer_t *buf, BIO *bio)
{
	int ret;
	if(!bio || buffer_empty(buf))
		return;
	ret = BIO_write(bio, buf->data, buf->used);
	if(ret > 0) {
		buffer_takedata(buf, NULL, ret);
		buf->total_out += ret;
	}
}

#endif /* !defined(NO_OPENSSL) */

#endif /* !defined(NO_BUFFER) */
