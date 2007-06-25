use ExtUtils::MakeMaker;
use strict;
use Config;

my $obj_files = join ' ', map {
    my $c = $_;
    $c =~ s/\.c$/$Config::Config{_o}/;
    $c;
} glob("*.c"), 'LibYAML.c';

WriteMakefile(
    NAME => 'YAML::XS::LibYAML',
    PREREQ_PM => {},
    # CCFLAGS => '-ansi -pedantic -Wall',
    # CCFLAGS => '-ansi -Wall',
    # CCFLAGS => '-pedantic -Wall',
    # CCFLAGS => '-Wall',
    DEFINE => '-DHAVE_CONFIG_H',
    LIBS => [''], # e.g., '-lm'
    INC => '-I.',
    OBJECT => $obj_files,
    ABSTRACT_FROM => 'lib/YAML/XS/LibYAML.pm',
    AUTHOR => 'Ingy döt Net <ingy@cpan.org>',
);
