#
# Origional BC Makefile from Teun <Teun.Nijssen@kub.nl>
#
#
CC      = bcc
TLIB    = tlib /0 /C
# note: the -3 flag produces code for 386, 486, Pentium etc; omit it for 286s
OPTIMIZE= -3 -O2
#WINDOWS= -W
CFLAGS  = -c -ml -d $(OPTIMIZE) $(WINDOWS) -DMSDOS
LFLAGS  = -ml $(WINDOWS)

.c.obj:
	$(CC) $(CFLAGS) $*.c

.obj.exe:
	$(CC) $(LFLAGS) -e$*.exe $*.obj libdes.lib  

all: $(LIB) destest.exe rpw.exe des.exe speed.exe

# "make clean": use a directory containing only libdes .exe and .obj files...
clean:
	del *.exe
	del *.obj
	del libdes.lib
	del libdes.rsp

OBJS=   cbc_cksm.obj cbc_enc.obj  ecb_enc.obj  pcbc_enc.obj \
	qud_cksm.obj rand_key.obj set_key.obj  str2key.obj \
	enc_read.obj enc_writ.obj fcrypt.obj   cfb_enc.obj \
	ecb3_enc.obj ofb_enc.obj  cbc3_enc.obj read_pwd.obj\
	cfb64enc.obj ofb64enc.obj ede_enc.obj  cfb64ede.obj\
	ofb64ede.obj supp.obj

LIB=    libdes.lib

$(LIB): $(OBJS)
	del $(LIB)
	makersp "+%s &\n" &&|
	$(OBJS)
|       >libdes.rsp
	$(TLIB) libdes.lib @libdes.rsp,nul
	del libdes.rsp

destest.exe: destest.obj libdes.lib
rpw.exe:     rpw.obj libdes.lib
speed.exe:   speed.obj libdes.lib
des.exe:     des.obj libdes.lib


