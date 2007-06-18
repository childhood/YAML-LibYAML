# ToDo:
#
# - Rewrite documentation
# - Copy all relevant code from YAML::Syck
#
# Types:
# - Blessed scalar refs
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
package YAML::XS;
use 5.008003;
use strict;
use warnings;
our $VERSION = '0.18';
use base 'Exporter';

our @EXPORT = qw(Load Dump);
# our $UseCode = 0;
# our $DumpCode = 0;
# our $LoadCode = 0;

use YAML::XS::LibYAML qw(Load Dump);

# XXX Figure out how to lazily load this module. 
# So far I've tried using the C function:
#      load_module(PERL_LOADMOD_NOIMPORT, newSVpv("B::Deparse", 0), NULL);
# But it didn't seem to work.
use B::Deparse;

# XXX The following code should be moved from Perl to C.
our $coderef2text = sub {
    my $coderef = shift;
    my $deparse = B::Deparse->new();
    my $text;
    eval {
        local $^W = 0;
        $text = $deparse->coderef2text($coderef);
    };
    if ($@) {
        warn "YAML::XS failed to dump code ref:\n$@";
        return;
    }

    return $text;
};

our $glob2hash = sub {
    my $hash = {};
    for my $type (qw(PACKAGE NAME SCALAR ARRAY HASH CODE IO)) {
        my $value = *{$_[0]}{$type};
        $value = $$value if $type eq 'SCALAR';
        if (defined $value) {
            if ($type eq 'IO') {
                my @stats = qw(device inode mode links uid gid rdev size
                               atime mtime ctime blksize blocks);
                undef $value;
                $value->{stat} = {};
                map {$value->{stat}{shift @stats} = $_} stat(*{$_[0]});
                $value->{fileno} = fileno(*{$_[0]});
                {
                    local $^W;
                    $value->{tell} = tell(*{$_[0]});
                }
            }
            $hash->{$type} = $value;
        }
    }
    return $hash;
};

1;

=head1 NAME

YAML::XS - Perl YAML implementation using XS and libyaml

=head1 SYNOPSIS

    use YAML::XS;

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
  * Typeglobs 
  * File handles (IO refs)

UNSUPPORTED:

  * Regexps

This work should progress quickly so check back often.

=head1 BUGS

There are no known bugs in the libyaml C library yet. The code is nearly
2 years old.

The YAML::XS::LibYAML binding is new. Bugs are likely. Please
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
