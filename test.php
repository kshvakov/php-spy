<?php

function foo() {
	return "bar";
}

function test() {
	echo "test";
}

function test2() {
	$bar = "foo";

	echo $bar . foo() . PHP_EOL;
}

$a = 2;
$b = 3;

if ($a > 4) {
	$c = 5;
} else {
	$c = 10;
}

var_dump(test2());
var_dump($a, $b, $c);
test2();


require_once 'test2.php';
baz();

require_once 'test3.php';
baz2();

