<?php

error_reporting(E_ALL | E_STRICT);

require_once('flatted.php');

class console {
  public static function assert($condition, $message) {
    if (!$condition) {
      echo $message."\n";
      exit(1);
    }
  }
}

console::assert(Flatted::stringify([null, null]) === '[[null,null]]', 'multiple null failed');

$a = array();
$o = new stdClass;

console::assert(Flatted::stringify($a) === '[[]]', 'empty Array');
console::assert(Flatted::stringify($o) === '[{}]', 'empty Object');

$a[] = &$a;
$o->o = &$o;

console::assert(Flatted::stringify($a) === '[["0"]]', 'recursive Array');
console::assert(Flatted::stringify($o) === '[{"o":"0"}]', 'recursive Object');

$b = Flatted::parse(Flatted::stringify($a));
console::assert(is_array($b) && $b[0] === $b, 'restoring recursive Array');

$a[] = 1;
$a[] = 'two';
$a[] = true;
$o->one = 1;
$o->two = 'two';
$o->three = true;

console::assert(Flatted::stringify($a) === '[["0",1,"1",true],"two"]', 'values in Array');
console::assert(Flatted::stringify($o) === '[{"o":"0","one":1,"two":"1","three":true},"two"]', 'values in Object');

$a[] = &$o;
$o->a = &$a;

console::assert(Flatted::stringify($a) === '[["0",1,"1",true,"2"],"two",{"o":"2","one":1,"two":"1","three":true,"a":"0"}]', 'object in Array');
console::assert(Flatted::stringify($o) === '[{"o":"0","one":1,"two":"1","three":true,"a":"2"},"two",["2",1,"1",true,"0"]]', 'array in Object');

$a[] = array('test' => 'OK');
$a[] = [1, 2, 3];

$o->test = array('test' => 'OK');
$o->array = [1, 2, 3];

console::assert(Flatted::stringify($a) === '[["0",1,"1",true,"2","3","4"],"two",{"o":"2","one":1,"two":"1","three":true,"a":"0","test":"3","array":"4"},{"test":"5"},[1,2,3],"OK"]', 'objects in Array');
console::assert(Flatted::stringify($o) === '[{"o":"0","one":1,"two":"1","three":true,"a":"2","test":"3","array":"4"},"two",["2",1,"1",true,"0","3","4"],{"test":"5"},[1,2,3],"OK"]', 'objects in Object');

$a2 = Flatted::parse(Flatted::stringify($a));
$o2 = Flatted::parse(Flatted::stringify($o));

console::assert($a2[0] === $a2, 'parsed Array');
console::assert($o2->o === $o2, 'parsed Object');

console::assert(
  $a2[1] === 1 &&
  $a2[2] === 'two' &&
  $a2[3] === true &&
  $a2[4] instanceof stdClass &&
  json_encode($a2[5]) === json_encode(array('test' => 'OK')) &&
  json_encode($a2[6]) === json_encode([1, 2, 3]),
  'array values are all OK'
);

console::assert($a2[4] === $a2[4]->o && $a2 === $a2[4]->o->a, 'array recursive values are OK');

console::assert(
  $o2->one === 1 &&
  $o2->two === 'two' &&
  $o2->three === true &&
  is_array($o2->a) &&
  json_encode($o2->test) === json_encode(array('test' => 'OK')) &&
  json_encode($o2->array) === json_encode([1, 2, 3]),
  'object values are all OK'
);

console::assert($o2->a === $o2->a[0] && $o2 === $o2->a[4], 'object recursive values are OK');

console::assert(Flatted::parse(Flatted::stringify(1)) === 1, 'numbers can be parsed too');
console::assert(Flatted::parse(Flatted::stringify(false)) === false, 'booleans can be parsed too');
console::assert(Flatted::parse(Flatted::stringify(null)) === null, 'null can be parsed too');
console::assert(Flatted::parse(Flatted::stringify('test')) === 'test', 'strings can be parsed too');

$str = Flatted::parse('[{"prop":"1","a":"2","b":"3"},{"value":123},["4","5"],{"e":"6","t":"7","p":4},{},{"b":"8"},"f",{"a":"9"},["10"],"sup",{"a":1,"d":2,"c":"7","z":"11","h":1},{"g":2,"a":"7","b":"12","f":6},{"r":4,"u":"7","c":5}]');

console::assert(
  $str->b->t->a === 'sup' &&
  $str->a[1]->b[0]->c === $str->b->t,
  'str is fine'
);

$oo = Flatted::parse('[{"a":"1","b":"0","c":"2"},{"aa":"3"},{"ca":"4","cb":"5","cc":"6","cd":"7","ce":"8","cf":"9"},{"aaa":"10"},{"caa":"4"},{"cba":"5"},{"cca":"2"},{"cda":"4"},"value2","value3","value1"]');

console::assert(
  $oo->a->aa->aaa = 'value1'
  && $oo === $oo->b
  && $oo === $oo->b
  && $oo->c->ca->caa === $oo->c->ca
  && $oo->c->cb->cba === $oo->c->cb
  && $oo->c->cc->cca === $oo->c
  && $oo->c->cd->cda === $oo->c->ca->caa
  && $oo->c->ce === 'value2'
  && $oo->c->cf === 'value3',
  'parse is correct'
);

echo "OK\n";

?>