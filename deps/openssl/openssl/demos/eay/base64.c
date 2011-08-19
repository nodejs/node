/* This is a simple example of using the base64 BIO to a memory BIO and then
 * getting the data.
 */
#include <stdio.h>
#include <openssl/bio.h>
#include <openssl/evp.h>

main()
	{
	int i;
	BIO *mbio,*b64bio,*bio;
	char buf[512];
	char *p;

	mbio=BIO_new(BIO_s_mem());
	b64bio=BIO_new(BIO_f_base64());

	bio=BIO_push(b64bio,mbio);
	/* We now have bio pointing at b64->mem, the base64 bio encodes on
	 * write and decodes on read */

	for (;;)
		{
		i=fread(buf,1,512,stdin);
		if (i <= 0) break;
		BIO_write(bio,buf,i);
		}
	/* We need to 'flush' things to push out the encoding of the
	 * last few bytes.  There is special encoding if it is not a
	 * multiple of 3
	 */
	BIO_flush(bio);

	printf("We have %d bytes available\n",BIO_pending(mbio));

	/* We will now get a pointer to the data and the number of elements. */
	/* hmm... this one was not defined by a macro in bio.h, it will be for
	 * 0.9.1.  The other option is too just read from the memory bio.
	 */
	i=(int)BIO_ctrl(mbio,BIO_CTRL_INFO,0,(char *)&p);

	printf("%d\n",i);
	fwrite("---\n",1,4,stdout);
	fwrite(p,1,i,stdout);
	fwrite("---\n",1,4,stdout);

	/* This call will walk the chain freeing all the BIOs */
	BIO_free_all(bio);
	}
