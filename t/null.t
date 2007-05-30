use t::TestYAMLTests tests => 4;

my $array = [
    undef,
    'undef',
    undef,
    '~',
    undef,
    undef,
    '~/file.txt',
];

my $yaml = Dump($array);

is $yaml, <<'...', "Nulls dump as ~";
---
- ~
- undef
- ~
- '~'
- ~
- ~
- ~/file.txt
...

my $array2 = Load($yaml);

is_deeply $array2, $array,
    "YAML with undefs loads properly";

$yaml = "{foo, bar}\n";
my $perl = {foo => undef, bar => undef};

is_deeply Load($yaml), $perl,
    "Set notation has null values";

$yaml = <<'...';
---
foo:
bar:
-
- -
...
$perl = {foo => undef, bar => [undef, [undef]]};

is_deeply Load($yaml), $perl,
    "Empty values Load as undefs";
