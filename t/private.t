use t::TestYAMLTests tests => 3;

my $a = Load(<<'...');
---
- !foo [1]
- !Bar::Bar {fa: la}
...

is ref($a), 'ARRAY', 'Load worked';
is ref($a->[0]), 'foo', 'Private tag works for array';
is ref($a->[1]), 'Bar::Bar', 'Private tag works for hash';
