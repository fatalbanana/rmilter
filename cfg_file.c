#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libmilter/mfapi.h>
#include <sys/queue.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <netdb.h>
#include <math.h>

#include "pcre.h"
#include "cfg_file.h"
#include "spf.h"

extern int yylineno;
extern char *yytext;

static size_t
copy_regexp (char **dst, const char *src)
{
	size_t len;
	if (!src || *src == '\0') return 0;

	len = strlen (src);

	/* Skip slashes */
	if (*src == '/') {
		src++;
		len--;
	}
	if (src[len - 1] == '/') {
		len--;
	}

	*dst = malloc (len + 1);
	if (!*dst) return 0;

	return strlcpy (*dst, src, len + 1);
}

int
add_memcached_server (struct config_file *cf, char *str, char *str2, int type)
{
	char *cur_tok, *err_str;
	struct memcached_server *mc = NULL;
	struct hostent *he;
	uint16_t port;

	if (str == NULL) return 0;

	if (type == MEMCACHED_SERVER_GREY) {
		if(cf->memcached_servers_grey_num == MAX_MEMCACHED_SERVERS) {
			yywarn ("yyparse: maximum number of memcached servers is reached %d", MAX_MEMCACHED_SERVERS);
			return 0;
		}
	
		mc = &cf->memcached_servers_grey[cf->memcached_servers_grey_num];
	}
	else if (type == MEMCACHED_SERVER_WHITE) {
		if(cf->memcached_servers_white_num == MAX_MEMCACHED_SERVERS) {
			yywarn ("yyparse: maximum number of whitelist memcached servers is reached %d", MAX_MEMCACHED_SERVERS);
			return 0;
		}
	
		mc = &cf->memcached_servers_white[cf->memcached_servers_white_num];
	}
	else if (type == MEMCACHED_SERVER_LIMITS) {
		if(cf->memcached_servers_limits_num == MAX_MEMCACHED_SERVERS) {
			yywarn ("yyparse: maximum number of whitelist memcached servers is reached %d", MAX_MEMCACHED_SERVERS);
			return 0;
		}
	
		mc = &cf->memcached_servers_limits[cf->memcached_servers_limits_num];
	}
	if (mc == NULL) return 0;

	cur_tok = strsep (&str, ":");

	if (cur_tok == NULL || *cur_tok == '\0') return 0;

	/* cur_tok - server name, str - server port */
	if (str == NULL) {
		port = htons(DEFAULT_MEMCACHED_PORT);
	}
	else {
		port = htons ((uint16_t)strtoul (str, &err_str, 10));
		if (*err_str != '\0') {
			return 0;
		}
	}

	if (!inet_aton (cur_tok, &mc->addr[0])) {
		/* Try to call gethostbyname */
		he = gethostbyname (cur_tok);
		if (he == NULL) {
			return 0;
		}
		else {
			memcpy((char *)&mc->addr[0], he->h_addr, sizeof(struct in_addr));
		}
	}
	mc->port[0] = port;

	if (str2 == NULL) {
		mc->num = 1;
	}
	else {
		mc->num = 2;
		cur_tok = strsep (&str2, ":");

		if (cur_tok == NULL || *cur_tok == '\0') return 0;

		/* cur_tok - server name, str - server port */
		if (str2 == NULL) {
			port = htons(DEFAULT_MEMCACHED_PORT);
		}
		else {
			port = htons ((uint16_t)strtoul (str2, &err_str, 10));
			if (*err_str != '\0') {
				return 0;
			}
		}

		if (!inet_aton (cur_tok, &mc->addr[1])) {
			/* Try to call gethostbyname */
			he = gethostbyname (cur_tok);
			if (he == NULL) {
				return 0;
			}
			else {
				memcpy((char *)&mc->addr[1], he->h_addr, sizeof(struct in_addr));
			}
		}
		mc->port[1] = port;
	}
	
	mc->alive[0] = 1;
	mc->alive[1] = 1;

	if (type == MEMCACHED_SERVER_GREY) {
		cf->memcached_servers_grey_num++;
	}
	else if (type == MEMCACHED_SERVER_WHITE) {
		cf->memcached_servers_white_num++;
	}
	else if (type == MEMCACHED_SERVER_LIMITS) {
		cf->memcached_servers_limits_num++;
	}

	return 1;
}

int
add_clamav_server (struct config_file *cf, char *str)
{
	char *cur_tok, *err_str;
	struct clamav_server *srv;
	struct hostent *he;
	size_t s;

	if (str == NULL) return 0;
	
	cur_tok = strsep (&str, ":");
	
	if (cur_tok == NULL || *cur_tok == '\0') return 0;

	if (cf->clamav_servers_num == MAX_CLAMAV_SERVERS) {
		yywarn ("yyparse: maximum number of clamav servers is reached %d", MAX_CLAMAV_SERVERS);
	}

	srv = &cf->clamav_servers[cf->clamav_servers_num];

	if (srv == NULL) return 0;

	if (cur_tok[0] == '/' || cur_tok[0] == '.') {
		srv->sock.unix_path = strdup (cur_tok);
		srv->sock_type = AF_UNIX;
		srv->name = srv->sock.unix_path;

		cf->clamav_servers_num++;
		return 1;

	} else {
		if (str == '\0') {
			srv->sock.inet.port = htons (DEFAULT_CLAMAV_PORT);
		}
		else {
			srv->sock.inet.port = htons ((uint16_t)strtoul (str, &err_str, 10));
			if (*err_str != '\0') {
				return 0;
			}
		}

		if (!inet_aton (cur_tok, &srv->sock.inet.addr)) {
			/* Try to call gethostbyname */
			he = gethostbyname (cur_tok);
			if (he == NULL) {
				return 0;
			}
			else {
				srv->name = strdup (cur_tok);
				memcpy((char *)&srv->sock.inet.addr, he->h_addr, sizeof(struct in_addr));
				s = strlen (cur_tok) + 1;
			}
		}

		srv->sock_type = AF_INET;
		cf->clamav_servers_num++;
		return 1;
	}

	return 0;
}

int
add_spamd_server (struct config_file *cf, char *str)
{
	char *cur_tok, *err_str;
	struct spamd_server *srv;
	struct hostent *he;
	size_t s;

	if (str == NULL) return 0;
	
	cur_tok = strsep (&str, ":");
	
	if (cur_tok == NULL || *cur_tok == '\0') return 0;

	if (cf->spamd_servers_num == MAX_SPAMD_SERVERS) {
		yywarn ("yyparse: maximum number of spamd servers is reached %d", MAX_SPAMD_SERVERS);
	}

	srv = &cf->spamd_servers[cf->spamd_servers_num];

	if (srv == NULL) return 0;

	if (cur_tok[0] == '/' || cur_tok[0] == '.') {
		srv->sock.unix_path = strdup (cur_tok);
		srv->sock_type = AF_UNIX;
		srv->name = srv->sock.unix_path;

		cf->spamd_servers_num++;
		return 1;

	} else {
		if (str == '\0') {
			srv->sock.inet.port = htons (DEFAULT_SPAMD_PORT);
		}
		else {
			srv->sock.inet.port = htons ((uint16_t)strtoul (str, &err_str, 10));
			if (*err_str != '\0') {
				return 0;
			}
		}

		if (!inet_aton (cur_tok, &srv->sock.inet.addr)) {
			/* Try to call gethostbyname */
			he = gethostbyname (cur_tok);
			if (he == NULL) {
				return 0;
			}
			else {
				srv->name = strdup (cur_tok);
				memcpy((char *)&srv->sock.inet.addr, he->h_addr, sizeof(struct in_addr));
				s = strlen (cur_tok) + 1;
			}
		}

		srv->sock_type = AF_INET;
		cf->spamd_servers_num++;
		return 1;
	}

	return 0;
}


struct action *
create_action (enum action_type type, const char *message)
{
	struct action *new;
	size_t len = strlen (message);

	if (message == NULL) return NULL;

	new = (struct action *)malloc (sizeof (struct action)); 

	if (new == NULL) return NULL;

	new->type = type;
	/* Trim quotes */
	if (*message == '"') {
		message++;
		len--;
	}
	if (message[len - 1] == '"') {
		len--;
	}

	new->message = (char *)malloc (len + 1);

	if (new->message == NULL) return NULL;

	strlcpy (new->message, message, len + 1);

	return new;
}

struct condition *
create_cond (enum condition_type type, const char *arg1, const char *arg2)
{
	struct condition *new;
	int offset;
	const char *read_err;

	new = (struct condition *)malloc (sizeof (struct condition));
	bzero (new, sizeof (struct condition));
	
	if (new == NULL) return NULL;

	if (arg1 == NULL || *arg1 == '\0') {
		new->args[0].empty = 1;
	}
	else {
		if (!copy_regexp (&new->args[0].src, arg1)) {
			new->args[0].empty = 1;
		}
		else {
			new->args[0].re = pcre_compile (new->args[0].src, 0, &read_err, &offset, NULL);
			if (new->args[0].re == NULL) {
				new->args[0].empty = 1;
			}
		}
	}
	if (arg2 == NULL || *arg2 == '\0') {
		new->args[1].empty = 1;
	}
	else {
		if (!copy_regexp (&new->args[1].src, arg2)) {
			new->args[1].empty = 1;
		}
		else {
			new->args[1].re = pcre_compile (new->args[1].src, 0, &read_err, &offset, NULL);
			if (new->args[1].re == NULL) {
				new->args[1].empty = 1;
			}
		}
	}

	new->type = type;

	return new;
}

int
add_spf_domain (struct config_file *cfg, char *domain)
{
	if (!domain) return 0;

	if (cfg->spf_domains_num > MAX_SPF_DOMAINS) {
		return 0;
	}

	cfg->spf_domains[cfg->spf_domains_num] = domain;
	cfg->spf_domains_num ++;

	return 1;
}

int
add_ip_radix (struct config_file *cfg, char *ipnet)
{
	uint32_t mask;
	uint32_t ip;
	char *token;
	struct in_addr ina;
	int k;

	token = strsep (&ipnet, "/");

	if (ipnet == NULL) {
		/* Assume /32 if no mask is given */
		mask = 0xFFFFFFFF;
	}
	else {
		k = atoi (ipnet);
		if (k > 32 || k < 0) {
			yywarn ("add_ip_radix: invalid netmask value: %d", k);
			k = 32;
		}
		mask = pow (2, k) - 1;
		mask <<= 32 - k;
	}

	if (inet_aton (token, &ina) == 0) {
		yyerror ("add_ip_radix: invalid ip address: %s", token);
		return 0;
	}

	ip = (uint32_t)ina.s_addr;
	if (radix32tree_insert (cfg->grey_whitelist_tree, ip, mask, 1) == -1) {
		yyerror ("add_ip_radix: cannot insert ip to tree");
		return 0;
	}

	return 1;
}

void
init_defaults (struct config_file *cfg)
{
	LIST_INIT (&cfg->rules);
	LIST_INIT (&cfg->whitelist_ip);
	LIST_INIT (&cfg->whitelist_rcpt);
	LIST_INIT (&cfg->bounce_addrs);

	cfg->clamav_connect_timeout = DEFAULT_CLAMAV_CONNECT_TIMEOUT;
	cfg->clamav_port_timeout = DEFAULT_CLAMAV_PORT_TIMEOUT;
	cfg->clamav_results_timeout = DEFAULT_CLAMAV_RESULTS_TIMEOUT;
	cfg->memcached_connect_timeout = DEFAULT_MEMCACHED_CONNECT_TIMEOUT;
	cfg->spamd_connect_timeout = DEFAULT_SPAMD_CONNECT_TIMEOUT;
	cfg->spamd_results_timeout = DEFAULT_SPAMD_RESULTS_TIMEOUT;

	cfg->clamav_error_time = DEFAULT_UPSTREAM_ERROR_TIME;
	cfg->clamav_dead_time = DEFAULT_UPSTREAM_DEAD_TIME;
	cfg->clamav_maxerrors = DEFAULT_UPSTREAM_MAXERRORS;

	cfg->spamd_error_time = DEFAULT_UPSTREAM_ERROR_TIME;
	cfg->spamd_dead_time = DEFAULT_UPSTREAM_DEAD_TIME;
	cfg->spamd_maxerrors = DEFAULT_UPSTREAM_MAXERRORS;


	cfg->memcached_error_time = DEFAULT_UPSTREAM_ERROR_TIME;
	cfg->memcached_dead_time = DEFAULT_UPSTREAM_DEAD_TIME;
	cfg->memcached_maxerrors = DEFAULT_UPSTREAM_MAXERRORS;
	cfg->memcached_protocol = UDP_TEXT;
	
	cfg->grey_whitelist_tree = radix_tree_create ();

	cfg->spf_domains = (char **) calloc (MAX_SPF_DOMAINS, sizeof (char *));
	cfg->awl_enable = 0;
}

void
free_config (struct config_file *cfg)
{
	int i;
	struct rule *cur, *tmp_rule;
	struct condition *cond, *tmp_cond;
	struct ip_list_entry *ip_cur, *ip_tmp;
	struct addr_list_entry *addr_cur, *addr_tmp;

	if (cfg->pid_file) {
		free (cfg->pid_file);
	}
	if (cfg->temp_dir) {
		free (cfg->temp_dir);
	}
	if (cfg->sock_cred) {
		free (cfg->sock_cred);
	}

	if (cfg->spf_domains) {
		for (i = 0; i < MAX_SPF_DOMAINS; i++) {
			free (cfg->spf_domains[i]);
		}
		free (cfg->spf_domains);
	}
	
	for (i = 0; i < cfg->clamav_servers_num; i++) {
		free (cfg->clamav_servers[i].name);
	}
	for (i = 0; i < cfg->spamd_servers_num; i++) {
		free (cfg->spamd_servers[i].name);
	}
	/* Free rules list */
	LIST_FOREACH_SAFE (cur, &cfg->rules, next, tmp_rule) {
		LIST_FOREACH_SAFE (cond, cur->conditions, next, tmp_cond) {
			if (!cond->args[0].empty) {
				if (cond->args[0].re != NULL) {
					pcre_free (cond->args[0].re);
				}
				if (cond->args[0].src != NULL) {
					free (cond->args[0].src);
				}
			}
			if (!cond->args[1].empty) {
				if (cond->args[1].re != NULL) {
					pcre_free (cond->args[1].re);
				}
				if (cond->args[1].src != NULL) {
					free (cond->args[1].src);
				}
			}
			LIST_REMOVE (cond, next);
			free (cond);
		}
		if (cur->act->message) {
			free (cur->act->message);
		}
		free (cur->act);
		LIST_REMOVE (cur, next);
		free (cur);
	}
	/* Free whitelists and bounce list*/
	LIST_FOREACH_SAFE (ip_cur, &cfg->whitelist_ip, next, ip_tmp) {
		LIST_REMOVE (ip_cur, next);
		free (ip_cur);
	}
	LIST_FOREACH_SAFE (addr_cur, &cfg->whitelist_rcpt, next, addr_tmp) {
		if (addr_cur->addr) {
			free (addr_cur->addr);
		}
		LIST_REMOVE (addr_cur, next);
		free (addr_cur);
	}
	LIST_FOREACH_SAFE (addr_cur, &cfg->bounce_addrs, next, addr_tmp) {
		if (addr_cur->addr) {
			free (addr_cur->addr);
		}
		LIST_REMOVE (addr_cur, next);
		free (addr_cur);
	}

	radix32tree_delete (cfg->grey_whitelist_tree, 0, 0);
	free (cfg->grey_whitelist_tree);

	if (cfg->awl_enable && cfg->awl_hash != NULL) {
		free (cfg->awl_hash->pool);
		free (cfg->awl_hash);
	}
}

/*
 * vi:ts=4
 */
