NOTES
-----

I've checked out HPUX (well, version 11 at least) and shl_t is
a pointer type so it's safe to use in the way it has been in
dso_dl.c. On the other hand, HPUX11 support dlfcn too and
according to their man page, prefer developers to move to that.
I'll leave Richard's changes there as I guess dso_dl is needed
for HPUX10.20.

There is now a callback scheme in place where filename conversion can
(a) be turned off altogether through the use of the
    DSO_FLAG_NO_NAME_TRANSLATION flag,
(b) be handled by default using the default DSO_METHOD's converter
(c) overriden per-DSO by setting the override callback
(d) a mix of (b) and (c) - eg. implement an override callback that;
    (i) checks if we're win32 (if(strstr(dso->meth->name, "win32")....)
        and if so, convert "blah" into "blah32.dll" (the default is
	otherwise to make it "blah.dll").
    (ii) default to the normal behaviour - we're not on win32, eg.
         finish with (return dso->meth->dso_name_converter(dso,NULL)).

