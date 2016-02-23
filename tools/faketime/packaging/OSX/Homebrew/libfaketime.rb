require 'formula'

class Libfaketime < Formula
  homepage 'http://www.code-wizards.com/projects/libfaketime'
  url 'http://code-wizards.com/projects/libfaketime/libfaketime-0.9.5.tar.gz'
  sha1 '12199af854004f231892ab6976c2e99b937e2d61'

  depends_on :macos => :lion

  fails_with :llvm do
    build 2336
    cause 'No thread local storage support'
  end

  def install
    system "make", "-C", "src", "-f", "Makefile.OSX", "PREFIX=#{prefix}"
    bin.install 'src/faketime'
    (lib/'faketime').install 'src/libfaketime.1.dylib'
    man1.install 'man/faketime.1'
  end
end

