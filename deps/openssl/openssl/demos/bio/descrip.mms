# This build description trusts that the following logical names are defined:
#
# For compilation: OPENSSL
# For linking with shared libraries: OSSL$LIBCRYPTO_SHR and OSSL$LIBSSL_SHR
# For linking with static libraries: OSSL$LIBCRYPTO and OSSL$LIBSSL
#
# These are normally defined with the OpenSSL startup procedure

# By default, we link with the shared libraries
SHARED = TRUE

# Alternative, for linking with static libraries
#SHARED = FALSE

.FIRST :
	IF "$(SHARED)" .EQS. "TRUE" THEN DEFINE OPT []shared.opt
	IF "$(SHARED)" .NES. "TRUE" THEN DEFINE OPT []static.opt

.LAST :
	DEASSIGN OPT

.DEFAULT :
	@ !

# Because we use an option file, we need to redefine this
.obj.exe :
	$(LINK) $(LINKFLAGS) $<,OPT:/OPT

all : client-arg.exe client-conf.exe saccept.exe sconnect.exe -
      server-arg.exe server-cmod.exe server-conf.exe

client-arg.exe : client-arg.obj
client-conf.exe : client-conf.obj
saccept.exe : saccept.obj
sconnect.exe : sconnect.obj
server-arg.exe : server-arg.obj
server-cmod.exe : server-cmod.obj
server-conf.exe : server-conf.obj

# MMS doesn't infer this automatically...
client-arg.obj : client-arg.c
client-conf.obj : client-conf.c
saccept.obj : saccept.c
sconnect.obj : sconnect.c
server-arg.obj : server-arg.c
server-cmod.obj : server-cmod.c
server-conf.obj : server-conf.c
