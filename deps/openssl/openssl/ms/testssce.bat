rem set ssleay=..\out\ssleay
set ssleay=%1

set reqcmd=%ssleay% req
set x509cmd=%ssleay% x509
set verifycmd=%ssleay% verify

set CAkey=\OpenSSL\keyCA.ss
set CAcert=\OpenSSL\certCA.ss
set CAserial=\OpenSSL\certCA.srl
set CAreq=\OpenSSL\reqCA.ss
cecopy ..\test\CAss.cnf CE:\OpenSSL
set CAconf=\OpenSSL\CAss.cnf
set CAreq2=\OpenSSL\req2CA.ss	

cecopy ..\test\Uss.cnf CE:\OpenSSL
set Uconf=\OpenSSL\Uss.cnf
set Ukey=\OpenSSL\keyU.ss
set Ureq=\OpenSSL\reqU.ss
set Ucert=\OpenSSL\certU.ss

echo make a certificate request using 'req'
cerun CE:\OpenSSL\%reqcmd% -config %CAconf% -out %CAreq% -keyout %CAkey% -new
if errorlevel 1 goto e_req

echo convert the certificate request into a self signed certificate using 'x509'
cerun CE:\OpenSSL\%x509cmd% -CAcreateserial -in %CAreq% -days 30 -req -out %CAcert% -signkey %CAkey% "> \OpenSSL\err.ss"
if errorlevel 1 goto e_x509

echo --
echo convert a certificate into a certificate request using 'x509'
cerun CE:\OpenSSL\%x509cmd% -in %CAcert% -x509toreq -signkey %CAkey% -out %CAreq2% "> \OpenSSL\err.ss"
if errorlevel 1 goto e_x509_2

cerun CE:\OpenSSL\%reqcmd% -verify -in %CAreq% -noout
if errorlevel 1 goto e_vrfy_1

cerun CE:\OpenSSL\%reqcmd% -verify -in %CAreq2% -noout
if errorlevel 1 goto e_vrfy_2

cerun CE:\OpenSSL\%verifycmd% -CAfile %CAcert% %CAcert%
if errorlevel 1 goto e_vrfy_3

echo --
echo make another certificate request using 'req'
cerun CE:\OpenSSL\%reqcmd% -config %Uconf% -out %Ureq% -keyout %Ukey% -new "> \OpenSSL\err.ss"
if errorlevel 1 goto e_req_gen

echo --
echo sign certificate request with the just created CA via 'x509'
cerun CE:\OpenSSL\%x509cmd% -CAcreateserial -in %Ureq% -days 30 -req -out %Ucert% -CA %CAcert% -CAkey %CAkey% -CAserial %CAserial%
if errorlevel 1 goto e_x_sign

cerun CE:\OpenSSL\%verifycmd% -CAfile %CAcert% %Ucert%
echo --
echo Certificate details
cerun CE:\OpenSSL\%x509cmd% -subject -issuer -startdate -enddate -noout -in %Ucert%

cecopy CE:%CAcert% .
cecopy CE:%CAkey% .
cecopy CE:%CAserial% .
cecopy CE:%Ucert% .
cecopy CE:%Ukey% .

echo Everything appeared to work
echo --
echo The generated CA certificate is %CAcert%
echo The generated CA private key is %CAkey%
echo The current CA signing serial number is in %CAserial%

echo The generated user certificate is %Ucert%
echo The generated user private key is %Ukey%
echo --

cedel CE:\OpenSSL\err.ss

goto end

:e_req
echo error using 'req' to generate a certificate request
goto end
:e_x509
echo error using 'x509' to self sign a certificate request
goto end
:e_x509_2
echo error using 'x509' convert a certificate to a certificate request
goto end
:e_vrfy_1
echo first generated request is invalid
goto end
:e_vrfy_2
echo second generated request is invalid
goto end
:e_vrfy_3
echo first generated cert is invalid
goto end
:e_req_gen
echo error using 'req' to generate a certificate request
goto end
:e_x_sign
echo error using 'x509' to sign a certificate request
goto end

:end
