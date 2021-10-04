Node.js

Node.js es un entorno de ejecución de JavaScript de código abierto y multiplataforma. Ejecuta código JavaScript fuera de un navegador. Para obtener más información sobre el uso de Node.js, consulte el sitio web de Node.js .

El proyecto Node.js utiliza un modelo de gobierno abierto . La Fundación OpenJS proporciona soporte para el proyecto.

Este proyecto está sujeto a un Código de Conducta .

Tabla de contenido
Apoyo
Tipos de lanzamiento
Descargar
Versiones actuales y LTS
Lanzamientos nocturnos
Documentación de la API
Verificando binarios
Construyendo Node.js
Seguridad
Contribuyendo a Node.js
Miembros actuales del equipo del proyecto
TSC (Comité Directivo Técnico)
Colaboradores
Triagers
Liberar llaves
Licencia
Apoyo
¿En busca de ayuda? Consulte las instrucciones para obtener asistencia .

Tipos de lanzamiento
Actual : En desarrollo activo. El código de la versión actual está en la rama de su número de versión principal (por ejemplo, v15.x ). Node.js lanza una nueva versión principal cada 6 meses, lo que permite realizar cambios importantes. Esto sucede en abril y octubre de todos los años. Los lanzamientos que aparecen cada octubre tienen una vida útil de 8 meses. Las versiones que aparecen cada abril se convierten a LTS (ver más abajo) cada octubre.
LTS : Versiones que reciben soporte a largo plazo, con un enfoque en la estabilidad y la seguridad. Cada versión principal par se convertirá en una versión LTS. Las versiones de LTS reciben 12 meses de soporte de LTS activo y 18 meses más de mantenimiento . Las líneas de lanzamiento de LTS tienen nombres de código ordenados alfabéticamente, comenzando con v4 Argon. No hay cambios importantes ni adiciones de funciones, excepto en algunas circunstancias especiales.
Nocturno : el código de la rama actual se crea cada 24 horas cuando hay cambios. Úselo con precaución.
Las versiones actuales y LTS siguen el control de versiones semántico . Un miembro del equipo de lanzamiento firma cada lanzamiento actual y LTS. Para obtener más información, consulte la versión README .

Descargar
Los archivos binarios, instaladores y tarballs de origen están disponibles en https://nodejs.org/en/download/ .

Versiones actuales y LTS
https://nodejs.org/download/release/

El directorio más reciente es un alias de la última versión actual. El directorio de nombre en código más reciente es un alias para la última versión de una línea LTS. Por ejemplo, el directorio latest-fermium contiene la última versión de Fermium (Node.js 14).

Lanzamientos nocturnos
https://nodejs.org/download/nightly/

Cada nombre de directorio y nombre de archivo contiene una fecha (en UTC) y el SHA de confirmación en el HEAD del lanzamiento.

Documentación de la API
La documentación de la última versión actual se encuentra en https://nodejs.org/api/ . La documentación específica de la versión está disponible en cada directorio de lanzamiento en el subdirectorio docs . La documentación específica de la versión también se encuentra en https://nodejs.org/download/docs/ .

Verificando binarios
Los directorios de descarga contienen un SHASUMS256.txtarchivo con sumas de comprobación SHA para los archivos.

Para descargar SHASUMS256.txtusando curl:

$ curl -O https://nodejs.org/dist/vx.yz/SHASUMS256.txt
Para comprobar que un archivo descargado coincide con la suma de comprobación, ejecútelo sha256sumcon un comando como:

$ grep node-vx.yztar.gz SHASUMS256.txt | sha256sum -c -
Para Current y LTS, la firma separada de GPG SHASUMS256.txtestá en SHASUMS256.txt.sig. Puede usarlo con gpgpara verificar la integridad de SHASUMS256.txt. Primero deberá importar las claves GPG de las personas autorizadas para crear lanzamientos . Para importar las claves:

$ gpg --keyserver pool.sks-keyservers.net --recv-keys DD8F2338BAE7501E3DD5AC78C273792F7D83545D
Consulte la parte inferior de este archivo README para obtener un script completo para importar claves de versión activas.

A continuación, descargue el SHASUMS256.txt.sigpara el lanzamiento:

$ curl -O https://nodejs.org/dist/vx.yz/SHASUMS256.txt.sig
Luego use gpg --verify SHASUMS256.txt.sig SHASUMS256.txtpara verificar la firma del archivo.

Construyendo Node.js
Consulte BUILDING.md para obtener instrucciones sobre cómo compilar Node.js desde la fuente y una lista de plataformas compatibles.

Seguridad
For information on reporting security vulnerabilities in Node.js, see SECURITY.md.

Contributing to Node.js
Contributing to the project
Working Groups
Strategic initiatives
Technical values and prioritization
Current project team members
For information about the governance of the Node.js project, see GOVERNANCE.md.

TSC (Technical Steering Committee)
aduh95 - Antoine du Hamel <duhamelantoine1995@gmail.com> (he/him)
apapirovski - Anatoli Papirovski <apapirovski@mac.com> (he/him)
BethGriggs - Beth Griggs <bgriggs@redhat.com> (she/her)
BridgeAR - Ruben Bridgewater <ruben@bridgewater.de> (he/him)
ChALkeR - Сковорода Никита Андреевич <chalkerx@gmail.com> (he/him)
cjihrig - Colin Ihrig <cjihrig@gmail.com> (he/him)
codebytere - Shelley Vohr <shelley.vohr@gmail.com> (she/her)
danielleadams - Danielle Adams <adamzdanielle@gmail.com> (she/her)
fhinkel - Franziska Hinkelmann <franziska.hinkelmann@gmail.com> (she/her)
gabrielschulhof - Gabriel Schulhof <gabrielschulhof@gmail.com>
gireeshpunathil - Gireesh Punathil <gpunathi@in.ibm.com> (he/him)
jasnell - James M Snell <jasnell@gmail.com> (he/him)
joyeecheung - Joyee Cheung <joyeec9h3@gmail.com> (she/her)
mcollina - Matteo Collina <matteo.collina@gmail.com> (he/him)
mhdawson - Michael Dawson <midawson@redhat.com> (he/him)
mmarchini - Mary Marchini <oss@mmarchini.me> (she/her)
MylesBorins - Myles Borins <myles.borins@gmail.com> (he/him)
ronag - Robert Nagy <ronagy@icloud.com>
targos - Michaël Zasso <targos@protonmail.com> (he/him)
tniessen - Tobias Nießen <tniessen@tnie.de>
Trott - Rich Trott <rtrott@gmail.com> (he/him)
Emeriti
Collaborators
addaleax - Anna Henningsen <anna@addaleax.net> (she/her)
aduh95 - Antoine du Hamel <duhamelantoine1995@gmail.com> (he/him)
ak239 - Aleksei Koziatinskii <ak239spb@gmail.com>
antsmartian - Anto Aravinth <anto.aravinth.cse@gmail.com> (he/him)
apapirovski - Anatoli Papirovski <apapirovski@mac.com> (he/him)
AshCripps - Ash Cripps <acripps@redhat.com>
Ayase-252 - Qingyu Deng <i@ayase-lab.com>
bcoe - Ben Coe <bencoe@gmail.com> (he/him)
bengl - Bryan English <bryan@bryanenglish.com> (he/him)
benjamingr - Benjamin Gruenbaum <benjamingr@gmail.com>
BethGriggs - Beth Griggs <bgriggs@redhat.com> (she/her)
bmeck - Bradley Farias <bradley.meck@gmail.com>
boneskull - Christopher Hiller < boneskull@boneskull.com > (él / él)
BridgeAR - Ruben Bridgewater < ruben@bridgewater.de > (él / él)
bzoz - Bartosz Sosnowski < bartosz@janeasystems.com >
cclauss - Christian Clauss < cclauss@me.com > (él / él)
CHALKER - Сковорода Никита Андреевич < chalkerx@gmail.com > (él / él)
cjihrig - Colin Ihrig < cjihrig@gmail.com > (él / él)
codebytere - Shelley Vohr < shelley.vohr@gmail.com > (ella / ella)
danbev - Daniel Bevenius < daniel.bevenius@gmail.com > (él / él)
danielleadams - Danielle Adams < adamzdanielle@gmail.com > (ella / ella)
davisjam - Jamie Davis < davisjam@vt.edu > (él / él)
DerekNonGeneric - Derek Lewis < DerekNonGeneric@inf.is > (él / él)
devnexen - David Carlier < devnexen@gmail.com >
devsnek - Gus Caplan < me@gus.host > (ellos / ellos)
dmabupt - Xu Meng < dmabupt@gmail.com > (él / él)
dnlup Daniele Belardi < dwon.dnl@gmail.com > (él / él)
edsadr - Adrian Estrada < edsadr@gmail.com > (él / él)
eugeneo - Eugene Ostroukhov < eostroukhov@google.com >
evanlucas - Evan Lucas < evanlucas@me.com > (él / él)
fhinkel - Franziska Hinkelmann < franziska.hinkelmann@gmail.com > (ella / ella)
Fishrock123 - Jeremiah Senkpiel < fishrock123@rocketmail.com > (él / ellos)
Flarna - Gerhard Stöbich < deb2001-github@yahoo.de > (él / ellos)
gabrielschulhof - Gabriel Schulhof < gabrielschulhof@gmail.com >
gengjiawen - Jiawen Geng < technicalcute@gmail.com >
GeoffreyBooth - Geoffrey Booth < webadmin@geoffreybooth.com > (él / él)
gireeshpunathil - Gireesh Punathil < gpunathi@in.ibm.com > (él / él)
guybedford - Guy Bedford < guybedford@gmail.com > (él / él)
HarshithaKP - Harshitha KP < harshitha014@gmail.com > (ella / ella)
hashseed - Yang Guo < yangguo@chromium.org > (él / él)
él mismo65 - Zeyu Yang <él mismo65@outlook.com > (él / él)
hiroppy - Yuta Hiroto < hello@hiroppy.me > (él / él)
iansu - Ian Sutherland < ian@iansutherland.ca >
indutny - Fedor Indutny < fedor@indutny.com >
JacksonTian - Jackson Tian < shyvo1987@gmail.com >
jasnell - James M Snell < jasnell@gmail.com > (él / él)
jkrems - Jan Krems < jan.krems@gmail.com > (él / él)
joaocgreis - João Reis < reis@janeasystems.com >
joyeecheung - Joyee Cheung < joyeec9h3@gmail.com > (ella / ella)
juanarbol - Juan José Arboleda < soyjuanarbol@gmail.com > (él / él)
JungMinu - Minwoo Jung < nodecorelab@gmail.com > (él / él)
legendecas - Chengzhong Wu < legendecas@gmail.com > (él / él)
Leko - Shingo Inoue < leko.noor@gmail.com > (él / él)
linkgoron - Nitzan Uziely < linkgoron@gmail.com >
lpinca - Luigi Pinca < luigipinca@gmail.com > (él / él)
lundibundi - Denys Otrishko < shishugi@gmail.com > (él / él)
Lxxyx - Zijian Liu < lxxyxzj@gmail.com > (él / él)
mafintosh - Mathias Buus < mathiasbuus@gmail.com > (él / él)
mcollina - Matteo Collina < matteo.collina@gmail.com > (él / él)
mhdawson - Michael Dawson < midawson@redhat.com > (él / él)
miladfarca - Milad Fa < mfarazma@redhat.com > (él / él)
mildsunrise - Alba Mendez < me@alba.sh > (ella / ella)
mmarchini - Mary Marchini < oss@mmarchini.me > (ella / ella)
mscdex - Brian White < mscdex@mscdex.net >
MylesBorins - Myles Borins < myles.borins@gmail.com > (él / él)
oyyd - Ouyang Yadong < oyydoibh@gmail.com > (él / él)
panva - Filip Skokan < panva.ip@gmail.com >
PoojaDurgad - Pooja DP < Pooja.DP@ibm.com > (ella / ella)
puzpuzpuz - Andrey Pechkurov < apechkurov@gmail.com > (él / él)
Qard - Stephen Belanger < admin@stephenbelanger.com > (él / él)
RaisinTen - Darshan Sen < raisinten@gmail.com > (él / él)
rexagod - Pranshu Srivastava < rexagod@gmail.com > (él / él)
richardlau - Richard Lau < rlau@redhat.com >
rickyes - Ricky Zhou < 0x19951125@gmail.com > (él / él)
ronag - Robert Nagy <ronagy@icloud.com>
ruyadorno - Ruy Adorno <ruyadorno@github.com> (he/him)
rvagg - Rod Vagg <rod@vagg.org>
ryzokuken - Ujjwal Sharma <ryzokuken@disroot.org> (he/him)
santigimeno - Santiago Gimeno <santiago.gimeno@gmail.com>
seishun - Nikolai Vavilov <vvnicholas@gmail.com>
shisama - Masashi Hirano <shisama07@gmail.com> (he/him)
silverwind - Roman Reiss <me@silverwind.io>
srl295 - Steven R Loomis <srloomis@us.ibm.com>
starkwang - Weijia Wang <starkwang@126.com>
sxa - Stewart X Addison <sxa@redhat.com> (he/him)
targos - Michaël Zasso <targos@protonmail.com> (he/him)
TimothyGu - Tiancheng "Timothy" Gu <timothygu99@gmail.com> (he/him)
tniessen - Tobias Nießen <tniessen@tnie.de>
trivikr - Trivikram Kamat <trivikr.dev@gmail.com>
Trott - Rich Trott <rtrott@gmail.com> (he/him)
vdeturckheim - Vladimir de Turckheim <vlad2t@hotmail.com> (he/him)
watilde - Daijiro Wachi <daijiro.wachi@gmail.com> (he/him)
watson - Thomas Watson <w@tson.dk>
XadillaX - Khaidi Chu <i@2333.moe> (he/him)
yashLadha - Yash Ladha <yash@yashladha.in> (he/him)
yhwang - Yihong Wang <yh.wang@ibm.com>
yosuke-furukawa - Yosuke Furukawa <yosuke.furukawa@gmail.com>
ZYSzys - Yongsheng Zhang <zyszys98@gmail.com> (he/him)
Emeriti
Collaborators follow the Collaborator Guide in maintaining the Node.js project.

Triagers
Ayase-252 - Qingyu Deng <i@ayase-lab.com>
himadriganguly - Himadri Ganguly <himadri.tech@gmail.com> (he/him)
iam-frankqiu - Frank Qiu <iam.frankqiu@gmail.com> (he/him)
marsonya - Akhil Marsonya <akhil.marsonya27@gmail.com> (he/him)
Mesteery - Mestery <mestery@pm.me>
PoojaDurgad - Pooja Durgad <Pooja.D.P@ibm.com>
RaisinTen - Darshan Sen <raisinten@gmail.com>
VoltrexMaster - Voltrex <mohammadkeyvanzade94@gmail.com> (he/him)
Release keys
Primary GPG keys for Node.js Releasers (some Releasers sign with subkeys):

Beth Griggs < bgriggs@redhat.com > 4ED778F539E3634C779C87C6D7062848A1AB005C
Colin Ihrig < cjihrig@gmail.com > 94AE36675C464D64BAFA68DD7434390BDBE9B9C5
Danielle Adams < adamzdanielle@gmail.com > 74F12602B6F1C4E913FAA37AD3A89613643B6201
James M Snell < jasnell@keybase.io > 71DCFD284A79C3B38668286BC97EC7A07EDE3FC1
Michaël Zasso < targos@protonmail.com > 8FCCA13FEF1D0C2E91008E09770F7A9A5AE15600
Myles Borins < myles.borins@gmail.com > C4F0DFFF4E8C1A8236409D08E73BC641CC11F4C8
Richard Lau < rlau@redhat.com > C82FA3AE1CBEDC6BE46B9360C43CEC45C17AB93C
Rod Vagg < rod@vagg.org > DD8F2338BAE7501E3DD5AC78C273792F7D83545D
Ruben Bridgewater < ruben@bridgewater.de > A48C2BEE680E841632CD4E44F07496B3EB3C1762
Ruy Adorno < ruyadorno@hotmail.com > 108F52B48DB57BB0CC439B2997B01419BD92F80A
Shelley Vohr < shelley.vohr@gmail.com > B9E2F5981AA6E0CD28160D9FF13993A75599653C
Para importar el conjunto completo de claves de versión de confianza (incluidas las subclaves que posiblemente se utilicen para firmar las versiones):

gpg --keyserver pool.sks-keyservers.net --recv-keys 4ED778F539E3634C779C87C6D7062848A1AB005C
gpg --keyserver pool.sks-keyservers.net --recv-keys 94AE36675C464D64BAFA68DD7434390BDBE9B9C5
gpg --keyserver pool.sks-keyservers.net --recv-keys 74F12602B6F1C4E913FAA37AD3A89613643B6201
gpg --keyserver pool.sks-keyservers.net --recv-keys 71DCFD284A79C3B38668286BC97EC7A07EDE3FC1
gpg --keyserver pool.sks-keyservers.net --recv-keys 8FCCA13FEF1D0C2E91008E09770F7A9A5AE15600
gpg --keyserver pool.sks-keyservers.net --recv-keys C4F0DFFF4E8C1A8236409D08E73BC641CC11F4C8
gpg --keyserver pool.sks-keyservers.net --recv-keys C82FA3AE1CBEDC6BE46B9360C43CEC45C17AB93C
gpg --keyserver pool.sks-keyservers.net --recv-keys DD8F2338BAE7501E3DD5AC78C273792F7D83545D
gpg --keyserver pool.sks-keyservers.net --recv-keys A48C2BEE680E841632CD4E44F07496B3EB3C1762
gpg --keyserver pool.sks-keyservers.net --recv-keys 108F52B48DB57BB0CC439B2997B01419BD92F80A
gpg --keyserver pool.sks-keyservers.net --recv-keys B9E2F5981AA6E0CD28160D9FF13993A75599653C
Consulte la sección anterior sobre Verificación de binarios para saber cómo usar estas claves para verificar un archivo descargado.

Otras claves utilizadas para firmar algunos lanzamientos anteriores
Licencia
Node.js está disponible bajo la licencia MIT . Node.js también incluye bibliotecas externas que están disponibles bajo una variedad de licencias. Consulte LICENCIA para obtener el texto completo de la licencia.
