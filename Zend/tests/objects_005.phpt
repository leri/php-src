--TEST--
method overloading with different method signature
--INI--
error_reporting=8191
--FILE--
<?php

class test {
	function &foo() {}
}

class test2 extends test {
	function &foo() {} 
}

class test3 extends test {
	function foo() {} 
}

echo "Done\n";
?>
--EXPECTF--
Fatal error: Declaration of test3::foo() must be compatible with that of test::foo() in %s on line %d
