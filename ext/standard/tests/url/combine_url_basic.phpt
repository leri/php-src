--TEST--
Test parse_url() function : check values of URL related constants 
--FILE--
<?php
$parts = [
	'scheme' => 'http',
	'host' => 'example.com',
	'port' => 123,
	'user' => 'user',
	'pass' => 'pass',
	'path' => '/foo/bar/baz',
	'fragment' => 'hash',
	'query' => ['foo' => 'bar baz']
];

echo combine_url($parts)."\n";
echo combine_url($parts, PHP_URL_IPv4, PHP_QUERY_RFC3986)."\n";

$parts['host'] = '2607:f0d0:1002:0051:0000:0000:0000:0004';
echo combine_url($parts, PHP_URL_IPv6);
?>

--EXPECTF--
http://user:pass@example.com:123/foo/bar/baz?foo=bar+baz#hash
http://user:pass@example.com:123/foo/bar/baz?foo=bar%20baz#hash
http://user:pass@[2607:f0d0:1002:0051:0000:0000:0000:0004]:123/foo/bar/baz?foo=bar+baz#hash
