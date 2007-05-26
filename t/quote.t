use t::TestYAML tests => 3;

use YAML::LibYAML;
use Data::Dumper;

is Dump('', [''], {foo => ''}), <<'...', 'Dumped empty string is quoted';
--- ''
---
- ''
---
foo: ''
...

is Dump({}, [{}], {foo => {}}), <<'...', 'Dumped empty map is {}';
--- {}
---
- {}
---
foo: {}
...

is Dump([], [[]], {foo => []}), <<'...', 'Dumped empty seq is []';
--- []
---
- []
---
foo: []
...
