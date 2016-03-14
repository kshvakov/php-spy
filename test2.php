<?php

function baz() {
	$n = 1;
	for ($i=0; $i < 10; $i++) {
		$n++;
	}
	return "bar" . $n;
}
