# perl script to run OpenSSL tests


my $base_path      = "\\openssl";

my $output_path    = "$base_path\\test_out";
my $cert_path      = "$base_path\\certs";
my $test_path      = "$base_path\\test";
my $app_path       = "$base_path\\apps";

my $tmp_cert       = "$output_path\\cert.tmp";
my $OpenSSL_config = "$app_path\\openssl.cnf";
my $log_file       = "$output_path\\tests.log";

my $pause = 0;


#  process the command line args to see if they wanted us to pause
#  between executing each command
foreach $i (@ARGV)
{
   if ($i =~ /^-p$/)
   { $pause=1; }
}



main();


############################################################################
sub main()
{
   # delete all the output files in the output directory
   unlink <$output_path\\*.*>;

   # open the main log file
   open(OUT, ">$log_file") || die "unable to open $log_file\n";

   print( OUT "========================================================\n");
   my $outFile = "$output_path\\version.out";
   system("openssl2 version (CLIB_OPT)/>$outFile");
   log_output("CHECKING FOR OPENSSL VERSION:", $outFile);

   algorithm_tests();
   encryption_tests();
   evp_tests();
   pem_tests();
   verify_tests();
   ca_tests();
   ssl_tests();

   close(OUT);

   print("\nCompleted running tests.\n\n");
   print("Check log file for errors: $log_file\n");
}

############################################################################
sub algorithm_tests
{
   my $i;
   my $outFile;
   my @tests = ( rsa_test, destest, ideatest, bftest, bntest, shatest, sha1test,
                 sha256t, sha512t, dsatest, md2test, md4test, md5test, mdc2test,
                 rc2test, rc4test, rc5test, randtest, rmdtest, dhtest, ecdhtest,
                 ecdsatest, ectest, exptest, casttest, hmactest );

   print( "\nRUNNING CRYPTO ALGORITHM TESTS:\n\n");

   print( OUT "\n========================================================\n");
   print( OUT "CRYPTO ALGORITHM TESTS:\n\n");

   foreach $i (@tests)
   {
      if (-e "$base_path\\$i.nlm")
      {
         $outFile = "$output_path\\$i.out";
         system("$i (CLIB_OPT)/>$outFile");
         log_desc("Test: $i\.nlm:");
         log_output("", $outFile );
      }
      else
      {
         log_desc("Test: $i\.nlm: file not found");
      }
   }
}

############################################################################
sub encryption_tests
{
   my $i;
   my $outFile;
   my @enc_tests = ( "enc", "rc4", "des-cfb", "des-ede-cfb", "des-ede3-cfb",
                     "des-ofb", "des-ede-ofb", "des-ede3-ofb",
                     "des-ecb", "des-ede", "des-ede3", "des-cbc",
                     "des-ede-cbc", "des-ede3-cbc", "idea-ecb", "idea-cfb",
                     "idea-ofb", "idea-cbc", "rc2-ecb", "rc2-cfb",
                     "rc2-ofb", "rc2-cbc", "bf-ecb", "bf-cfb",
                     "bf-ofb", "bf-cbc" );

   my $input = "$base_path\\do_tests.pl";
   my $cipher = "$output_path\\cipher.out";
   my $clear = "$output_path\\clear.out";

   print( "\nRUNNING ENCRYPTION & DECRYPTION TESTS:\n\n");

   print( OUT "\n========================================================\n");
   print( OUT "FILE ENCRYPTION & DECRYPTION TESTS:\n\n");

   foreach $i (@enc_tests)
   {
      log_desc("Testing: $i");

      # do encryption
      $outFile = "$output_path\\enc.out";
      system("openssl2 $i -e -bufsize 113 -k test -in $input -out $cipher (CLIB_OPT)/>$outFile" );
      log_output("Encrypting: $input --> $cipher", $outFile);

      # do decryption
      $outFile = "$output_path\\dec.out";
      system("openssl2 $i -d -bufsize 157 -k test -in $cipher -out $clear (CLIB_OPT)/>$outFile");
      log_output("Decrypting: $cipher --> $clear", $outFile);

      # compare files
      $x = compare_files( $input, $clear, 1);
      if ( $x == 0 )
      {
         print( "\rSUCCESS - files match: $input, $clear\n");
         print( OUT "SUCCESS - files match: $input, $clear\n");
      }
      else
      {
         print( "\rERROR: files don't match\n");
         print( OUT "ERROR: files don't match\n");
      }

      do_wait();

      # Now do the same encryption but use Base64

      # do encryption B64
      $outFile = "$output_path\\B64enc.out";
      system("openssl2 $i -a -e -bufsize 113 -k test -in $input -out $cipher (CLIB_OPT)/>$outFile");
      log_output("Encrypting(B64): $cipher --> $clear", $outFile);

      # do decryption B64
      $outFile = "$output_path\\B64dec.out";
      system("openssl2 $i -a -d -bufsize 157 -k test -in $cipher -out $clear (CLIB_OPT)/>$outFile");
      log_output("Decrypting(B64): $cipher --> $clear", $outFile);

      # compare files
      $x = compare_files( $input, $clear, 1);
      if ( $x == 0 )
      {
         print( "\rSUCCESS - files match: $input, $clear\n");
         print( OUT "SUCCESS - files match: $input, $clear\n");
      }
      else
      {
         print( "\rERROR: files don't match\n");
         print( OUT "ERROR: files don't match\n");
      }

      do_wait();

   } # end foreach

   # delete the temporary files
   unlink($cipher);
   unlink($clear);
}


############################################################################
sub pem_tests
{
   my $i;
   my $tmp_out;
   my $outFile = "$output_path\\pem.out";

   my %pem_tests = (
         "crl"      => "testcrl.pem",
          "pkcs7"   => "testp7.pem",
          "req"     => "testreq2.pem",
          "rsa"     => "testrsa.pem",
          "x509"    => "testx509.pem",
          "x509"    => "v3-cert1.pem",
          "sess_id" => "testsid.pem"  );


   print( "\nRUNNING PEM TESTS:\n\n");

   print( OUT "\n========================================================\n");
   print( OUT "PEM TESTS:\n\n");

   foreach $i (keys(%pem_tests))
   {
      log_desc( "Testing: $i");

      my $input = "$test_path\\$pem_tests{$i}";

      $tmp_out = "$output_path\\$pem_tests{$i}";

      if ($i ne "req" )
      {
         system("openssl2 $i -in $input -out $tmp_out (CLIB_OPT)/>$outFile");
         log_output( "openssl2 $i -in $input -out $tmp_out", $outFile);
      }
      else
      {
         system("openssl2 $i -in $input -out $tmp_out -config $OpenSSL_config (CLIB_OPT)/>$outFile");
         log_output( "openssl2 $i -in $input -out $tmp_out -config $OpenSSL_config", $outFile );
      }

      $x = compare_files( $input, $tmp_out);
      if ( $x == 0 )
      {
         print( "\rSUCCESS - files match: $input, $tmp_out\n");
         print( OUT "SUCCESS - files match: $input, $tmp_out\n");
      }
      else
      {
         print( "\rERROR: files don't match\n");
         print( OUT "ERROR: files don't match\n");
      }
      do_wait();

   } # end foreach
}


############################################################################
sub verify_tests
{
   my $i;
   my $outFile = "$output_path\\verify.out";

   $cert_path =~ s/\\/\//g;
   my @cert_files = <$cert_path/*.pem>;

   print( "\nRUNNING VERIFY TESTS:\n\n");

   print( OUT "\n========================================================\n");
   print( OUT "VERIFY TESTS:\n\n");

   make_tmp_cert_file();

   foreach $i (@cert_files)
   {
      system("openssl2 verify -CAfile $tmp_cert $i (CLIB_OPT)/>$outFile");
      log_desc("Verifying cert: $i");
      log_output("openssl2 verify -CAfile $tmp_cert $i", $outFile);
   }
}


############################################################################
sub ssl_tests
{
   my $outFile = "$output_path\\ssl_tst.out";
   my($CAcert) = "$output_path\\certCA.ss";
   my($Ukey)   = "$output_path\\keyU.ss";
   my($Ucert)  = "$output_path\\certU.ss";
   my($ssltest)= "ssltest -key $Ukey -cert $Ucert -c_key $Ukey -c_cert $Ucert -CAfile $CAcert";

   print( "\nRUNNING SSL TESTS:\n\n");

   print( OUT "\n========================================================\n");
   print( OUT "SSL TESTS:\n\n");

   system("ssltest -ssl2 (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2:");
   log_output("ssltest -ssl2", $outFile);

   system("$ssltest -ssl2 -server_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2 with server authentication:");
   log_output("$ssltest -ssl2 -server_auth", $outFile);

   system("$ssltest -ssl2 -client_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2 with client authentication:");
   log_output("$ssltest -ssl2 -client_auth", $outFile);

   system("$ssltest -ssl2 -server_auth -client_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2 with both client and server authentication:");
   log_output("$ssltest -ssl2 -server_auth -client_auth", $outFile);

   system("ssltest -ssl3 (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv3:");
   log_output("ssltest -ssl3", $outFile);

   system("$ssltest -ssl3 -server_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv3 with server authentication:");
   log_output("$ssltest -ssl3 -server_auth", $outFile);

   system("$ssltest -ssl3 -client_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv3 with client authentication:");
   log_output("$ssltest -ssl3 -client_auth", $outFile);

   system("$ssltest -ssl3 -server_auth -client_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv3 with both client and server authentication:");
   log_output("$ssltest -ssl3 -server_auth -client_auth", $outFile);

   system("ssltest (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2/sslv3:");
   log_output("ssltest", $outFile);

   system("$ssltest -server_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2/sslv3 with server authentication:");
   log_output("$ssltest -server_auth", $outFile);

   system("$ssltest -client_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2/sslv3 with client authentication:");
   log_output("$ssltest -client_auth ", $outFile);

   system("$ssltest -server_auth -client_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2/sslv3 with both client and server authentication:");
   log_output("$ssltest -server_auth -client_auth", $outFile);

   system("ssltest -bio_pair -ssl2 (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2 via BIO pair:");
   log_output("ssltest -bio_pair -ssl2", $outFile);

   system("ssltest -bio_pair -dhe1024dsa -v (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2/sslv3 with 1024 bit DHE via BIO pair:");
   log_output("ssltest -bio_pair -dhe1024dsa -v", $outFile);

   system("$ssltest -bio_pair -ssl2 -server_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2 with server authentication via BIO pair:");
   log_output("$ssltest -bio_pair -ssl2 -server_auth", $outFile);

   system("$ssltest -bio_pair -ssl2 -client_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2 with client authentication via BIO pair:");
   log_output("$ssltest -bio_pair -ssl2 -client_auth", $outFile);

   system("$ssltest -bio_pair -ssl2 -server_auth -client_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2 with both client and server authentication via BIO pair:");
   log_output("$ssltest -bio_pair -ssl2 -server_auth -client_auth", $outFile);

   system("ssltest -bio_pair -ssl3 (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv3 via BIO pair:");
   log_output("ssltest -bio_pair -ssl3", $outFile);

   system("$ssltest -bio_pair -ssl3 -server_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv3 with server authentication via BIO pair:");
   log_output("$ssltest -bio_pair -ssl3 -server_auth", $outFile);

   system("$ssltest -bio_pair -ssl3 -client_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv3 with client authentication  via BIO pair:");
   log_output("$ssltest -bio_pair -ssl3 -client_auth", $outFile);

   system("$ssltest -bio_pair -ssl3 -server_auth -client_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv3 with both client and server authentication via BIO pair:");
   log_output("$ssltest -bio_pair -ssl3 -server_auth -client_auth", $outFile);

   system("ssltest -bio_pair (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2/sslv3 via BIO pair:");
   log_output("ssltest -bio_pair", $outFile);

   system("$ssltest -bio_pair -server_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2/sslv3 with server authentication via BIO pair:");
   log_output("$ssltest -bio_pair -server_auth", $outFile);

   system("$ssltest -bio_pair -client_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2/sslv3 with client authentication via BIO pair:");
   log_output("$ssltest -bio_pair -client_auth", $outFile);

   system("$ssltest -bio_pair -server_auth -client_auth (CLIB_OPT)/>$outFile");
   log_desc("Testing sslv2/sslv3 with both client and server authentication via BIO pair:");
   log_output("$ssltest -bio_pair -server_auth -client_auth", $outFile);
}


############################################################################
sub ca_tests
{
   my $outFile = "$output_path\\ca_tst.out";

   my($CAkey)     = "$output_path\\keyCA.ss";
   my($CAcert)    = "$output_path\\certCA.ss";
   my($CAserial)  = "$output_path\\certCA.srl";
   my($CAreq)     = "$output_path\\reqCA.ss";
   my($CAreq2)    = "$output_path\\req2CA.ss";

   my($CAconf)    = "$test_path\\CAss.cnf";

   my($Uconf)     = "$test_path\\Uss.cnf";

   my($Ukey)      = "$output_path\\keyU.ss";
   my($Ureq)      = "$output_path\\reqU.ss";
   my($Ucert)     = "$output_path\\certU.ss";

   print( "\nRUNNING CA TESTS:\n\n");

   print( OUT "\n========================================================\n");
   print( OUT "CA TESTS:\n");

   system("openssl2 req -config $CAconf -out $CAreq -keyout $CAkey -new (CLIB_OPT)/>$outFile");
   log_desc("Make a certificate request using req:");
   log_output("openssl2 req -config $CAconf -out $CAreq -keyout $CAkey -new", $outFile);

   system("openssl2 x509 -CAcreateserial -in $CAreq -days 30 -req -out $CAcert -signkey $CAkey (CLIB_OPT)/>$outFile");
   log_desc("Convert the certificate request into a self signed certificate using x509:");
   log_output("openssl2 x509 -CAcreateserial -in $CAreq -days 30 -req -out $CAcert -signkey $CAkey", $outFile);

   system("openssl2 x509 -in $CAcert -x509toreq -signkey $CAkey -out $CAreq2 (CLIB_OPT)/>$outFile");
   log_desc("Convert a certificate into a certificate request using 'x509':");
   log_output("openssl2 x509 -in $CAcert -x509toreq -signkey $CAkey -out $CAreq2", $outFile);

   system("openssl2 req -config $OpenSSL_config -verify -in $CAreq -noout (CLIB_OPT)/>$outFile");
   log_output("openssl2 req -config $OpenSSL_config -verify -in $CAreq -noout", $outFile);

   system("openssl2 req -config $OpenSSL_config -verify -in $CAreq2 -noout (CLIB_OPT)/>$outFile");
   log_output( "openssl2 req -config $OpenSSL_config -verify -in $CAreq2 -noout", $outFile);

   system("openssl2 verify -CAfile $CAcert $CAcert (CLIB_OPT)/>$outFile");
   log_output("openssl2 verify -CAfile $CAcert $CAcert", $outFile);

   system("openssl2 req -config $Uconf -out $Ureq -keyout $Ukey -new (CLIB_OPT)/>$outFile");
   log_desc("Make another certificate request using req:");
   log_output("openssl2 req -config $Uconf -out $Ureq -keyout $Ukey -new", $outFile);

   system("openssl2 x509 -CAcreateserial -in $Ureq -days 30 -req -out $Ucert -CA $CAcert -CAkey $CAkey -CAserial $CAserial (CLIB_OPT)/>$outFile");
   log_desc("Sign certificate request with the just created CA via x509:");
   log_output("openssl2 x509 -CAcreateserial -in $Ureq -days 30 -req -out $Ucert -CA $CAcert -CAkey $CAkey -CAserial $CAserial", $outFile);

   system("openssl2 verify -CAfile $CAcert $Ucert (CLIB_OPT)/>$outFile");
   log_output("openssl2 verify -CAfile $CAcert $Ucert", $outFile);

   system("openssl2 x509 -subject -issuer -startdate -enddate -noout -in $Ucert (CLIB_OPT)/>$outFile");
   log_desc("Certificate details");
   log_output("openssl2 x509 -subject -issuer -startdate -enddate -noout -in $Ucert", $outFile);

   print(OUT "--\n");
   print(OUT "The generated CA certificate is $CAcert\n");
   print(OUT "The generated CA private key is $CAkey\n");
   print(OUT "The current CA signing serial number is in $CAserial\n");

   print(OUT "The generated user certificate is $Ucert\n");
   print(OUT "The generated user private key is $Ukey\n");
   print(OUT "--\n");
}

############################################################################
sub evp_tests
{
   my $i = 'evp_test';

   print( "\nRUNNING EVP TESTS:\n\n");

   print( OUT "\n========================================================\n");
   print( OUT "EVP TESTS:\n\n");

   if (-e "$base_path\\$i.nlm")
   {
       my $outFile = "$output_path\\$i.out";
       system("$i $test_path\\evptests.txt (CLIB_OPT)/>$outFile");
       log_desc("Test: $i\.nlm:");
       log_output("", $outFile );
   }
   else
   {
       log_desc("Test: $i\.nlm: file not found");
   }
}

############################################################################
sub log_output( $ $ )
{
   my( $desc, $file ) = @_;
   my($error) = 0;
   my($key);
   my($msg);

   if ($desc)
   {
      print("\r$desc\n");
      print(OUT "$desc\n");
   }

      # loop waiting for test program to complete
   while ( stat($file) == 0)
      { print(". "); sleep(1); }


      # copy test output to log file
   open(IN, "<$file");
   while (<IN>)
   {
      print(OUT $_);
      if ( $_ =~ /ERROR/ )
      {
         $error = 1;
      }
   }
      # close and delete the temporary test output file
   close(IN);
   unlink($file);

   if ( $error == 0 )
   {
      $msg = "Test Succeeded";
   }
   else
   {
      $msg = "Test Failed";
   }

   print(OUT "$msg\n");

   if ($pause)
   {
      print("$msg - press ENTER to continue...");
      $key = getc;
      print("\n");
   }

      # Several of the testing scripts run a loop loading the
      # same NLM with different options.
      # On slow NetWare machines there appears to be some delay in the
      # OS actually unloading the test nlms and the OS complains about.
      # the NLM already being loaded.  This additional pause is to
      # to help provide a little more time for unloading before trying to
      # load again.
   sleep(1);
}


############################################################################
sub log_desc( $ )
{
   my( $desc ) = @_;

   print("\n");
   print("$desc\n");

   print(OUT "\n");
   print(OUT "$desc\n");
   print(OUT "======================================\n");
}

############################################################################
sub compare_files( $ $ $ )
{
   my( $file1, $file2, $binary ) = @_;
   my( $n1, $n2, $b1, $b2 );
   my($ret) = 1;

   open(IN0, $file1) || die "\nunable to open $file1\n";
   open(IN1, $file2) || die "\nunable to open $file2\n";

  if ($binary)
  {
      binmode IN0;
      binmode IN1;
  }

   for (;;)
   {
      $n1 = read(IN0, $b1, 512);
      $n2 = read(IN1, $b2, 512);

      if ($n1 != $n2) {last;}
      if ($b1 != $b2) {last;}

      if ($n1 == 0)
      {
         $ret = 0;
         last;
      }
   }
   close(IN0);
   close(IN1);
   return($ret);
}

############################################################################
sub do_wait()
{
   my($key);

   if ($pause)
   {
      print("Press ENTER to continue...");
      $key = getc;
      print("\n");
   }
}


############################################################################
sub make_tmp_cert_file()
{
   my @cert_files = <$cert_path/*.pem>;

      # delete the file if it already exists
   unlink($tmp_cert);

   open( TMP_CERT, ">$tmp_cert") || die "\nunable to open $tmp_cert\n";

   print("building temporary cert file\n");

   # create a temporary cert file that contains all the certs
   foreach $i (@cert_files)
   {
      open( IN_CERT, $i ) || die "\nunable to open $i\n";

      for(;;)
      {
         $n = sysread(IN_CERT, $data, 1024);

         if ($n == 0)
         {
            close(IN_CERT);
            last;
         };

         syswrite(TMP_CERT, $data, $n);
      }
   }

   close( TMP_CERT );
}
