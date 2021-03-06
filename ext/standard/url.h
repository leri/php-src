/*
   +----------------------------------------------------------------------+
   | PHP Version 5                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2014 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Jim Winstead <jimw@php.net>                                  |
   +----------------------------------------------------------------------+
 */
/* $Id$ */

#ifndef URL_H
#define URL_H

#include "php_smart_str.h"

typedef struct php_url {
	char *scheme;
	char *user;
	char *pass;
	char *host;
	unsigned short port;
	char *path;
	char *query;
	char *fragment;
} php_url;

PHPAPI void php_url_free(php_url *theurl);
PHPAPI php_url *php_url_parse(char const *str);
PHPAPI php_url *php_url_parse_ex(char const *str, int length);
PHPAPI int php_combine_url(smart_str *res, char *scheme, char *host, long port, char *user, char *pass, char *path, HashTable *query, char *fragment, int ip_version, long enc_type);
PHPAPI int php_url_decode(char *str, int len); /* return value: length of decoded string */
PHPAPI int php_raw_url_decode(char *str, int len); /* return value: length of decoded string */
PHPAPI char *php_url_encode(char const *s, int len, int *new_length);
PHPAPI char *php_raw_url_encode(char const *s, int len, int *new_length);
PHPAPI char *php_url_encode_by_type(char const *s, int len, int *new_length, long enc_type);
PHPAPI char *php_url_get_encoded_property(zval *instance, const char *name, int name_length, int *new_length);

PHP_FUNCTION(parse_url);
PHP_FUNCTION(combine_url);
PHP_FUNCTION(urlencode);
PHP_FUNCTION(urldecode);
PHP_FUNCTION(rawurlencode);
PHP_FUNCTION(rawurldecode);
PHP_FUNCTION(get_headers);

zend_class_entry *url_ce;

PHP_MINIT_FUNCTION(url);
PHP_METHOD(Url, __toString);
PHP_METHOD(Url, getEncType);
PHP_METHOD(Url, getIpVersion);
PHP_METHOD(Url, getScheme);
PHP_METHOD(Url, getUser);
PHP_METHOD(Url, getRawUser);
PHP_METHOD(Url, getPass);
PHP_METHOD(Url, getRawPass);
PHP_METHOD(Url, getHost);
PHP_METHOD(Url, getPort);
PHP_METHOD(Url, getPath);
PHP_METHOD(Url, getRawPath);
PHP_METHOD(Url, getQueryString);
PHP_METHOD(Url, getQueryVariables);
PHP_METHOD(Url, getQueryVariable);
PHP_METHOD(Url, getFragment);
PHP_METHOD(Url, getRawFragment);
PHP_METHOD(Url, setEncType);
PHP_METHOD(Url, setIpVersion);
PHP_METHOD(Url, setScheme);
PHP_METHOD(Url, setUser);
PHP_METHOD(Url, setPass);
PHP_METHOD(Url, setHost);
PHP_METHOD(Url, setPort);
PHP_METHOD(Url, setPath);
PHP_METHOD(Url, setQuery);
PHP_METHOD(Url, setFragment);

#define PHP_URL_GET_ENCTYPE(ce, instance) Z_LVAL_P(zend_read_property(ce, instance, "encType", sizeof("enctype") - 1, 0 TSRMLS_CC))
	

#define PHP_URL_SCHEME 0
#define PHP_URL_HOST 1
#define PHP_URL_PORT 2
#define PHP_URL_USER 3
#define PHP_URL_PASS 4
#define PHP_URL_PATH 5
#define PHP_URL_QUERY 6
#define PHP_URL_FRAGMENT 7

#define PHP_QUERY_RFC1738 1
#define PHP_QUERY_RFC3986 2

#define PHP_URL_SCHEME_KEY "scheme"
#define PHP_URL_HOST_KEY "host"
#define PHP_URL_PORT_KEY "port"
#define PHP_URL_USER_KEY "user"
#define PHP_URL_PASS_KEY "pass"
#define PHP_URL_PATH_KEY "path"
#define PHP_URL_QUERY_KEY "query"
#define PHP_URL_FRAGMENT_KEY "fragment"

#define PHP_URL_IPv4 0
#define PHP_URL_IPv6 1


#endif /* URL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
