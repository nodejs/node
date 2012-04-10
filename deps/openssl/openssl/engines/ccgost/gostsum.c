/**********************************************************************
 *                        gostsum.c                                   *
 *             Copyright (c) 2005-2006 Cryptocom LTD                  *
 *         This file is distributed under the same license as OpenSSL *
 *                                                                    *
 *        Almost drop-in replacement for md5sum and sha1sum           *
 *          which computes GOST R 34.11-94 hashsum instead            *
 *                                                                    *
 **********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <string.h>
#include "gosthash.h"
#define BUF_SIZE 262144
int hash_file(gost_hash_ctx *ctx,char *filename,char *sum,int mode);
int hash_stream(gost_hash_ctx *ctx,int fd, char *sum);
int get_line(FILE *f,char *hash,char *filename);
void help()
	{
	fprintf(stderr,"gostsum [-bvt] [-c [file]]| [files]\n"
		"\t-c check message digests (default is generate)\n"
		"\t-v verbose, print file names when checking\n"
		"\t-b read files in binary mode\n"
		"\t-t use test GOST paramset (default is CryptoPro paramset)\n"
		"The input for -c should be the list of message digests and file names\n"
		"that is printed on stdout by this program when it generates digests.\n");
	exit(3);
	}

#ifndef O_BINARY
#define O_BINARY 0
#endif

int main(int argc,char **argv)
	{
	int c,i;
	int verbose=0;
	int errors=0;
	int open_mode = O_RDONLY;
	gost_subst_block *b=  &GostR3411_94_CryptoProParamSet;
	FILE *check_file = NULL;
	gost_hash_ctx ctx;
	
	while( (c=getopt(argc,argv,"bc::tv"))!=-1)
		{
		switch (c)
			{
			case 'v': verbose=1; break;
			case 't': b= &GostR3411_94_TestParamSet; break;
			case 'b': open_mode |= O_BINARY; break;
			case 'c':
				if (optarg)
					{
					check_file = fopen(optarg,"r");
					if (!check_file)
						{
						perror(optarg);
						exit(2);
						}
					}
				else
					{
				  	check_file= stdin;
					}
				break;
			default:
				fprintf(stderr,"invalid option %c",optopt);
				help();
			}
		}
	init_gost_hash_ctx(&ctx,b);
	if (check_file)
		{
		char inhash[65],calcsum[65],filename[PATH_MAX];
		int failcount=0,count=0;;
		if (check_file==stdin && optind<argc)
			{
			check_file=fopen(argv[optind],"r");
			if (!check_file)
				{	
				perror(argv[optind]);
				exit(2);
				}
			}	
		while (get_line(check_file,inhash,filename))
			{
			if (!hash_file(&ctx,filename,calcsum,open_mode))
				{
				exit (2);
				}	
			count++;
			if (!strncmp(calcsum,inhash,65))
				{
				if (verbose)
					{
					fprintf(stderr,"%s\tOK\n",filename);
					}
				}
			else
				{
				if (verbose)
					{
					fprintf(stderr,"%s\tFAILED\n",filename);
					}
				else
					{
					fprintf(stderr,"%s: GOST hash sum check failed for '%s'\n",
						argv[0],filename);
					}
				failcount++;
				}
			}	
		if (verbose && failcount)
			{
			fprintf(stderr,"%s: %d of %d file(f) failed GOST hash sum check\n",
				argv[0],failcount,count);
			}
		exit (failcount?1:0);
		}
	if (optind==argc)
		{
		char sum[65];
		if (!hash_stream(&ctx,fileno(stdin),sum))
			{
			perror("stdin");
			exit(1);
			}	
		printf("%s -\n",sum);
		exit(0);
		}	
	for (i=optind;i<argc;i++)
		{
		char sum[65];
		if (!hash_file(&ctx,argv[i],sum,open_mode))
			{
			errors++;
			}
		else
			{	
			printf("%s %s\n",sum,argv[i]);
			}
		}	
	exit(errors?1:0);	
	}

int hash_file(gost_hash_ctx *ctx,char *filename,char *sum,int mode)
	{
	int fd;
	if ((fd=open(filename,mode))<0)
		{
		perror(filename);
		return 0;
		}
	if (!hash_stream(ctx,fd,sum))
		{
		perror(filename);
		return 0;
		}	
	close(fd);
	return 1;
	}

int hash_stream(gost_hash_ctx *ctx,int fd, char *sum)
	{
	unsigned char buffer[BUF_SIZE];
	ssize_t bytes;
	int i;
	start_hash(ctx);
	while ((bytes=read(fd,buffer,BUF_SIZE))>0)
		{
		hash_block(ctx,buffer,bytes);
		}
	if (bytes<0)
		{
		return 0;
		}	
	finish_hash(ctx,buffer);
	for (i=0;i<32;i++)
		{
		sprintf(sum+2*i,"%02x",buffer[31-i]);
		}
	return 1;
	}	
	
int get_line(FILE *f,char *hash,char *filename)
	{
	int i;
	if (fread(hash,1,64,f)<64) return 0;
	hash[64]=0;
	for (i=0;i<64;i++)
		{
		if (hash[i]<'0' || (hash[i]>'9' && hash[i]<'A') || (hash[i]>'F'
				&& hash[i]<'a')||hash[i]>'f')
			{
			fprintf(stderr,"Not a hash value '%s'\n",hash);
			return 0;
			}
		}	
	if (fgetc(f)!=' ')
		{
		fprintf(stderr,"Malformed input line\n");
		return 0;
		}
	i=strlen(fgets(filename,PATH_MAX,f));
	while (filename[--i]=='\n'||filename[i]=='\r') filename[i]=0;
	return 1;
	}	
