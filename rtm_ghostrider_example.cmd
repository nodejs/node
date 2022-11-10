:: Example batch file for mining Raptoreum at a pool
::
:: Format:
::      xmrig.exe -a gr -o <pool address>:<pool port> -u <pool username/wallet> -p <pool password>
::
:: Fields:
::      pool address            The host name of the pool stratum or its IP address, for example raptoreumemporium.com
::      pool port               The port of the pool's stratum to connect to, for example 3333. Check your pool's getting started page.
::      pool username/wallet    For most pools, this is the wallet address you want to mine to. Some pools require a username
::      pool password           For most pools this can be just 'x'. For pools using usernames, you may need to provide a password as configured on the pool.
::
:: List of Raptoreum mining pools:
::      https://miningpoolstats.stream/raptoreum
::
:: Choose pools outside of top 5 to help Raptoreum network be more decentralized!
:: Smaller pools also often have smaller fees/payout limits.

cd %~dp0
:: Use this command line to connect to non-SSL port
xmrig.exe -a gr -o raptoreumemporium.com:3008 -u WALLET_ADDRESS -p x
:: Or use this command line to connect to an SSL port
:: xmrig.exe -a gr -o rtm.suprnova.cc:4273 --tls -u WALLET_ADDRESS -p x
pause
