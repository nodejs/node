
package Text::Template::Preprocess;
use Text::Template;
@ISA = qw(Text::Template);
$Text::Template::Preprocess::VERSION = 1.46;

sub fill_in {
  my $self = shift;
  my (%args) = @_;
  my $pp = $args{PREPROCESSOR} || $self->{PREPROCESSOR} ;
  if ($pp) {
    local $_ = $self->source();
#    print "# fill_in: before <$_>\n";
    &$pp;
#    print "# fill_in: after <$_>\n";
    $self->set_source_data($_);
  }
  $self->SUPER::fill_in(@_);
}

sub preprocessor {
  my ($self, $pp) = @_;
  my $old_pp = $self->{PREPROCESSOR};
  $self->{PREPROCESSOR} = $pp if @_ > 1;  # OK to pass $pp=undef
  $old_pp;
}

1;


=head1 NAME 

Text::Template::Preprocess - Expand template text with embedded Perl

=head1 VERSION

This file documents C<Text::Template::Preprocess> version B<1.46>

=head1 SYNOPSIS

 use Text::Template::Preprocess;

 my $t = Text::Template::Preprocess->new(...);  # identical to Text::Template

 # Fill in template, but preprocess each code fragment with pp().
 my $result = $t->fill_in(..., PREPROCESSOR => \&pp);

 my $old_pp = $t->preprocessor(\&new_pp);

=head1 DESCRIPTION

C<Text::Template::Preprocess> provides a new C<PREPROCESSOR> option to
C<fill_in>.  If the C<PREPROCESSOR> option is supplied, it must be a
reference to a preprocessor subroutine.  When filling out a template,
C<Text::Template::Preprocessor> will use this subroutine to preprocess
the program fragment prior to evaluating the code.

The preprocessor subroutine will be called repeatedly, once for each
program fragment.  The program fragment will be in C<$_>.  The
subroutine should modify the contents of C<$_> and return.
C<Text::Template::Preprocess> will then execute contents of C<$_> and
insert the result into the appropriate part of the template.

C<Text::Template::Preprocess> objects also support a utility method,
C<preprocessor()>, which sets a new preprocessor for the object.  This
preprocessor is used for all subsequent calls to C<fill_in> except
where overridden by an explicit C<PREPROCESSOR> option.
C<preprocessor()> returns the previous default preprocessor function,
or undefined if there wasn't one.  When invoked with no arguments,
C<preprocessor()> returns the object's current default preprocessor
function without changing it.

In all other respects, C<Text::Template::Preprocess> is identical to
C<Text::Template>.

=head1 WHY?

One possible purpose:  If your files contain a lot of JavaScript, like
this:


        Plain text here...
        { perl code }
        <script language=JavaScript>
     	      if (br== "n3") { 
	  	  // etc.
	      }
        </script>
        { more perl code }
        More plain text...

You don't want C<Text::Template> to confuse the curly braces in the
JavaScript program with executable Perl code.  One strategy:

        sub quote_scripts {
          s(<script(.*?)</script>)(q{$1})gsi;
        }

Then use C<PREPROCESSOR =E<gt> \&quote_scripts>.  This will transform 



=head1 SEE ALSO

L<Text::Template>

=head1 AUTHOR


Mark Jason Dominus, Plover Systems

Please send questions and other remarks about this software to
C<mjd-perl-template+@plover.com>

You can join a very low-volume (E<lt>10 messages per year) mailing
list for announcements about this package.  Send an empty note to
C<mjd-perl-template-request@plover.com> to join.

For updates, visit C<http://www.plover.com/~mjd/perl/Template/>.

=head1 LICENSE

    Text::Template::Preprocess version 1.46
    Copyright 2013 Mark Jason Dominus

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  You may also can
    redistribute it and/or modify it under the terms of the Perl
    Artistic License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received copies of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.


=cut

