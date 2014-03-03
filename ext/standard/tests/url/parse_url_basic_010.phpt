--TEST--
Test parse_url() function : check values of URL related constants 
--FILE--
<?php
/* Prototype  : proto mixed parse_url(string url, [int url_component])
 * Description: Parse a URL and return its components 
 * Source code: ext/standard/url.c
 * Alias to functions: 
 */

/*
 *  check values of URL related constants
 */
foreach(get_defined_constants() as $constantName => $constantValue) {
	if (strpos($constantName, 'PHP_URL')===0) {
		echo "$constantName: $constantValue \n";
	}
}

echo "Done";
?>
--EXPECTF--
PHP_URL_SCHEME: 0 
PHP_URL_HOST: 1 
PHP_URL_PORT: 2 
PHP_URL_USER: 3 
PHP_URL_PASS: 4 
PHP_URL_PATH: 5 
PHP_URL_QUERY: 6 
PHP_URL_FRAGMENT: 7 
PHP_URL_IPv4: 0 
PHP_URL_IPv6: 1 
PHP_URL_SCHEME_KEY: scheme 
PHP_URL_HOST_KEY: host 
PHP_URL_PORT_KEY: port 
PHP_URL_USER_KEY: user 
PHP_URL_PASS_KEY: pass 
PHP_URL_PATH_KEY: path 
PHP_URL_QUERY_KEY: query 
PHP_URL_FRAGMENT_KEY: fragment 
Done
