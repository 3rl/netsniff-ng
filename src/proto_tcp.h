/*
 * netsniff-ng - the packet sniffing beast
 * By Daniel Borkmann <daniel@netsniff-ng.org>
 * Copyright 2009, 2010 Daniel Borkmann.
 * Subject to the GPL, version 2.
 */

#ifndef TCP_H
#define TCP_H

#include <stdio.h>
#include <stdint.h>
#include <netinet/in.h>    /* for ntohs() */

#include "proto_struct.h"
#include "dissector_eth.h"

struct tcphdr {
	uint16_t source;
	uint16_t dest;
	uint32_t seq;
	uint32_t ack_seq;
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__extension__ uint16_t res1:4,
			       doff:4,
			       fin:1,
			       syn:1,
			       rst:1,
			       psh:1,
			       ack:1,
			       urg:1,
			       ece:1,
			       cwr:1;
#elif defined(__BIG_ENDIAN_BITFIELD)
	__extension__ uint16_t doff:4,
			       res1:4,
			       cwr:1,
			       ece:1,
			       urg:1,
			       ack:1,
			       psh:1,
			       rst:1,
			       syn:1,
			       fin:1;
#else
# error  "Adjust your <asm/byteorder.h> defines"
#endif  
	uint16_t window;
	uint16_t check;
	uint16_t urg_ptr;
} __attribute__((packed));

static inline uint16_t tcp_port(uint16_t src, uint16_t dst)
{
	char *tmp1, *tmp2;

	src = ntohs(src);
	dst = ntohs(dst);

	/* XXX: Is there a better way to determine? */
	if (src < dst && src < 1024) {
		return src;
	} else if (dst < src && dst < 1024) {
		return dst;
	} else {
		tmp1 = lookup_port_tcp(src);
		tmp2 = lookup_port_tcp(dst);
		if (tmp1 && !tmp2) {
			return src;
		} else if (!tmp1 && tmp2) {
			return dst;
		} else {
			if (src < dst)
				return src;
			else
				return dst;
		}
	}
}

static inline void tcp(uint8_t *packet, size_t len)
{
	struct tcphdr *tcp = (struct tcphdr *) packet;

	if (len < sizeof(struct tcphdr))
		return;

	tprintf(" [ TCP ");
	tprintf("Port (%u => %u, %s%s%s), ",
		ntohs(tcp->source), ntohs(tcp->dest),
		colorize_start(bold),
		lookup_port_tcp(tcp_port(tcp->source, tcp->dest)),
		colorize_end());
	tprintf("SN (0x%x), ", ntohl(tcp->seq));
	tprintf("AN (0x%x), ", ntohl(tcp->ack_seq));
	tprintf("DataOff (%u), ", tcp->doff);
	tprintf("Res (%u), ", tcp->res1);
	tprintf("Flags (");
	if (tcp->fin)
		tprintf("FIN ");
	if (tcp->syn)
		tprintf("SYN ");
	if (tcp->rst)
		tprintf("RST ");
	if (tcp->psh)
		tprintf("PSH ");
	if (tcp->ack)
		tprintf("ACK ");
	if (tcp->urg)
		tprintf("URG ");
	if (tcp->ece)
		tprintf("ECE ");
	if (tcp->cwr)
		tprintf("CWR ");
	tprintf("), ");
	tprintf("Window (%u), ", ntohs(tcp->window));
	tprintf("CSum (0x%.4x), ", ntohs(tcp->check));
	tprintf("UrgPtr (%u)", ntohs(tcp->urg_ptr));
	tprintf(" ]\n");
}

static inline void tcp_less(uint8_t *packet, size_t len)
{
	struct tcphdr *tcp = (struct tcphdr *) packet;

	if (len < sizeof(struct tcphdr))
		return;

	tprintf(" TCP %s%s%s %u/%u F%s",
		colorize_start(bold),
		lookup_port_tcp(tcp_port(tcp->source, tcp->dest)),
		colorize_end(), ntohs(tcp->source), ntohs(tcp->dest),
		colorize_start(bold));
	if (tcp->fin)
		tprintf(" FIN");
	if (tcp->syn)
		tprintf(" SYN");
	if (tcp->rst)
		tprintf(" RST");
	if (tcp->psh)
		tprintf(" PSH");
	if (tcp->ack)
		tprintf(" ACK");
	if (tcp->urg)
		tprintf(" URG");
	if (tcp->ece)
		tprintf(" ECE");
	if (tcp->cwr)
		tprintf(" CWR");
	tprintf("%s Win %u S/A 0x%x/0x%x", colorize_end(),
		ntohs(tcp->window), ntohl(tcp->seq), ntohl(tcp->ack_seq));
}

static inline void tcp_next(uint8_t *packet, size_t len,
			    struct hash_table **table,
			    unsigned int *key, size_t *off)
{
	struct tcphdr *tcp = (struct tcphdr *) packet;

	if (len < sizeof(struct tcphdr))
		goto invalid;

	(*off) = sizeof(struct tcphdr);
	(*key) = tcp_port(tcp->source, tcp->dest);
	(*table) = &eth_lay4;

	return;
invalid:
	(*off) = 0;
	(*key) = 0;
	(*table) = NULL;
}

struct protocol tcp_ops = {
	.key = 0x06,
	.offset = sizeof(struct tcphdr),
	.print_full = tcp,
	.print_less = tcp_less,
	.print_pay_ascii = empty,
	.print_pay_hex = empty,
	.print_pay_none = tcp,
	.print_all_cstyle = __hex2,
	.print_all_hex = __hex,
	.proto_next = tcp_next,
};

#endif /* TCP_H */
