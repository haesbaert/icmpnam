/*
 * Copyright (c) 2011 Christiano F. Haesbaert <haesbaert@haesbaert.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "log.h"

#define CONFIGFILE	"/etc/icmpnam.conf"
#define VERSION		"muahaha"

__dead void	usage(void);
__dead void	display_version(void);
void		conf_load(char *);
void		tun_open(void);
void		icmp_open(void);
void		divert_open(void);

extern char		*malloc_options;
int			sock_tun;
int			sock_icmp;
int			sock_divert;
struct sockaddr_in	sin_remote;

struct configopts {
	char	*name;
	void 	(*func)(char **);
	int	 nargs;
} configopts[] = {
	{"remote", 	NULL, 	1},
	{"dev", 	NULL, 	3},
	{NULL, 		NULL, 	-1},
};

__dead void
usage(void)
{
	extern char	*__progname;

	fprintf(stderr, "usage: %s [-d] [-f configfile]\n",
	    __progname);
	fprintf(stderr, "usage: %s -v\n", __progname);
	exit(1);
}

__dead void
display_version(void)
{
	printf("icmpnam %s\n", VERSION);
	printf("Copyright (C) 2011 Christiano F. Haesbaert\n");
	
	exit(0);
}

/* mostly copied from scrotwm */
void
conf_load(char *cfile)
{
	FILE *config;
	char *cp, *line, *argv[16], tmp[1024];
	size_t len, lineno;
	int wordlen, nargs;
	struct configopts *copts;

	len = lineno = 0;
	if (cfile == NULL)
		fatalx("conf_load: no filename");
	if ((config = fopen(cfile, "r")) == NULL)
		log_err("conf_load: fopen %s", cfile);
	while (!feof(config)) {
		if ((line = fparseln(config, &len, &lineno, NULL, 0))
		    == NULL) {
			if (ferror(config))
				log_err("%s", cfile);
			else
				continue;
		}
		cp = line;
		cp += strspn(cp, " \t\n"); /* eat whitespace */
		if (cp[0] == '\0') {
			/* empty line */
			free(line);
			continue;
		}
		wordlen = strcspn(cp, " \t\n");
		if (wordlen == 0)
			log_err("%s: line %zd: no option found",
			    cfile, lineno);
		for (copts = configopts; copts->name; copts++) {
			if (strncasecmp(cp, copts->name, wordlen))
				continue;
			/* match */
			nargs = copts->nargs;
			cp += wordlen;
			while (nargs) {
				cp += strspn(cp, " \t\n");
				wordlen = strcspn(cp, " \t\n");
				if (wordlen == 0)
					log_errx("%s wants %d arguments, "
					    "%d given", copts->name,
					    copts->nargs,
					    copts->nargs - nargs);
				if (wordlen >= (int)sizeof(tmp))
					log_errx("%s: line %zd "
					    "too long", cfile, lineno);
				(void)strlcpy(tmp, cp, wordlen + 1);
				argv[copts->nargs - nargs] = strdup(tmp);
				nargs--;
				cp += wordlen;
			}
			cp += strspn(cp, " \t\n");
			if (*cp != 0)
				log_errx("%s: line %d superfluous "
				    "argument", cfile, lineno);
			nargs = copts->nargs;
			while (nargs)
				free(argv[--nargs]);
			/* XXX notyet copts->func(argv); */
		}
		free(line);
	}
	fclose(config);
}

void
tun_open(void)
{
	
}

void
icmp_open(void)
{
	
}

void
divert_open(void)
{
	
}

int
main(int argc, char *argv[])
{
	int ch, debug;
	char *cfile;
	
	debug = 0;
	cfile = CONFIGFILE;
	while ((ch = getopt(argc, argv, "dvf:")) != -1) {
		switch (ch) {
		case 'd':
			debug = 1;
			malloc_options = "AFGJPX";
			break;
		case 'v':
			display_version();
			break;	/* NOTREACHED */
		case 'f':
			cfile = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	/* Log to stderr until daemonized */
	log_init(1);	
	/* Show who we are */
	setproctitle("icmpnam");
	/* Load config */
	conf_load(cfile);
	/* Start libevent prior to open sockets */
	event_init();
	/* Open tun interface and socket */
	tun_open();
	/* Open icmp socket */
	icmp_open();
	/* Open divert socket */
	divert_open();
	/* Finally go daemon */
	log_init(debug);
	if (!debug)
		daemon(1, 0);
	/* Mainloop */
	event_dispatch();
		    
	return (0);
}
