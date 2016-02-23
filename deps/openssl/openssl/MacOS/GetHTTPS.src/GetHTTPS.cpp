/*
 *	An demo illustrating how to retrieve a URI from a secure HTTP server.
 *
 *	Author: 	Roy Wood
 *	Date:		September 7, 1999
 *	Comments:	This relies heavily on my MacSockets library.
 *				This project is also set up so that it expects the OpenSSL source folder (0.9.4 as I write this)
 *				to live in a folder called "OpenSSL-0.9.4" in this project's parent folder.  For example:
 *
 *					Macintosh HD:
 *						Development:
 *							OpenSSL-0.9.4:
 *								(OpenSSL sources here)
 *							OpenSSL Example:
 *								(OpenSSL example junk here)
 *
 *
 *				Also-- before attempting to compile this, make sure the aliases in "OpenSSL-0.9.4:include:openssl" 
 *				are installed!  Use the AppleScript applet in the "openssl-0.9.4" folder to do this!
 */
/* modified to seed the PRNG */
/* modified to use CRandomizer for seeding */


//	Include some funky libs I've developed over time

#include "CPStringUtils.hpp"
#include "ErrorHandling.hpp"
#include "MacSocket.h"
#include "Randomizer.h"

//	We use the OpenSSL implementation of SSL....
//	This was a lot of work to finally get going, though you wouldn't know it by the results!

#include <openssl/ssl.h>
#include <openssl/err.h>

#include <timer.h>

//	Let's try grabbing some data from here:

#define kHTTPS_DNS		"www.apache-ssl.org"
#define kHTTPS_Port		443
#define kHTTPS_URI		"/"


//	Forward-declare this

OSErr MyMacSocket_IdleWaitCallback(void *inUserRefPtr);

//	My idle-wait callback.  Doesn't do much, does it?  Silly cooperative multitasking.

OSErr MyMacSocket_IdleWaitCallback(void *inUserRefPtr)
{
#pragma unused(inUserRefPtr)

EventRecord		theEvent;
	::EventAvail(everyEvent,&theEvent);
	
	CRandomizer *randomizer = (CRandomizer*)inUserRefPtr;
	if (randomizer)
		randomizer->PeriodicAction();

	return(noErr);
}


//	Finally!

void main(void)
{
	OSErr				errCode;
	int					theSocket = -1;
	int					theTimeout = 30;

	SSL_CTX				*ssl_ctx = nil;
	SSL					*ssl = nil;

	char				tempString[256];
	UnsignedWide		microTickCount;


	CRandomizer randomizer;
	
	printf("OpenSSL Demo by Roy Wood, roy@centricsystems.ca\n\n");
	
	BailIfError(errCode = MacSocket_Startup());



	//	Create a socket-like object
	
	BailIfError(errCode = MacSocket_socket(&theSocket,false,theTimeout * 60,MyMacSocket_IdleWaitCallback,&randomizer));

	
	//	Set up the connect string and try to connect
	
	CopyCStrAndInsertCStrLongIntIntoCStr("%s:%ld",kHTTPS_DNS,kHTTPS_Port,tempString,sizeof(tempString));
	
	printf("Connecting to %s....\n",tempString);

	BailIfError(errCode = MacSocket_connect(theSocket,tempString));
	
	
	//	Init SSL stuff
	
	SSL_load_error_strings();
	
	SSLeay_add_ssl_algorithms();
	
	
	//	Pick the SSL method
	
//	ssl_ctx = SSL_CTX_new(SSLv2_client_method());
	ssl_ctx = SSL_CTX_new(SSLv23_client_method());
//	ssl_ctx = SSL_CTX_new(SSLv3_client_method());
			

	//	Create an SSL thingey and try to negotiate the connection
	
	ssl = SSL_new(ssl_ctx);
	
	SSL_set_fd(ssl,theSocket);
	
	errCode = SSL_connect(ssl);
	
	if (errCode < 0)
	{
		SetErrorMessageAndLongIntAndBail("OpenSSL: Can't initiate SSL connection, SSL_connect() = ",errCode);
	}
	
	//	Request the URI from the host
	
	CopyCStrToCStr("GET ",tempString,sizeof(tempString));
	ConcatCStrToCStr(kHTTPS_URI,tempString,sizeof(tempString));
	ConcatCStrToCStr(" HTTP/1.0\r\n\r\n",tempString,sizeof(tempString));

	
	errCode = SSL_write(ssl,tempString,CStrLength(tempString));
	
	if (errCode < 0)
	{
		SetErrorMessageAndLongIntAndBail("OpenSSL: Error writing data via ssl, SSL_write() = ",errCode);
	}
	

	for (;;)
	{
	char	tempString[256];
	int		bytesRead;
		

		//	Read some bytes and dump them to the console
		
		bytesRead = SSL_read(ssl,tempString,sizeof(tempString) - 1);
		
		if (bytesRead == 0 && MacSocket_RemoteEndIsClosing(theSocket))
		{
			break;
		}
		
		else if (bytesRead < 0)
		{
			SetErrorMessageAndLongIntAndBail("OpenSSL: Error reading data via ssl, SSL_read() = ",bytesRead);
		}
		
		
		tempString[bytesRead] = '\0';
		
		printf("%s", tempString);
	}
	
	printf("\n\n\n");
	
	//	All done!
	
	errCode = noErr;
	
	
EXITPOINT:

	//	Clean up and go home
	
	if (theSocket >= 0)
	{
		MacSocket_close(theSocket);
	}
	
	if (ssl != nil)
	{
		SSL_free(ssl);
	}
	
	if (ssl_ctx != nil)
	{
		SSL_CTX_free(ssl_ctx);
	}
	
	
	if (errCode != noErr)
	{
		printf("An error occurred:\n");
		
		printf("%s",GetErrorMessage());
	}
	
	
	MacSocket_Shutdown();
}
