@echo off

rem set ssleay=..\out\ssleay
set ssleay=%1

set reqcmd=%ssleay% req
set x509cmd=%ssleay% x509 -sha1
set verifycmd=%ssleay% verify

set CAkey=keyCA.ss
set CAcert=certCA.ss
set CAserial=certCA.srl
set CAreq=reqCA.ss
set CAconf=..\test\CAss.cnf
set CAreq2=req2CA.ss	

set Uconf=..\test\Uss.cnf
set Ukey=keyU.ss
set Ureq=reqU.ss
set Ucert=certU.ss

echo make a certificate request using 'req'
%reqcmd% -config %CAconf% -out %CAreq% -keyout %CAkey% -new
if errorlevel 1 goto e_req

echo convert the certificate request into a self signed certificate using 'x509'
%x509cmd% -CAcreateserial -in %CAreq% -days 30 -req -out %CAcert% -signkey %CAkey% >err.ss
if errorlevel 1 goto e_x509

echo --
echo convert a certificate into a certificate request using 'x509'
%x509cmd% -in %CAcert% -x509toreq -signkey %CAkey% -out %CAreq2% >err.ss
if errorlevel 1 goto e_x509_2

%reqcmd% -verify -in %CAreq% -noout
if errorlevel 1 goto e_vrfy_1

%reqcmd% -verify -in %CAreq2% -noout
if errorlevel 1 goto e_vrfy_2

%verifycmd% -CAfile %CAcert% %CAcert%
if errorlevel 1 goto e_vrfy_3

echo --
echo make another certificate request using 'req'
%reqcmd% -config %Uconf% -out %Ureq% -keyout %Ukey% -new >err.ss
if errorlevel 1 goto e_req_gen

echo --
echo sign certificate request with the just created CA via 'x509'
%x509cmd% -CAcreateserial -in %Ureq% -days 30 -req -out %Ucert% -CA %CAcert% -CAkey %CAkey% -CAserial %CAserial%
if errorlevel 1 goto e_x_sign

%verifycmd% -CAfile %CAcert% %Ucert%
echo --
echo Certificate details
%x509cmd% -subject -issuer -startdate -enddate -noout -in %Ucert%

echo Everything appeared to work
echo --
echo The generated CA certificate is %CAcert%
echo The generated CA private key is %CAkey%
echo The current CA signing serial number is in %CAserial%

echo The generated user certificate is %Ucert%
echo The generated user private key is %Ukey%
echo --

del err.ss

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
