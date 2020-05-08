# The tests are compiled and linked using musl-cross. The advantage of this is
# that everything is self-contained and thus easy to reproduce. Musl is also
# designed for static linking with little overhead, which gives us a good
# metric for the "complete package" of a statically linked minimal XML reader
# for each lib.

FILE1=enwiki-20130805-abstract5.xml
FILE1URL=http://dumps.wikimedia.org/enwiki/20130805/enwiki-20130805-abstract5.xml
FILE2=discogs_20130801_labels.xml
FILE2URL=http://www.discogs.com/data/${FILE2}.gz
FILES=${FILE1} ${FILE2}
MUSLCROSS=x86_64-linux-musl
MUSLCROSSURL=https://googledrive.com/host/0BwnS5DMB0YQ6bURENWxvR0toTFU/crossx86-x86_64-linux-musl-0.9.13.tar.xz
EXPAT=expat-2.1.0
EXPATURL=http://downloads.sourceforge.net/project/expat/expat/2.1.0/${EXPAT}.tar.gz
LIBXML2=libxml2-2.9.1
LIBXML2URL=ftp://xmlsoft.org/libxml2/${LIBXML2}.tar.gz
MXML=mxml-2.7
MXMLURL=http://www.msweet.org/files/project3/${MXML}.tar.gz

HOST=x86_64-musl-linux
CC=${HOST}-gcc
AR=${HOST}-ar
STRIP=${HOST}-strip
export CFLAGS = -static -O2
export PATH := ${PATH}:${PWD}/${MUSLCROSS}/bin

.PHONY: notice all clean distclean

notice:
	@echo "You probably don't want to 'make' these benchmarks."
	@echo "Lots of stuff is downloaded, lots of stuff is compiled and lots of stuff is run."
	@echo "Run 'make all -j1' if you do have the patience to run the benchmarks."
	@echo
	@false

build: strlen yxml expat libxml2 mxml
all: strlen-bench yxml-bench expat-bench libxml2-bench mxml-bench

${FILE1}:
	curl ${FILE1URL} -O ${FILE1}

${FILE2}:
	@echo "This doesn't work. Probably want to download the file manually with a browser"
	curl ${FILE2URL} | zcat > ${FILE2}

${MUSLCROSS}:
	curl -L ${MUSLCROSSURL} | xzcat | tar -xvf-

${EXPAT}:
	curl -L ${EXPATURL} | zcat | tar -xvf-

${LIBXML2}:
	curl ${LIBXML2URL} | zcat | tar -xvf-

${MXML}:
	curl ${MXMLURL} | zcat | tar -xvf-



yxml.o: ../yxml.c
	${CC} ${CFLAGS} -I.. ../yxml.c -c

yxml: ${MUSLCROSS} bench.c yxml.o
	${CC} ${CFLAGS} -I.. -DYXML bench.c yxml.o -o yxml
	${STRIP} -s yxml

yxml-bench: yxml ${FILES}
	./runbench.sh yxml.o yxml ${FILES}



strlen: ${MUSLCROSS} bench.c
	${CC} ${CFLAGS} -DSTRLEN bench.c -o strlen
	${STRIP} -s strlen

strlen-bench: strlen ${FILES}
	./runbench.sh strlen strlen ${FILES}



libexpat.a: ${MUSLCROSS} ${EXPAT}
	cd ${EXPAT} && ./configure --disable-shared --host=${HOST} && make buildlib
	cp ${EXPAT}/.libs/libexpat.a libexpat.a

expat: libexpat.a bench.c
	${CC} ${CFLAGS} -I${EXPAT}/lib -DEXPAT bench.c -L. -lexpat -o expat
	${STRIP} -s expat

expat-bench: expat ${FILES}
	./runbench.sh libexpat.a expat ${FILES}



libxml2.a: ${MUSLCROSS} ${LIBXML2}
	@# So many options, yet --with-minimum and --without-output don't seem to work
	cd ${LIBXML2} && ./configure --disable-shared --without-c14n --without-catalog --without-debug\
		--without-docbook --without-ftp --without-html --without-http --without-iconv --without-iso8859x\
		--without-legacy --without-pattern --without-push --without-regexps --without-sax1\
		--without-schemas --without-schematron --without-threads --without-tree --without-valid\
		--without-writer --without-xinclude --without-xpath --without-xpath --without-modules\
		--without-zlib --without-lzma --host ${HOST} && make libxml2.la
	cp ${LIBXML2}/.libs/libxml2.a libxml2.a

libxml2: libxml2.a bench.c
	${CC} ${CFLAGS} -I${LIBXML2}/include -DLIBXML2 bench.c -L. -lxml2 -o libxml2
	${STRIP} -s libxml2

libxml2-bench: libxml2 ${FILES}
	./runbench.sh libxml2.a libxml2 ${FILES}



libmxml.a: ${MUSLCROSS} ${MXML}
	cd ${MXML} && ./configure --disable-shared --disable-threads --host ${HOST} && make libmxml.a
	cp ${MXML}/libmxml.a libmxml.a

mxml: libmxml.a bench.c
	${CC} ${CFLAGS} -I${MXML} -DMXML bench.c -L. -lmxml -o mxml
	${STRIP} -s mxml

mxml-bench: mxml ${FILES}
	./runbench.sh libmxml.a mxml ${FILES}



clean:
	rm -f *.a *.o *-bench yxml strlen expat libxml2 mxml

distclean: clean
	rm -rf ${MUSLCROSS} ${FILES} ${EXPAT} ${LIBXML2} ${MXML}
