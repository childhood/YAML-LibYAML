use t::TestYAMLTests tests => 6;

my $yaml = <<'...';
---
foo: !!perl/hash:Foo::Bar {}
bar: !!perl/hash:Foo::Bar
  bass: bawl
one: !!perl/array:BigList []
two: !!perl/array:BigList
- lola
- alol
...

my $hash = Load($yaml);

my $hash2 = {
    foo => (bless {}, "Foo::Bar"),
    bar => (bless {bass => 'bawl'}, "Foo::Bar"),
    one => (bless [], "BigList"),
    two => (bless [lola => 'alol'], "BigList"),
};

# is_deeply is broken and doesn't check blessings
is_deeply $hash, $hash2, "Load Blessed hashes and arrays";

is ref($hash2->{foo}), 'Foo::Bar',
    "Object at 'foo' is blessed 'Foo::Bar'";
is ref($hash2->{bar}), 'Foo::Bar',
    "Object at 'bar' is blessed 'Foo::Bar'";
is ref($hash2->{one}), 'BigList',
    "Object at 'one' is blessed 'BigList'";
is ref($hash2->{two}), 'BigList',
    "Object at 'two' is blessed 'BigList'";

my $yaml2 = Dump($hash2);

is $yaml2, <<'...', "Dumping blessed hashes and arrays works";
---
bar: !!perl/hash:Foo::Bar
  bass: bawl
foo: !!perl/hash:Foo::Bar {}
one: !!perl/array:BigList []
two: !!perl/array:BigList
- lola
- alol
...
