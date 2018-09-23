rem called by testpemce

echo test %1 %2
cecopy %2 CE:\OpenSSL
cerun CE:\OpenSSL\%ssleay% %1 -in \OpenSSL\%2 -out \OpenSSL\%tmp1%
del %tmp1% >nul 2>&1
cecopy CE:\OpenSSL\%tmp1% .
%cmp% %2 %tmp1%
