rem called by testenc

echo test %1 %2 %3 %4 %5 %6 
%ssleay% %1 %2 %3 %4 %5 %6 -e -bufsize 113 -k test -in %input% -out %tmp1%
%ssleay% %1 %2 %3 %4 %5 %6 -d -bufsize 157 -k test -in %tmp1% -out %out1%
%cmp% %input% %out1%
if errorlevel 1 goto err

echo test base64 %1 %2 %3 %4 %5 %6 
%ssleay% %1 %2 %3 %4 %5 %6 -a -e -bufsize 113 -k test -in %input% -out %tmp1%
%ssleay% %1 %2 %3 %4 %5 %6 -a -d -bufsize 157 -k test -in %tmp1% -out %out1%
%cmp% %input% %out1%

:err
