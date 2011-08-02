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

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
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
#define DIVERT_PORT	1805

__dead void	usage(void);
__dead void	display_version(void);
int		conf_load(char *);
int		conf_remote(char **);
int		conf_dev(char **);
void		tun_open(void);
void		icmp_open(void);
void		divert_open(void);

extern char	*malloc_options;
int		 sock_tun;
int		 sock_icmp;
int		 sock_divert;
struct in_addr	 in_remote;
char		 tun_dev[IFNAMSIZ];
struct in_addr	 tun_us;
struct in_addr	 tun_them;
u_int16_t	 divert_port = DIVERT_PORT;

struct configopts {
	char	*name;
	int 	(*func)(char **);
	int	 nargs;
} configopts[] = {
	{"remote", 	conf_remote, 	1},
	{"dev", 	conf_dev, 	3},
	{NULL, 		NULL, 		-1},
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

int
conf_remote(char **argv)
{
	char *s = argv[0];
	
	if (inet_aton(s, &in_remote) == -1) {
		log_warn("invalid remote");
		return (-1);
	}
	log_debug("remote %s", s);
	
	return (0);
}

int conf_dev(char **argv)
{
	char	*dev  = argv[0];
	char	*us   = argv[1];
	char	*them = argv[2];

	if (strncmp(dev, "tun", 3) != 0) {
		log_warnx("Invalid dev, need a tun interface");
		return (-1);
	}
	(void)strlcpy(tun_dev, dev, sizeof(tun_dev));
	if (inet_aton(us, &tun_us) == -1) {
		log_warn("invalid address %s", us);
		return (-1);
	}
	if (inet_aton(them, &tun_them) == -1) {
		log_warn("invalid address %s", them);
		return (-1);
	}
	log_debug("dev %s %s %s", dev, us, them);
	
	return (0);
}

/* mostly copied from scrotwm */
int
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
	if ((config = fopen(cfile, "r")) == NULL) {
		log_warn("conf_load: fopen %s", cfile);
		return (-1);
	}
	while (!feof(config)) {
		if ((line = fparseln(config, &len, &lineno, NULL, 0))
		    == NULL) {
			if (ferror(config)) {
				log_warn("%s", cfile);
				return (-1);
			}
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
		if (wordlen == 0) {
			log_warnx("%s: line %zd: no option found",
			    cfile, lineno);
			return (-1);
		}
		for (copts = configopts; copts->name; copts++) {
			if (strncasecmp(cp, copts->name, wordlen))
				continue;
			/* match */
			nargs = copts->nargs;
			cp += wordlen;
			while (nargs) {
				cp += strspn(cp, " \t\n");
				wordlen = strcspn(cp, " \t\n");
				if (wordlen == 0) {
					log_warnx("%s wants %d arguments, "
					    "%d given", copts->name,
					    copts->nargs,
					    copts->nargs - nargs);
					return (-1);
				}
				if (wordlen >= (int)sizeof(tmp)) {
					log_warnx("%s: line %zd "
					    "too long", cfile, lineno);
					return (-1);
				}
				(void)strlcpy(tmp, cp, wordlen + 1);
				argv[copts->nargs - nargs] = strdup(tmp);
				nargs--;
				cp += wordlen;
			}
			cp += strspn(cp, " \t\n");
			if (*cp != 0)
				log_warnx("%s: line %d superfluous "
				    "argument, ignoring", cfile, lineno);
			nargs = copts->nargs;
			if (copts->func(argv) == -1)
				return (-1);
			while (nargs)
				free(argv[--nargs]);
		}
		free(line);
	}
	fclose(config);
	
	/* Check if we have everything */
	if (tun_dev[0] == 0)
		fatalx("no dev specified");
	if (in_remote.s_addr == 0)
		fatalx("no remote specified");
	
	return (0);
}

void
tun_open(void)
{
	struct ifreq ifr;
	struct in_aliasreq ifra;
	char tunpath[16];
	int s;
	
	bzero(&ifr, sizeof(ifr));
	(void)strlcpy(ifr.ifr_name, tun_dev, sizeof(ifr.ifr_name));
	ifr.ifr_addr.sa_family = AF_INET;
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
		fatal("socket");
	if (ioctl(s, SIOCIFDESTROY, &ifr) == -1 && errno != ENXIO)
		fatal("ioctl: SIOCIFDESTROY");
	if (ioctl(s, SIOCIFCREATE, (caddr_t)&ifr) == -1)
		fatal("ioctl: SIOCIFCREATE");
	/* ifra */
	bzero(&ifra, sizeof(ifra));
	strlcpy(ifra.ifra_name, tun_dev, sizeof(ifra.ifra_name));
	ifra.ifra_addr.sin_len	     = sizeof(struct sockaddr_in);
	ifra.ifra_addr.sin_family    = AF_INET;
	ifra.ifra_addr.sin_addr	     = tun_us;
	ifra.ifra_dstaddr.sin_len    = sizeof(struct sockaddr_in);
	ifra.ifra_dstaddr.sin_family = AF_INET;
	ifra.ifra_dstaddr.sin_addr   = tun_them;
	if (ioctl(s, SIOCAIFADDR, (caddr_t)&ifra) == -1)
		fatal("ioctl: SIOCAIFADDR");
	(void)snprintf(tunpath, sizeof(tunpath), "/dev/%s", tun_dev);
	if ((sock_tun = open(tunpath, O_RDWR, 0)) == -1)
		fatal("open");
	close(s);
	log_debug("sock_tun = %d", sock_tun);
}

void
icmp_open(void)
{
	if ((sock_icmp = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
		fatal("socket");
	log_debug("sock_icmp = %d", sock_icmp);
}

void
divert_open(void)
{
	struct sockaddr_in sin;
	
	bzero(&sin, sizeof(sin));
	sin.sin_port = htons(divert_port);
	if ((sock_divert = socket(AF_INET, SOCK_RAW, IPPROTO_DIVERT)) == -1)
		fatal("socket");
	if (bind(sock_divert, (struct sockaddr *)&sin, sizeof(sin)) == -1)
		fatal("bind");
	log_debug("sock_divert = %d", sock_divert);
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
	/* Log init */
	log_init(debug);	
	/* Show who we are */
	setproctitle("icmpnam");
	/* Load config */
	if (conf_load(cfile) != 0)
		fatalx("can't parse config file"); 
	/* Start libevent prior to open sockets */
	event_init();
	/* Open tun interface and socket */
	tun_open();
	/* Open icmp socket */
	icmp_open();
	/* Open divert socket */
	divert_open();
	/* Finally go daemon */
	if (!debug)
		daemon(1, 0);
	log_info("startup");
	/* Mainloop */
	event_dispatch();
		    
	return (0);
}
