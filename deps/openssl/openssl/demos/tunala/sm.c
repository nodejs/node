#include "tunala.h"

#ifndef NO_TUNALA

void state_machine_init(state_machine_t *machine)
{
	machine->ssl = NULL;
	machine->bio_intossl = machine->bio_fromssl = NULL;
	buffer_init(&machine->clean_in);
	buffer_init(&machine->clean_out);
	buffer_init(&machine->dirty_in);
	buffer_init(&machine->dirty_out);
}

void state_machine_close(state_machine_t *machine)
{
	if(machine->ssl)
		SSL_free(machine->ssl);
/* SSL_free seems to decrement the reference counts already so doing this goes
 * kaboom. */
#if 0
	if(machine->bio_intossl)
		BIO_free(machine->bio_intossl);
	if(machine->bio_fromssl)
		BIO_free(machine->bio_fromssl);
#endif
	buffer_close(&machine->clean_in);
	buffer_close(&machine->clean_out);
	buffer_close(&machine->dirty_in);
	buffer_close(&machine->dirty_out);
	state_machine_init(machine);
}

buffer_t *state_machine_get_buffer(state_machine_t *machine, sm_buffer_t type)
{
	switch(type) {
	case SM_CLEAN_IN:
		return &machine->clean_in;
	case SM_CLEAN_OUT:
		return &machine->clean_out;
	case SM_DIRTY_IN:
		return &machine->dirty_in;
	case SM_DIRTY_OUT:
		return &machine->dirty_out;
	default:
		break;
	}
	/* Should never get here */
	abort();
	return NULL;
}

SSL *state_machine_get_SSL(state_machine_t *machine)
{
	return machine->ssl;
}

int state_machine_set_SSL(state_machine_t *machine, SSL *ssl, int is_server)
{
	if(machine->ssl)
		/* Shouldn't ever be set twice */
		abort();
	machine->ssl = ssl;
	/* Create the BIOs to handle the dirty side of the SSL */
	if((machine->bio_intossl = BIO_new(BIO_s_mem())) == NULL)
		abort();
	if((machine->bio_fromssl = BIO_new(BIO_s_mem())) == NULL)
		abort();
	/* Hook up the BIOs on the dirty side of the SSL */
	SSL_set_bio(machine->ssl, machine->bio_intossl, machine->bio_fromssl);
	if(is_server)
		SSL_set_accept_state(machine->ssl);
	else
		SSL_set_connect_state(machine->ssl);
	/* If we're the first one to generate traffic - do it now otherwise we
	 * go into the next select empty-handed and our peer will not send data
	 * but will similarly wait for us. */
	return state_machine_churn(machine);
}

/* Performs the data-IO loop and returns zero if the machine should close */
int state_machine_churn(state_machine_t *machine)
{
	unsigned int loop;
	if(machine->ssl == NULL) {
		if(buffer_empty(&machine->clean_out))
			/* Time to close this state-machine altogether */
			return 0;
		else
			/* Still buffered data on the clean side to go out */
			return 1;
	}
	/* Do this loop twice to cover any dependencies about which precise
	 * order of reads and writes is required. */
	for(loop = 0; loop < 2; loop++) {
		buffer_to_SSL(&machine->clean_in, machine->ssl);
		buffer_to_BIO(&machine->dirty_in, machine->bio_intossl);
		buffer_from_SSL(&machine->clean_out, machine->ssl);
		buffer_from_BIO(&machine->dirty_out, machine->bio_fromssl);
	}
	/* We close on the SSL side if the info callback noticed some problems
	 * or an SSL shutdown was underway and shutdown traffic had all been
	 * sent. */
	if(SSL_get_app_data(machine->ssl) || (SSL_get_shutdown(machine->ssl) &&
				buffer_empty(&machine->dirty_out))) {
		/* Great, we can seal off the dirty side completely */
		if(!state_machine_close_dirty(machine))
			return 0;
	}
	/* Either the SSL is alive and well, or the closing process still has
	 * outgoing data waiting to be sent */
	return 1;
}

/* Called when the clean side of the SSL has lost its connection */
int state_machine_close_clean(state_machine_t *machine)
{
	/* Well, first thing to do is null out the clean-side buffers - they're
	 * no use any more. */
	buffer_close(&machine->clean_in);
	buffer_close(&machine->clean_out);
	/* And start an SSL shutdown */
	if(machine->ssl)
		SSL_shutdown(machine->ssl);
	/* This is an "event", so flush the SSL of any generated traffic */
	state_machine_churn(machine);
	if(buffer_empty(&machine->dirty_in) &&
			buffer_empty(&machine->dirty_out))
		return 0;
	return 1;
}

/* Called when the dirty side of the SSL has lost its connection. This is pretty
 * terminal as all that can be left to do is send any buffered output on the
 * clean side - after that, we're done. */
int state_machine_close_dirty(state_machine_t *machine)
{
	buffer_close(&machine->dirty_in);
	buffer_close(&machine->dirty_out);
	buffer_close(&machine->clean_in);
	if(machine->ssl)
		SSL_free(machine->ssl);
	machine->ssl = NULL;
	machine->bio_intossl = machine->bio_fromssl = NULL;
	if(buffer_empty(&machine->clean_out))
		return 0;
	return 1;
}

#endif /* !defined(NO_TUNALA) */

