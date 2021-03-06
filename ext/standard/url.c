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

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

#include "php.h"
#include "php_variables.h"

#include "url.h"
#include "file.h"
#ifdef _OSD_POSIX
#ifndef APACHE
#error On this EBCDIC platform, PHP is only supported as an Apache module.
#else /*APACHE*/
#ifndef CHARSET_EBCDIC
#define CHARSET_EBCDIC /* this machine uses EBCDIC, not ASCII! */
#endif
#include "ebcdic.h"
#endif /*APACHE*/
#endif /*_OSD_POSIX*/

/* {{{ free_url
 */
PHPAPI void php_url_free(php_url *theurl)
{
	if (theurl->scheme)
		efree(theurl->scheme);
	if (theurl->user)
		efree(theurl->user);
	if (theurl->pass)
		efree(theurl->pass);
	if (theurl->host)
		efree(theurl->host);
	if (theurl->path)
		efree(theurl->path);
	if (theurl->query)
		efree(theurl->query);
	if (theurl->fragment)
		efree(theurl->fragment);
	efree(theurl);
}
/* }}} */

/* {{{ php_replace_controlchars
 */
PHPAPI char *php_replace_controlchars_ex(char *str, int len)
{
	unsigned char *s = (unsigned char *)str;
	unsigned char *e = (unsigned char *)str + len;
	
	if (!str) {
		return (NULL);
	}
	
	while (s < e) {
	    
		if (iscntrl(*s)) {
			*s='_';
		}	
		s++;
	}
	
	return (str);
} 
/* }}} */

PHPAPI char *php_replace_controlchars(char *str)
{
	return php_replace_controlchars_ex(str, strlen(str));
} 

PHPAPI php_url *php_url_parse(char const *str)
{
	return php_url_parse_ex(str, strlen(str));
}

/* {{{ php_url_parse
 */
PHPAPI php_url *php_url_parse_ex(char const *str, int length)
{
	char port_buf[6];
	php_url *ret = ecalloc(1, sizeof(php_url));
	char const *s, *e, *p, *pp, *ue;
		
	s = str;
	ue = s + length;

	/* parse scheme */
	if ((e = memchr(s, ':', length)) && (e - s)) {
		/* validate scheme */
		p = s;
		while (p < e) {
			/* scheme = 1*[ lowalpha | digit | "+" | "-" | "." ] */
			if (!isalpha(*p) && !isdigit(*p) && *p != '+' && *p != '.' && *p != '-') {
				if (e + 1 < ue) {
					goto parse_port;
				} else {
					goto just_path;
				}
			}
			p++;
		}
	
		if (*(e + 1) == '\0') { /* only scheme is available */
			ret->scheme = estrndup(s, (e - s));
			php_replace_controlchars_ex(ret->scheme, (e - s));
			goto end;
		}

		/* 
		 * certain schemas like mailto: and zlib: may not have any / after them
		 * this check ensures we support those.
		 */
		if (*(e+1) != '/') {
			/* check if the data we get is a port this allows us to 
			 * correctly parse things like a.com:80
			 */
			p = e + 1;
			while (isdigit(*p)) {
				p++;
			}
			
			if ((*p == '\0' || *p == '/') && (p - e) < 7) {
				goto parse_port;
			}
			
			ret->scheme = estrndup(s, (e-s));
			php_replace_controlchars_ex(ret->scheme, (e - s));
			
			length -= ++e - s;
			s = e;
			goto just_path;
		} else {
			ret->scheme = estrndup(s, (e-s));
			php_replace_controlchars_ex(ret->scheme, (e - s));
		
			if (*(e+2) == '/') {
				s = e + 3;
				if (!strncasecmp("file", ret->scheme, sizeof("file"))) {
					if (*(e + 3) == '/') {
						/* support windows drive letters as in:
						   file:///c:/somedir/file.txt
						*/
						if (*(e + 5) == ':') {
							s = e + 4;
						}
						goto nohost;
					}
				}
			} else {
				if (!strncasecmp("file", ret->scheme, sizeof("file"))) {
					s = e + 1;
					goto nohost;
				} else {
					length -= ++e - s;
					s = e;
					goto just_path;
				}	
			}
		}	
	} else if (e) { /* no scheme; starts with colon: look for port */
		parse_port:
		p = e + 1;
		pp = p;

		while (pp-p < 6 && isdigit(*pp)) {
			pp++;
		}

		if (pp - p > 0 && pp - p < 6 && (*pp == '/' || *pp == '\0')) {
			long port;
			memcpy(port_buf, p, (pp - p));
			port_buf[pp - p] = '\0';
			port = strtol(port_buf, NULL, 10);
			if (port > 0 && port <= 65535) {
				ret->port = (unsigned short) port;
			} else {
				STR_FREE(ret->scheme);
				efree(ret);
				return NULL;
			}
		} else if (p == pp && *pp == '\0') {
			STR_FREE(ret->scheme);
			efree(ret);
			return NULL;
		} else if (*s == '/' && *(s+1) == '/') { /* relative-scheme URL */
			s += 2;
		} else {
			goto just_path;
		}
	} else if (*s == '/' && *(s+1) == '/') { /* relative-scheme URL */
		s += 2;
	} else {
		just_path:
		ue = s + length;
		goto nohost;
	}
	
	e = ue;
	
	if (!(p = memchr(s, '/', (ue - s)))) {
		char *query, *fragment;

		query = memchr(s, '?', (ue - s));
		fragment = memchr(s, '#', (ue - s));

		if (query && fragment) {
			if (query > fragment) {
				e = fragment;
			} else {
				e = query;
			}
		} else if (query) {
			e = query;
		} else if (fragment) {
			e = fragment;
		}
	} else {
		e = p;
	}	
		
	/* check for login and password */
	if ((p = zend_memrchr(s, '@', (e-s)))) {
		if ((pp = memchr(s, ':', (p-s)))) {
			if ((pp-s) > 0) {
				ret->user = estrndup(s, (pp-s));
				php_replace_controlchars_ex(ret->user, (pp - s));
			}	
		
			pp++;
			if (p-pp > 0) {
				ret->pass = estrndup(pp, (p-pp));
				php_replace_controlchars_ex(ret->pass, (p-pp));
			}	
		} else {
			ret->user = estrndup(s, (p-s));
			php_replace_controlchars_ex(ret->user, (p-s));
		}
		
		s = p + 1;
	}

	/* check for port */
	if (*s == '[' && *(e-1) == ']') {
		/* Short circuit portscan, 
		   we're dealing with an 
		   IPv6 embedded address */
		p = s;
	} else {
		/* memrchr is a GNU specific extension
		   Emulate for wide compatibility */
		for(p = e; *p != ':' && p >= s; p--);
	}

	if (p >= s && *p == ':') {
		if (!ret->port) {
			p++;
			if (e-p > 5) { /* port cannot be longer then 5 characters */
				STR_FREE(ret->scheme);
				STR_FREE(ret->user);
				STR_FREE(ret->pass);
				efree(ret);
				return NULL;
			} else if (e - p > 0) {
				long port;
				memcpy(port_buf, p, (e - p));
				port_buf[e - p] = '\0';
				port = strtol(port_buf, NULL, 10);
				if (port > 0 && port <= 65535) {
					ret->port = (unsigned short)port;
				} else {
					STR_FREE(ret->scheme);
					STR_FREE(ret->user);
					STR_FREE(ret->pass);
					efree(ret);
					return NULL;
				}
			}
			p--;
		}	
	} else {
		p = e;
	}
	
	/* check if we have a valid host, if we don't reject the string as url */
	if ((p-s) < 1) {
		STR_FREE(ret->scheme);
		STR_FREE(ret->user);
		STR_FREE(ret->pass);
		efree(ret);
		return NULL;
	}

	ret->host = estrndup(s, (p-s));
	php_replace_controlchars_ex(ret->host, (p - s));
	
	if (e == ue) {
		return ret;
	}
	
	s = e;
	
	nohost:
	
	if ((p = memchr(s, '?', (ue - s)))) {
		pp = strchr(s, '#');

		if (pp && pp < p) {
			if (pp - s) {
				ret->path = estrndup(s, (pp-s));
				php_replace_controlchars_ex(ret->path, (pp - s));
			}
			p = pp;
			goto label_parse;
		}
	
		if (p - s) {
			ret->path = estrndup(s, (p-s));
			php_replace_controlchars_ex(ret->path, (p - s));
		}	
	
		if (pp) {
			if (pp - ++p) { 
				ret->query = estrndup(p, (pp-p));
				php_replace_controlchars_ex(ret->query, (pp - p));
			}
			p = pp;
			goto label_parse;
		} else if (++p - ue) {
			ret->query = estrndup(p, (ue-p));
			php_replace_controlchars_ex(ret->query, (ue - p));
		}
	} else if ((p = memchr(s, '#', (ue - s)))) {
		if (p - s) {
			ret->path = estrndup(s, (p-s));
			php_replace_controlchars_ex(ret->path, (p - s));
		}	
		
		label_parse:
		p++;
		
		if (ue - p) {
			ret->fragment = estrndup(p, (ue-p));
			php_replace_controlchars_ex(ret->fragment, (ue - p));
		}	
	} else {
		ret->path = estrndup(s, (ue-s));
		php_replace_controlchars_ex(ret->path, (ue - s));
	}
end:
	return ret;
}
/* }}} */

/* {{{ proto mixed parse_url(string url, [int url_component])
   Parse a URL and return its components */
PHP_FUNCTION(parse_url)
{
	char *str;
	int str_len;
	php_url *resource;
	long key = -1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &str, &str_len, &key) == FAILURE) {
		return;
	}

	resource = php_url_parse_ex(str, str_len);
	if (resource == NULL) {
		/* @todo Find a method to determine why php_url_parse_ex() failed */
		RETURN_FALSE;
	}

	if (key > -1) {
		switch (key) {
			case PHP_URL_SCHEME:
				if (resource->scheme != NULL) RETVAL_STRING(resource->scheme, 1);
				break;
			case PHP_URL_HOST:
				if (resource->host != NULL) RETVAL_STRING(resource->host, 1);
				break;
			case PHP_URL_PORT:
				if (resource->port != 0) RETVAL_LONG(resource->port);
				break;
			case PHP_URL_USER:
				if (resource->user != NULL) RETVAL_STRING(resource->user, 1);
				break;
			case PHP_URL_PASS:
				if (resource->pass != NULL) RETVAL_STRING(resource->pass, 1);
				break;
			case PHP_URL_PATH:
				if (resource->path != NULL) RETVAL_STRING(resource->path, 1);
				break;
			case PHP_URL_QUERY:
				if (resource->query != NULL) RETVAL_STRING(resource->query, 1);
				break;
			case PHP_URL_FRAGMENT:
				if (resource->fragment != NULL) RETVAL_STRING(resource->fragment, 1);
				break;
			default:
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid URL component identifier %ld", key);
				RETVAL_FALSE;
		}
		goto done;
	}

	/* allocate an array for return */
	array_init(return_value);

    /* add the various elements to the array */
	if (resource->scheme != NULL)
		add_assoc_string(return_value, PHP_URL_SCHEME_KEY, resource->scheme, 1);
	if (resource->host != NULL)
		add_assoc_string(return_value, PHP_URL_HOST_KEY, resource->host, 1);
	if (resource->port != 0)
		add_assoc_long(return_value, PHP_URL_PORT_KEY, resource->port);
	if (resource->user != NULL)
		add_assoc_string(return_value, PHP_URL_USER_KEY, resource->user, 1);
	if (resource->pass != NULL)
		add_assoc_string(return_value, PHP_URL_PASS_KEY, resource->pass, 1);
	if (resource->path != NULL)
		add_assoc_string(return_value, PHP_URL_PATH_KEY, resource->path, 1);
	if (resource->query != NULL)
		add_assoc_string(return_value, PHP_URL_QUERY_KEY, resource->query, 1);
	if (resource->fragment != NULL)
		add_assoc_string(return_value, PHP_URL_FRAGMENT_KEY, resource->fragment, 1);
done:	
	php_url_free(resource);
}
/* }}} */

static int php_url_get_str_part(char **part, char *key, long key_len, HashTable *parts) {
	zval **zv_part;

	if (zend_hash_find(parts, key, key_len, (void**)&zv_part) == SUCCESS) {
		if (Z_TYPE_PP(zv_part) != IS_STRING) {
			zend_error(E_WARNING, "%s should be string.", key);
			return FAILURE;
		}

		*part = Z_STRVAL_PP(zv_part);
	} else {
		*part = NULL;
	}

	return SUCCESS;
}

/* {{{ php_combine_url
*/
PHPAPI int php_combine_url(smart_str *res, char *scheme, char *host, long port, char *user, char *pass, char *path, HashTable *query, char *fragment, int ip_version, long enc_type) {
	if (host != NULL) {
		if (scheme != NULL) { // TODO validate scheme for validity.
			smart_str_appendl(res, scheme, strlen(scheme));
			smart_str_appendc(res, ':');
		}

		smart_str_appendl(res, "//", strlen("//"));

		if (user != NULL) {
			int user_len;
			user = php_url_encode_by_type(user, strlen(user), &user_len, enc_type);
			smart_str_appendl(res, user, user_len);

			if (pass != NULL) {
				smart_str_appendc(res, ':');
				int pass_len;
				pass = php_url_encode_by_type(pass, strlen(pass), &pass_len, enc_type);
				smart_str_appendl(res, pass, pass_len);
			}

			smart_str_appendc(res, '@');
		}

		if (ip_version == PHP_URL_IPv6) {
			smart_str_appendc(res, '[');
		}

		smart_str_appendl(res, host, strlen(host));

		if (ip_version == PHP_URL_IPv6) {
			smart_str_appendc(res, ']');
		}

		if (port > -1) {
			smart_str_appendc(res, ':');
			char port_str[128]; // large enough for 64 bit numbers.
			sprintf(port_str, "%lu", port);
			smart_str_appendl(res, port_str, strlen(port_str));
		}
	}

	if (path != NULL) {
		char *path_tmp = estrdup(path);
		char *path_delimiter_str = "/";
		char path_delimiter = '/';
		char *path_part = strtok(path_tmp, path_delimiter_str);

		while (path_part != NULL) {
			smart_str_appendc(res, path_delimiter);

			int path_part_len;
			path_part = php_url_encode_by_type(path_part, strlen(path_part), &path_part_len, enc_type);
			smart_str_appendl(res, path_part, path_part_len);

			path_part = strtok(NULL, path_delimiter_str);
		}
		
		efree(path_tmp);
	}

	if (query != NULL) {
		smart_str_appendc(res, '?');

		smart_str query_res = {0};

		if (php_url_encode_hash_ex(query, &query_res, NULL, 0, NULL, 0, NULL, 0, NULL, NULL, enc_type TSRMLS_CC) == FAILURE) {
			if (query_res.c) {
				efree(query_res.c);
			}
			return FAILURE;
		}

		smart_str_append(res, &query_res);
	}

	if (fragment != NULL) {
		smart_str_appendc(res, '#');
		int fragment_len;
		fragment = php_url_encode_by_type(fragment, strlen(fragment), &fragment_len, enc_type);
		smart_str_appendl(res, fragment, fragment_len);
	}

	return SUCCESS;
}
/* }}} */

/* {{{ proto mixed parse_url(string url, [int ip_version, int enc_type])
   Combines URL parts and returns valid url */
PHP_FUNCTION(combine_url) {
	HashTable *parts;
	long ip_version = PHP_URL_IPv4;
	long enc_type = PHP_QUERY_RFC1738;
	zval **zv_port;
	zval **zv_query;
	char *scheme;
	char *host;
	long port = -1;
	char *user;
	char *pass;
	char *path;
	HashTable *query = NULL;
	char *fragment;
	smart_str res = {0};

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "h|ll", &parts, &ip_version, &enc_type) == FAILURE) {
		RETURN_FALSE;
	}

	if (php_url_get_str_part(&scheme, PHP_URL_SCHEME_KEY, sizeof(PHP_URL_SCHEME_KEY), parts) == FAILURE) {
		RETURN_FALSE;
	}

	if (php_url_get_str_part(&host, PHP_URL_HOST_KEY, sizeof(PHP_URL_HOST_KEY), parts) == FAILURE) {
		RETURN_FALSE;
	}

	if (php_url_get_str_part(&user, PHP_URL_USER_KEY, sizeof(PHP_URL_USER_KEY), parts) == FAILURE) {
		RETURN_FALSE;
	}

	if (php_url_get_str_part(&pass, PHP_URL_PASS_KEY, sizeof(PHP_URL_PASS_KEY), parts) == FAILURE) {
		RETURN_FALSE;
	}

	if (php_url_get_str_part(&path, PHP_URL_PATH_KEY, sizeof(PHP_URL_PATH_KEY), parts) == FAILURE) {
		RETURN_FALSE;
	}

	if (php_url_get_str_part(&fragment, PHP_URL_FRAGMENT_KEY, sizeof(PHP_URL_FRAGMENT_KEY), parts) == FAILURE) {
		RETURN_FALSE;
	}

	if (zend_hash_find(parts, PHP_URL_PORT_KEY, sizeof(PHP_URL_PORT_KEY), (void**)&zv_port) == SUCCESS) {
		zval tmp_port;
		ZVAL_COPY_VALUE(&tmp_port, *zv_port);
		zval_copy_ctor(&tmp_port);
		convert_to_long(&tmp_port);
		port = Z_LVAL(tmp_port);
		zval_dtor(&tmp_port);
	}

	if (zend_hash_find(parts, PHP_URL_QUERY_KEY, sizeof(PHP_URL_QUERY_KEY), (void**)&zv_query) == SUCCESS) {
		switch (Z_TYPE_PP(zv_query)) {
			case IS_STRING: ;
				zval tmp;
				char *query_str = estrdup(Z_STRVAL_PP(zv_query));
				array_init(&tmp);
				sapi_module.treat_data(PARSE_STRING, query_str, &tmp TSRMLS_CC);
				query = HASH_OF(&tmp);
				break;
			case IS_ARRAY:
			case IS_OBJECT:
				query = HASH_OF(*zv_query);
				break;
			default:
				zend_error(E_WARNING, "Invalid type passed for query");
				break;
		}
	}

	if (php_combine_url(&res, scheme, host, port, user, pass, path, query, fragment, ip_version, enc_type) == FAILURE) {
		if (res.c) {
			efree(res.c);
		}
		RETURN_FALSE;
	}

	smart_str_0(&res);

	RETURN_STRINGL(res.c, res.len, 0);
}
/* }}} */

/* {{{ php_url_encode_by_type
*/
PHPAPI char *php_url_encode_by_type(char const *s, int len, int *new_length, long enc_type) {
	switch (enc_type) {
		case PHP_QUERY_RFC1738:
			return php_url_encode(s, len, new_length);
		case PHP_QUERY_RFC3986:
			return php_raw_url_encode(s, len, new_length);
	}

	return NULL;
}
/* }}} */

/** {{{ php_url_get_encoded_property
*/
PHPAPI char *php_url_get_encoded_property(zval *instance, const char *name, int name_length, int *new_length) {
	zval *zv_prop;
	char *prop;
	long encType = PHP_URL_GET_ENCTYPE(url_ce, instance);

	zv_prop = zend_read_property(url_ce, instance, name, name_length, 1 TSRMLS_CC);

	if (!zv_prop || Z_TYPE_P(zv_prop) != IS_STRING) {
		return NULL;
	}

	prop = Z_STRVAL_P(zv_prop);

	return php_url_encode_by_type(prop, strlen(prop), new_length, encType);
}
/* }}} */

/* {{{ php_htoi
 */
static int php_htoi(char *s)
{
	int value;
	int c;

	c = ((unsigned char *)s)[0];
	if (isupper(c))
		c = tolower(c);
	value = (c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10) * 16;

	c = ((unsigned char *)s)[1];
	if (isupper(c))
		c = tolower(c);
	value += c >= '0' && c <= '9' ? c - '0' : c - 'a' + 10;

	return (value);
}
/* }}} */

/* rfc1738:

   ...The characters ";",
   "/", "?", ":", "@", "=" and "&" are the characters which may be
   reserved for special meaning within a scheme...

   ...Thus, only alphanumerics, the special characters "$-_.+!*'(),", and
   reserved characters used for their reserved purposes may be used
   unencoded within a URL...

   For added safety, we only leave -_. unencoded.
 */

static unsigned char hexchars[] = "0123456789ABCDEF";

/* {{{ php_url_encode
 */
PHPAPI char *php_url_encode(char const *s, int len, int *new_length)
{
	register unsigned char c;
	unsigned char *to, *start;
	unsigned char const *from, *end;
	
	from = (unsigned char *)s;
	end = (unsigned char *)s + len;
	start = to = (unsigned char *) safe_emalloc(3, len, 1);

	while (from < end) {
		c = *from++;

		if (c == ' ') {
			*to++ = '+';
#ifndef CHARSET_EBCDIC
		} else if ((c < '0' && c != '-' && c != '.') ||
				   (c < 'A' && c > '9') ||
				   (c > 'Z' && c < 'a' && c != '_') ||
				   (c > 'z')) {
			to[0] = '%';
			to[1] = hexchars[c >> 4];
			to[2] = hexchars[c & 15];
			to += 3;
#else /*CHARSET_EBCDIC*/
		} else if (!isalnum(c) && strchr("_-.", c) == NULL) {
			/* Allow only alphanumeric chars and '_', '-', '.'; escape the rest */
			to[0] = '%';
			to[1] = hexchars[os_toascii[c] >> 4];
			to[2] = hexchars[os_toascii[c] & 15];
			to += 3;
#endif /*CHARSET_EBCDIC*/
		} else {
			*to++ = c;
		}
	}
	*to = 0;
	if (new_length) {
		*new_length = to - start;
	}
	return (char *) start;
}
/* }}} */

/* {{{ proto string urlencode(string str)
   URL-encodes string */
PHP_FUNCTION(urlencode)
{
	char *in_str, *out_str;
	int in_str_len, out_str_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &in_str,
							  &in_str_len) == FAILURE) {
		return;
	}

	out_str = php_url_encode(in_str, in_str_len, &out_str_len);
	RETURN_STRINGL(out_str, out_str_len, 0);
}
/* }}} */

/* {{{ proto string urldecode(string str)
   Decodes URL-encoded string */
PHP_FUNCTION(urldecode)
{
	char *in_str, *out_str;
	int in_str_len, out_str_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &in_str,
							  &in_str_len) == FAILURE) {
		return;
	}

	out_str = estrndup(in_str, in_str_len);
	out_str_len = php_url_decode(out_str, in_str_len);

    RETURN_STRINGL(out_str, out_str_len, 0);
}
/* }}} */

/* {{{ php_url_decode
 */
PHPAPI int php_url_decode(char *str, int len)
{
	char *dest = str;
	char *data = str;

	while (len--) {
		if (*data == '+') {
			*dest = ' ';
		}
		else if (*data == '%' && len >= 2 && isxdigit((int) *(data + 1)) 
				 && isxdigit((int) *(data + 2))) {
#ifndef CHARSET_EBCDIC
			*dest = (char) php_htoi(data + 1);
#else
			*dest = os_toebcdic[(char) php_htoi(data + 1)];
#endif
			data += 2;
			len -= 2;
		} else {
			*dest = *data;
		}
		data++;
		dest++;
	}
	*dest = '\0';
	return dest - str;
}
/* }}} */

/* {{{ php_raw_url_encode
 */
PHPAPI char *php_raw_url_encode(char const *s, int len, int *new_length)
{
	register int x, y;
	unsigned char *str;

	str = (unsigned char *) safe_emalloc(3, len, 1);
	for (x = 0, y = 0; len--; x++, y++) {
		str[y] = (unsigned char) s[x];
#ifndef CHARSET_EBCDIC
		if ((str[y] < '0' && str[y] != '-' && str[y] != '.') ||
			(str[y] < 'A' && str[y] > '9') ||
			(str[y] > 'Z' && str[y] < 'a' && str[y] != '_') ||
			(str[y] > 'z' && str[y] != '~')) {
			str[y++] = '%';
			str[y++] = hexchars[(unsigned char) s[x] >> 4];
			str[y] = hexchars[(unsigned char) s[x] & 15];
#else /*CHARSET_EBCDIC*/
		if (!isalnum(str[y]) && strchr("_-.~", str[y]) != NULL) {
			str[y++] = '%';
			str[y++] = hexchars[os_toascii[(unsigned char) s[x]] >> 4];
			str[y] = hexchars[os_toascii[(unsigned char) s[x]] & 15];
#endif /*CHARSET_EBCDIC*/
		}
	}
	str[y] = '\0';
	if (new_length) {
		*new_length = y;
	}
	return ((char *) str);
}
/* }}} */

/* {{{ proto string rawurlencode(string str)
   URL-encodes string */
PHP_FUNCTION(rawurlencode)
{
	char *in_str, *out_str;
	int in_str_len, out_str_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &in_str,
							  &in_str_len) == FAILURE) {
		return;
	}

	out_str = php_raw_url_encode(in_str, in_str_len, &out_str_len);
	RETURN_STRINGL(out_str, out_str_len, 0);
}
/* }}} */

/* {{{ proto string rawurldecode(string str)
   Decodes URL-encodes string */
PHP_FUNCTION(rawurldecode)
{
	char *in_str, *out_str;
	int in_str_len, out_str_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &in_str,
							  &in_str_len) == FAILURE) {
		return;
	}

	out_str = estrndup(in_str, in_str_len);
	out_str_len = php_raw_url_decode(out_str, in_str_len);

    RETURN_STRINGL(out_str, out_str_len, 0);
}
/* }}} */

/* {{{ php_raw_url_decode
 */
PHPAPI int php_raw_url_decode(char *str, int len)
{
	char *dest = str;
	char *data = str;

	while (len--) {
		if (*data == '%' && len >= 2 && isxdigit((int) *(data + 1)) 
			&& isxdigit((int) *(data + 2))) {
#ifndef CHARSET_EBCDIC
			*dest = (char) php_htoi(data + 1);
#else
			*dest = os_toebcdic[(char) php_htoi(data + 1)];
#endif
			data += 2;
			len -= 2;
		} else {
			*dest = *data;
		}
		data++;
		dest++;
	}
	*dest = '\0';
	return dest - str;
}
/* }}} */

/* {{{ proto array get_headers(string url[, int format])
   fetches all the headers sent by the server in response to a HTTP request */
PHP_FUNCTION(get_headers)
{
	char *url;
	int url_len;
	php_stream_context *context;
	php_stream *stream;
	zval **prev_val, **hdr = NULL, **h;
	HashPosition pos;
	HashTable *hashT;
	long format = 0;
                
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|l", &url, &url_len, &format) == FAILURE) {
		return;
	}
	context = FG(default_context) ? FG(default_context) : (FG(default_context) = php_stream_context_alloc(TSRMLS_C));

	if (!(stream = php_stream_open_wrapper_ex(url, "r", REPORT_ERRORS | STREAM_USE_URL | STREAM_ONLY_GET_HEADERS, NULL, context))) {
		RETURN_FALSE;
	}

	if (!stream->wrapperdata || Z_TYPE_P(stream->wrapperdata) != IS_ARRAY) {
		php_stream_close(stream);
		RETURN_FALSE;
	}

	array_init(return_value);

	/* check for curl-wrappers that provide headers via a special "headers" element */
	if (zend_hash_find(HASH_OF(stream->wrapperdata), "headers", sizeof("headers"), (void **)&h) != FAILURE && Z_TYPE_PP(h) == IS_ARRAY) {
		/* curl-wrappers don't load data until the 1st read */ 
		if (!Z_ARRVAL_PP(h)->nNumOfElements) {
			php_stream_getc(stream);
		}
		zend_hash_find(HASH_OF(stream->wrapperdata), "headers", sizeof("headers"), (void **)&h);
		hashT = Z_ARRVAL_PP(h);	
	} else {
		hashT = HASH_OF(stream->wrapperdata);
	}

	zend_hash_internal_pointer_reset_ex(hashT, &pos);
	while (zend_hash_get_current_data_ex(hashT, (void**)&hdr, &pos) != FAILURE) {
		if (!hdr || Z_TYPE_PP(hdr) != IS_STRING) {
			zend_hash_move_forward_ex(hashT, &pos);
			continue;
		}
		if (!format) {
no_name_header:
			add_next_index_stringl(return_value, Z_STRVAL_PP(hdr), Z_STRLEN_PP(hdr), 1);
		} else {
			char c;
			char *s, *p;

			if ((p = strchr(Z_STRVAL_PP(hdr), ':'))) {
				c = *p;
				*p = '\0';
				s = p + 1;
				while (isspace((int)*(unsigned char *)s)) {
					s++;
				}

				if (zend_hash_find(HASH_OF(return_value), Z_STRVAL_PP(hdr), (p - Z_STRVAL_PP(hdr) + 1), (void **) &prev_val) == FAILURE) {
					add_assoc_stringl_ex(return_value, Z_STRVAL_PP(hdr), (p - Z_STRVAL_PP(hdr) + 1), s, (Z_STRLEN_PP(hdr) - (s - Z_STRVAL_PP(hdr))), 1);
				} else { /* some headers may occur more then once, therefor we need to remake the string into an array */
					convert_to_array(*prev_val);
					add_next_index_stringl(*prev_val, s, (Z_STRLEN_PP(hdr) - (s - Z_STRVAL_PP(hdr))), 1);
				}

				*p = c;
			} else {
				goto no_name_header;
			}
		}
		zend_hash_move_forward_ex(hashT, &pos);
	}

	php_stream_close(stream);
}
/* }}} */

/** URL class */
PHP_METHOD(Url, __toString)
{
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	RETURN_STRING("Bar\n", 1);
}

PHP_METHOD(Url, getEncType)
{
	zval *instance, *encType;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	encType = zend_read_property(url_ce, instance, "encType", sizeof("enctype") - 1, 0 TSRMLS_CC);

	RETURN_ZVAL(encType, 1, 0);
}

PHP_METHOD(Url, getIpVersion)
{
	zval *instance, *ipVersion;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	ipVersion = zend_read_property(url_ce, instance, "ipVersion", sizeof("ipVersion") - 1, 1 TSRMLS_CC);

	RETURN_ZVAL(ipVersion, 1, 0);
}

PHP_METHOD(Url, getScheme)
{
	zval *instance, *scheme;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	scheme = zend_read_property(url_ce, instance, "scheme", sizeof("scheme") - 1, 1 TSRMLS_CC);

	RETURN_ZVAL(scheme, 1, 0);
}

PHP_METHOD(Url, getUser)
{
	zval *instance;
	char *user;
	int length;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	user = php_url_get_encoded_property(instance, "user", sizeof("user") - 1, &length);

	if (user == NULL) {
		RETURN_NULL();
	}

	RETURN_STRINGL(user, length, 0);
}

PHP_METHOD(Url, getRawUser)
{
	zval *instance, *user;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	user = zend_read_property(url_ce, instance, "user", sizeof("user") - 1, 1 TSRMLS_CC);

	RETURN_ZVAL(user, 1, 0);
}

PHP_METHOD(Url, getPass)
{
	zval *instance;
	char *pass;
	int length;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	pass = php_url_get_encoded_property(instance, "pass", sizeof("pass") - 1, &length);

	if (pass == NULL) {
		RETURN_NULL();
	}

	RETURN_STRINGL(pass, length, 0);
}

PHP_METHOD(Url, getRawPass)
{
	zval *instance, *pass;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	pass = zend_read_property(url_ce, instance, "pass", sizeof("pass") - 1, 1 TSRMLS_CC);

	RETURN_ZVAL(pass, 1, 0);
}

PHP_METHOD(Url, getHost)
{
	zval *instance, *host;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	host = zend_read_property(url_ce, instance, "host", sizeof("host") - 1, 1 TSRMLS_CC);

	RETURN_ZVAL(host, 1, 0);
}

PHP_METHOD(Url, getPort)
{
	zval *instance, *port;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	port = zend_read_property(url_ce, instance, "port", sizeof("port") - 1, 1 TSRMLS_CC);

	RETURN_ZVAL(port, 1, 0);
}

PHP_METHOD(Url, getPath)
{
	zval *instance, *zv_path;
	char *path;
	smart_str res = {0};
	long encType;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	zv_path = zend_read_property(url_ce, instance, "path", sizeof("path") - 1, 1 TSRMLS_CC);
	
	if (!zv_path || Z_TYPE_P(zv_path) != IS_STRING) {
		RETURN_NULL();
	}

	encType = PHP_URL_GET_ENCTYPE(url_ce, instance);
	path = Z_STRVAL_P(zv_path);
	char *path_tmp = estrdup(path);
	char *path_delimiter_str = "/";
	char path_delimiter = '/';
	char *path_part = strtok(path_tmp, path_delimiter_str);

	while (path_part != NULL) {
		smart_str_appendc(&res, path_delimiter);

		int path_part_len;
		path_part = php_url_encode_by_type(path_part, strlen(path_part), &path_part_len, encType);
		smart_str_appendl(&res, path_part, path_part_len);
		path_part = strtok(NULL, path_delimiter_str);
	}

	efree(path_tmp);

	RETURN_STRINGL(res.c, res.len, 0);
}

PHP_METHOD(Url, getRawPath)
{
	zval *instance, *path;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	path = zend_read_property(url_ce, instance, "path", sizeof("path") - 1, 1 TSRMLS_CC);

	RETURN_ZVAL(path, 1, 0);
}

PHP_METHOD(Url, getQueryString)
{
}

PHP_METHOD(Url, getQueryVariables)
{
	zval *instance, *queryVariables;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	queryVariables = zend_read_property(url_ce, instance, "queryVariables", sizeof("queryVariables") - 1, 1 TSRMLS_CC);

	RETURN_ZVAL(queryVariables, 1, 0);
}

PHP_METHOD(Url, getQueryVariable)
{
}

PHP_METHOD(Url, getFragment)
{
	zval *instance;
	char *fragment;
	int length;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	fragment = php_url_get_encoded_property(instance, "fragment", sizeof("fragment") - 1, &length);

	if (fragment == NULL) {
		RETURN_NULL();
	}

	RETURN_STRINGL(fragment, length, 0);
}

PHP_METHOD(Url, getRawFragment)
{
	zval *instance, *fragment;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	instance = getThis();
	fragment = zend_read_property(url_ce, instance, "fragment", sizeof("fragment") - 1, 1 TSRMLS_CC);

	RETURN_ZVAL(fragment, 1, 0);
}

PHP_METHOD(Url, setEncType)
{
	zval *instance;
	long encType;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &encType) == FAILURE) {
		return;
	}

	if (encType != PHP_QUERY_RFC1738 && encType != PHP_QUERY_RFC3986) {
		zend_throw_exception(NULL, "Invalid encoding type was passed", 0);
	}

	instance = getThis();
	zend_update_property_long(url_ce, instance, "encType", sizeof("encType") - 1, encType TSRMLS_CC);
}

PHP_METHOD(Url, setIpVersion)
{
	zval *instance;
	long ipVersion;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &ipVersion) == FAILURE) {
		return;
	}

	if (ipVersion != PHP_URL_IPv4 && ipVersion != PHP_URL_IPv6) {
		zend_throw_exception(NULL, "Invalid ip version was passed", 0);
	}

	instance = getThis();
	zend_update_property_long(url_ce, instance, "ipVersion", sizeof("ipVersion") - 1, ipVersion TSRMLS_CC);
}

PHP_METHOD(Url, setScheme)
{
	zval *instance;
	char *scheme;
	int scheme_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &scheme, &scheme_len) == FAILURE){
		return;
	}

	instance = getThis();
	zend_update_property_stringl(url_ce, instance, "scheme", sizeof("scheme") - 1, scheme, scheme_len TSRMLS_CC);
}

PHP_METHOD(Url, setUser)
{
	zval *instance;
	char *user;
	int user_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &user, &user_len) == FAILURE) {
		return;
	}

	instance = getThis();
	zend_update_property_stringl(url_ce, instance, "user", sizeof("user") - 1, user, user_len TSRMLS_CC);
}

PHP_METHOD(Url, setPass)
{
	zval *instance;
	char *pass;
	int pass_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &pass, &pass_len) == FAILURE) {
		return;
	}

	instance = getThis();
	zend_update_property_stringl(url_ce, instance, "pass", sizeof("pass") - 1, pass, pass_len TSRMLS_CC);
}

PHP_METHOD(Url, setHost)
{
	zval *instance;
	char *host;
	int host_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &host, &host_len) == FAILURE) {
		return;
	}

	instance = getThis();
	zend_update_property_stringl(url_ce, instance, "host", sizeof("host") - 1, host, host_len TSRMLS_CC);
}

PHP_METHOD(Url, setPort)
{
	zval *instance;
	long port;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &port) == FAILURE) {
		return;
	}

	instance = getThis();
	zend_update_property_long(url_ce, instance, "port", sizeof("port") - 1, port TSRMLS_CC);
}

PHP_METHOD(Url, setPath)
{
	zval *instance;
	char *path;
	int path_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &path, &path_len) == FAILURE) {
		return;
	}

	instance = getThis();
	zend_update_property_stringl(url_ce, instance, "path", sizeof("path") - 1, path, path_len TSRMLS_CC);
}

PHP_METHOD(Url, setQuery)
{
	zval *instance, *zv_query, *prop_query, tmp;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "z", &zv_query) == FAILURE) {
		return;
	}

	switch (Z_TYPE_P(zv_query)) {
		case IS_STRING: ;
			char *query_str = estrdup(Z_STRVAL_P(zv_query));
			array_init(&tmp);
			sapi_module.treat_data(PARSE_STRING, query_str, &tmp TSRMLS_CC);
			prop_query = &tmp;
			break;
		case IS_ARRAY:
			prop_query = zv_query;
			Z_ADDREF_P(zv_query);
		case IS_OBJECT:
			array_init(&tmp);
			Z_ARRVAL(tmp) = HASH_OF(zv_query);
			prop_query = &tmp;
			break;
		default:
			zend_throw_exception(NULL, "Invalid type passed for query", 0);
			break;
	}

	instance = getThis();
	zend_update_property(url_ce, instance, "queryVariables", sizeof("queryVariables") - 1, prop_query TSRMLS_CC);
}

PHP_METHOD(Url, setFragment)
{
	zval *instance;
	char *fragment;
	int fragment_len;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s", &fragment, &fragment_len) == FAILURE) {
		return;
	}

	instance = getThis();
	zend_update_property_stringl(url_ce, instance, "fragment", sizeof("fragment") - 1, fragment, fragment_len TSRMLS_CC);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_enc_type, 0, 0, 1)
	ZEND_ARG_INFO(0, encType)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_ip_version, 0, 0, 1)
	ZEND_ARG_INFO(0, ipVersion)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_scheme, 0, 0, 1)
	ZEND_ARG_INFO(0, scheme)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_user, 0, 0, 1)
	ZEND_ARG_INFO(0, user)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_pass, 0, 0, 1)
	ZEND_ARG_INFO(0, pass)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_host, 0, 0, 1)
	ZEND_ARG_INFO(0, host)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_port, 0, 0, 1)
	ZEND_ARG_INFO(0, port)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_path, 0, 0, 1)
	ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_query, 0, 0, 1)
	ZEND_ARG_INFO(0, query)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_set_fragment, 0, 0, 1)
	ZEND_ARG_INFO(0, fragment)
ZEND_END_ARG_INFO()

static const zend_function_entry url_functions[] = {
	PHP_ME(Url, __toString, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getEncType, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getIpVersion, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getScheme, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getUser, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getRawUser, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getPass, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getRawPass, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getHost, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getPort, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getPath, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getRawPath, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getQueryString, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getQueryVariables, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getQueryVariable, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getFragment, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, getRawFragment, arginfo_void, ZEND_ACC_PUBLIC)
	PHP_ME(Url, setEncType, arginfo_set_enc_type, ZEND_ACC_PUBLIC)
	PHP_ME(Url, setIpVersion, arginfo_set_ip_version, ZEND_ACC_PUBLIC)
	PHP_ME(Url, setScheme, arginfo_set_scheme, ZEND_ACC_PUBLIC)
	PHP_ME(Url, setUser, arginfo_set_user, ZEND_ACC_PUBLIC)
	PHP_ME(Url, setPass, arginfo_set_pass, ZEND_ACC_PUBLIC)
	PHP_ME(Url, setHost, arginfo_set_host, ZEND_ACC_PUBLIC)
	PHP_ME(Url, setPort, arginfo_set_port, ZEND_ACC_PUBLIC)
	PHP_ME(Url, setPath, arginfo_set_path, ZEND_ACC_PUBLIC)
	PHP_ME(Url, setQuery, arginfo_set_query, ZEND_ACC_PUBLIC)
	PHP_ME(Url, setFragment, arginfo_set_fragment, ZEND_ACC_PUBLIC)
	PHP_FE_END
};

PHP_MINIT_FUNCTION(url)
{
	zend_class_entry tmp_ce;
	INIT_CLASS_ENTRY(tmp_ce, "Url", url_functions);
	url_ce = zend_register_internal_class(&tmp_ce TSRMLS_CC);
	zend_declare_property_long(url_ce, "encType", sizeof("encType") - 1, PHP_QUERY_RFC3986, ZEND_ACC_PRIVATE TSRMLS_CC);

	return SUCCESS;
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
