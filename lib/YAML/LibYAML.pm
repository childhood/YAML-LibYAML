# ToDo:
#
# + Find out how to loop over all the args passed to a C function
# + Find out how to write C macros
# + Support ~, null, true, false
# - Rewrite documentation
# - Copy all relevant code from YAML::Syck
#
# Types:
# + Support blessed objects
# + Scalar refs
# - Globs
# - Regexps
#
# Tests:
# - http://svn.ali.as/cpan/concept/cpan-yaml-tiny/
#
# Profiling:
# - TonyC: sprof if I can remember the way to enable shared library profiling
# - TonyC: LD_PROFILE, but that may not work on OS X
# - TonyC: sample or Sampler.app on OS X, I'd guess
package YAML::LibYAML;
use 5.008003;
use strict;
use warnings;
our $VERSION = '0.15';
use base 'Exporter';

our @EXPORT = qw(Load Dump);

use YAML::LibYAML::XS qw(Load Dump);

1;

=head1 NAME

YAML::LibYAML - LibYAML bindings for Perl

=head1 SYNOPSIS

    use YAML::LibYAML;

    my $yaml = Dump [ 1..4 ];
    my $array = Load $yaml;

=head1 DESCRIPTION

Kirill Siminov's C<libyaml> is arguably the best YAML implementation.
The C library is written precisely to the YAML 1.1 specification. It was
originally bound to Python and was later bound to Ruby.

This module is a Perl XS binding to libyaml which will (eventually)
offer Perl the best YAML support to date.

This module exports the functions C<Dump> and C<Load>. These functions
are intended to work exactly like C<YAML.pm>'s corresponding functions.

SUPPORTED:

  * Unblessed hashes
  * Unblessed arrays
  * Unblessed scalars
  * Duplicate hash/array refs
  * Circular refs
  * Scalar refs
  * Empty Strings
  * Undef values
  * JSON true/false roundtripping
  * Blessed stuff
  * Code refs

UNSUPPORTED:

  * Typeglobs 
  * Regexps
  * File handles (IO refs)

YAML::LibYAML currently serializes these things as good as most
serializers, but YAML.pm goes farther and dumps these specially.
YAML::LibYAML does not yet dump these fully.

This work should progress quickly so check back often.

=head1 BUGS

There are no known bugs in the libyaml C library yet. The code is nearly
2 years old.

The YAML::LibYAML::XS binding is new. Bugs are likely. Please
report all bugs.

=head1 SEE ALSO

 * YAML.pm
 * YAML::Syck
 * YAML::Tiny
 * YAML::Tests

=head1 AUTHOR

Ingy döt Net <ingy@cpan.org>

=head1 COPYRIGHT

Copyright (c) 2007. Ingy döt Net. All rights reserved.

This program is free software; you can redistribute it and/or modify it
under the same terms as Perl itself.

See http://www.perl.com/perl/misc/Artistic.html

=cut
