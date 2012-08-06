

                          ___       __ _ _ __ ___  ___ 
                         / __| ___ / _` | '__/ _ \/ __|
                        | (_  |___| (_| | | |  __/\__ \
                         \___|     \__,_|_|  \___||___/


                How to build c-ares using MSVC or Visual Studio
               =================================================



  How to build using MSVC from the command line
  ---------------------------------------------

  Open a command prompt window and ensure that the environment is properly
  set up in order to use MSVC or Visual Studio compiler tools.

  Change to c-ares source folder where Makefile.msvc file is located and run:

  > nmake -f Makefile.msvc

  This will build all c-ares libraries as well as three sample programs.

  Once the above command has finished a new folder named MSVCXX will exist
  below the folder where makefile.msvc is found. The name of the folder
  depends on the MSVC compiler version being used to build c-ares.

  Below the MSVCXX folder there will exist four folders named 'cares',
  'ahost', 'acountry', and 'adig'. The 'cares' folder is the one that
  holds the c-ares libraries you have just generated, the other three
  hold sample programs that use the libraries.

  The above command builds four versions of the c-ares library, dynamic
  and static versions and each one in release and debug flavours. Each
  of these is found in folders named dll-release, dll-debug, lib-release,
  and lib-debug, which hang from the 'cares' folder mentioned above. Each
  sample program also has folders with the same names to reflect which
  library version it is using.


  How to install using MSVC from the command line
  -----------------------------------------------

  In order to allow easy usage of c-ares libraries it may be convenient to
  install c-ares libraries and header files to a common subdirectory tree.

  Once that c-ares libraries have been built using procedure described above,
  use same command prompt window to define environment variable INSTALL_DIR
  to designate the top subdirectory where installation of c-ares libraries and
  header files will be done.

  > set INSTALL_DIR=c:\c-ares

  Afterwards, run following command to actually perform the installation:

  > nmake -f Makefile.msvc install

  Installation procedure will copy c-ares libraries to subdirectory 'lib' and
  c-ares header files to subdirectory 'include' below the INSTALL_DIR subdir.

  When environment variable INSTALL_DIR is not defined, installation is done
  to c-ares source folder where Makefile.msvc file is located.


  How to build using Visual Studio 6 IDE
  --------------------------------------

  A VC++ 6.0 reference workspace (vc6aws.dsw) is available within the 'vc'
  folder to allow proper building of the library and sample programs.

  1) Open the vc6aws.dsw workspace with MSVC6's IDE.
  2) Select 'Build' from top menu.
  3) Select 'Batch Build' from dropdown menu.
  4) Make sure that the sixteen project configurations are 'checked'.
  5) Click on the 'Build' button.
  6) Once the sixteen project configurations are built you are done.

  Dynamic and static c-ares libraries are built in debug and release flavours,
  and can be located each one in its own subdirectory, dll-debug, dll-release,
  lib-debug and lib-release, all of them below the 'vc\cares' subdirectory.

  In the same way four executable versions of each sample program are built,
  each using its respective library. The resulting sample executables are
  located in its own subdirectory, dll-debug, dll-release, lib-debug and
  lib-release, below the 'vc\acountry', 'vc\adig' and 'vc\ahost'folders.

  These reference VC++ 6.0 configurations are generated using the dynamic CRT.


  How to build using Visual Studio 2003 or newer IDE
  --------------------------------------------------

  First you have to convert the VC++ 6.0 reference workspace and project files
  to the Visual Studio IDE version you are using, following next steps:

  1) Open vc\vc6aws.dsw with VS20XX.
  2) Allow VS20XX to update all projects and workspaces.
  3) Save ALL and close VS20XX.
  4) Open vc\vc6aws.sln with VS20XX.
  5) Select batch build, check 'all' projects and click 'build' button.

  Same comments relative to generated files and folders as done above for
  Visual Studio 6 IDE apply here.


  Relationship between c-ares library file names and versions
  -----------------------------------------------------------

  c-ares static release library version files:

      libcares.lib -> static release library

  c-ares static debug library version files:

      libcaresd.lib -> static debug library

  c-ares dynamic release library version files:

      cares.dll -> dynamic release library
      cares.lib -> import library for the dynamic release library
      cares.exp -> export file for the dynamic release library

  c-ares dynamic debug library version files:

      caresd.dll -> dynamic debug library
      caresd.lib -> import library for the dynamic debug library
      caresd.exp -> export file for the dynamic debug library
      caresd.pdb -> debug symbol file for the dynamic debug library


  How to use c-ares static libraries
  ----------------------------------

  When using the c-ares static library in your program, you will have to
  define preprocessor symbol CARES_STATICLIB while building your program,
  otherwise you will get errors at linkage stage.


Have Fun!
 
