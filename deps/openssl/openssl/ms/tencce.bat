rem called by testencce

echo test %1 %2 %3 %4 %5 %6 
cecopy %input% CE:\OpenSSL
cerun CE:\OpenSSL\%ssleay% %1 %2 %3 %4 %5 %6 -e -bufsize 113 -k test -in \OpenSSL\%input% -out \OpenSSL\%tmp1%
cerun CE:\OpenSSL\%ssleay% %1 %2 %3 %4 %5 %6 -d -bufsize 157 -k test -in \OpenSSL\%tmp1% -out \OpenSSL\%out1%
del %out1% >nul 2>&1
cecopy CE:\OpenSSL\%out1% .
%cmp% %input% %out1%
if errorlevel 1 goto err

echo test base64 %1 %2 %3 %4 %5 %6 
cerun CE:\OpenSSL\%ssleay% %1 %2 %3 %4 %5 %6 -a -e -bufsize 113 -k test -in \OpenSSL\%input% -out \OpenSSL\%tmp1%
cerun CE:\OpenSSL\%ssleay% %1 %2 %3 %4 %5 %6 -a -d -bufsize 157 -k test -in \OpenSSL\%tmp1% -out \OpenSSL\%out1%
del %out1% >nul 2>&1
cecopy CE:\OpenSSL\%out1% .
%cmp% %input% %out1%

:err
