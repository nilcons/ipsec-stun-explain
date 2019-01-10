# TLDR

We discuss:
  - STUN protocol,
  - UDP hole punching in the command line,
  - IPsec by hand (without daemons or `ipsec.conf`, only kernel `ip` commands).

We will establish a fully working IPsec tunnel between two machines
that are both behind NAT and only have IPv4 addresses.  The tunnel,
once established will only use direct communication between the
machines, no proxy or mediator will be injected into the traffic flow.

If you want to follow the steps, and you have two computers, but you
only have one internet connection, then you can try tethering with
your mobile for one of the computers.  This way you will have two
independent connections in the same room.

# About the STUN protocol

STUN is a standardized implementation of the "whatismyipaddress.com"
idea, with two very handy differences: it uses UDP, not HTTP, and it
also tells you the source port number that your packet used on your
side.

You can install the `stun-client` Debian package and then use it like
this:

    stun -v -i wlan0 -p 11341 stun.ekiga.net 1 2>&1 | grep ^MappedAddress

This sends an UDP packet from your machine with sourt port of 11341 to
the STUN server which in turn replies with your public IP and public
port:

    MappedAddress = 85.9.8.7:11341

In this case the NAT didn't translate the port number, just the IP.

You can find a public STUN server list here:
https://gist.github.com/mondain/b0ec1cf5f60ae726202e, if you choose
one which doesn't use the default port of 3478, then you can use it
like this:

    stun -v -i wlan0 -p 11341 stun3.l.google.com:19302 1 2>&1 | grep ^MappedAddress

## The NAT server on side A

In my case on side A I had a NAT server (my own router in my home),
that was not changing port numbers, only if absolutely necessary.
Therefore I can be quite certain that a new random port number I
choose will have the same public mapping, only the IP will change to
85.9.8.7.  So I chose 61525 and it worked.

## The NAT server on side B

On side B, I was less lucky: the NAT server changed the outgoing port
number randomly.  But, using two different STUN servers I noticed a
very interesting thing: the mapped source port only depends on the
original source port and not on the destination IP.  Therefore I can
ask STUN what is my public port and then assume that it will not
change when I start talking to an other IP.  I chose the local port
61523 and it got mapped to IP of 31.1.2.3 and port of 10421.

## Which is better, side A or side B?

Usual home routers or Linux kernels used as firewalls will work like
side A and big company firewalls or carrier-grade NAT solutions will
work like side B.  Side A looks easier and more simple, since you
simply keep your port number with 99% certainty.

In my opinion, side B's idea is better: you never keep your port
number, but if you use STUN, you get one assigned for sure, 100%.
This may well be one more step when you are trying to punch your UDP
hole, but in exchange you get certainty.

# Hole punching with netcat in the command-line

After understanding the specific working conditions of side A and side
B, we can finally do our UDP hole punching with netcat in command line:

Side A: `while sleep 1 ; do echo $(date) to side B ; done | nc -v -u -n -p 61525 31.1.2.3 10421`

Side B: `while sleep 1 ; do echo $(date) to side A ; done | nc -v -u -n -p 61523 85.9.8.7 61525`

Again, on side B, we don't have to know our mapped port number
of 10421.  This makes sense, it gets mapped and we can't do anything
about it, only the other side has to be aware.

Once you start seeing the messages on both sides, your UDP channel is
establised, you can stop the scripts and you have 1-2 minutes to start
the IPsec tunnel with the commands that we discuss in the following
sections. (Don't forget the C component.)  You only have 1-2 minutes,
because assigned UDP ports do expire in the NAT translator boxes
within minutes if they are idle.

## But my netcat fails with connection error

UDP is not a connection oriented protocol, what happens is that if the
port is not open, then some Linuxes send an ICMP reply asking you to
stop sending UDP packets, because nothing is running on the port.
These ICMP packets may be filtered by the network appliances on the
way, but if they are not, then netcat exits if any of them is received.

If you are working in an environment, where this is the case, then you
can do two things: press Enter in the two terminals (at the two sides)
AT EXACTLY THE SAME TIME.  Or disable the sending of these ICMP
packets: `iptables -I OUTPUT -p icmp --icmp-type
destination-unreachable -j DROP`.  You only have to do this on one of
the sides, where you start the netcat command second.

# The two abstractions of IPsec: associations and policies

A *security association (SA)* is a kernel space specification of how
to format, encrypt and deliver a packet that needs IPsec handling.

*Security policies (SP)* are the kernel space routing instructions on
which traffic should use IPsec and which SA should be used for what
kind of traffic.

By default, IPsec uses these two kernel level abstractions to specify
everything and even in tunneling mode, there will be no extra new
interfaces (tun, tap) to route the traffic into.  This is a huge
difference compared to other VPN solutions and this is what causes the
most confusion to beginners.

StrongSwan and most of the other IPsec solutions available for
GNU/Linux are using these features in the kernel.  So understanding
these primitives can help a lot while operating IPsec or debugging
problems.

We will handle SA manipulation with the `ip xfrm state` command and
SP manipulations with the `ip xfrm policy` command.

## How much for a tunnel?

To have the most basic tunnel between two machines (tunneling just two
private IPs on the public internet), you need 4 SAs and 4 SPs:
  - on machine A: SA to encode packets from A to B,
  - on machine B: SA to encode packets from B to A,
  - on machine A: SA to decode packets from B to A,
  - on machine B: SA to decode packets from A to B,
  - on machine A: SD to take ownership of some traffic flow from A to B,
  - on machine B: SD to take ownership of some traffic flow from B to A,
  - on machine A: SD to verify incoming IPsec packets from B,
  - on machine B: SD to verify incoming IPsec packets from A.

## The third component of IPsec: the IKE daemon

In real life, people use the IKE daemon (and its `ipsec.conf` config
file) to set up IPsec connections.  We will not discuss IKE.

# The ports of IPsec

Compared to OpenVPN, beginners can also get confused about the port
usage of IPsec, here is a short description.

For meta operations (starting up tunnels, agreeing on ciphers,
rekeying periodically), the IKE daemons use port 500 (TCP and UDP).
In this article we will not run an IKE daemon and therefore we will
not use port 500.

Once encryption and parameters are agreed, IPsec uses IP protocol 50
(ESP).  This is not a port number, this is an IP protocol.  This is on
the same level as TCP and UDP, it is a third kind of something, not
part of TCP or UDP.  It is also not easily masqueraded or port
forwarded.

Because of the obscurity and difficulty of using other IP protocols
than TCP and UDP with firewalls, it is possible to do ESP-over-UDP
encapsulation, this is usually using port 4500/UDP.

RFC 8229 (not widely implemented yet) specifies a method for
encapsulating ESP in TCP, which can come handy when some very
restricted environment does not allow UDP either.

TLDR: there are two ports, 500/UDP and 4500/UDP.  For us only 4500/UDP
is important (and even that encapsulated ESP-over-UDP protocol we will
talk on random ports, not on 4500).

# Security associations of a basic tunnel

    root@brooks:~# ip xfrm state
    src 31.1.2.3 dst 192.168.1.4
        proto esp spi 0xdeadbeef reqid 256 mode tunnel
        replay-window 0 flag af-unspec
        aead rfc4106(gcm(aes)) 0x0000000000000000000000000000000000000000 96
        encap type espinudp sport 10421 dport 61525 addr 0.0.0.0
        anti-replay context: seq 0x0, oseq 0x0, bitmap 0x00000000
    src 192.168.1.4 dst 31.1.2.3
        proto esp spi 0xdeadbeef reqid 256 mode tunnel
        replay-window 0 flag af-unspec
        aead rfc4106(gcm(aes)) 0x0000000000000000000000000000000000000000 96
        encap type espinudp sport 61525 dport 10421 addr 0.0.0.0
        anti-replay context: seq 0x0, oseq 0x2b2, bitmap 0x00000000

We have two SAs here for the two directions.

Let's go through the (important) fields:
  - src and dst: how to send the packet if this SA is chosen to be
    used.  The src is important too, not just the dst, because our
    peer will be expecting traffic from her knowledge of us, so if you
    have multiple IPs, you have to choose the right one here; remember
    that ESP is used by default, therefore we don't need ports,
  - spi: the ESP protocol needs some mechanism for having multiple
    tunnels between two machines (similarly how you can have parallel
    TCP connections thanks to ports), this is the SPI; an ESP
    connection is uniquely identified by `(src, dst, spi)`, trying to
    add multiple different parameters for the same triplet into the
    kernel with `ip xfrm add` will result in an error message,
  - reqid: an id for the sysadmin to connect SP's to SA's, is not part
    of the wire format,
  - aead: encryption and authentication of packets at the same time
    with the same AES key of all zeros (remember: we are testing),
  - encap: instead of using ESP, we will use ESP-over-UDP, with sender
    port of 10421 and destination port of 61525.

It is fairly important to understand, that the `src` and `dst`
settings here say nothing about the traffic that will be stuffed into
this tunnel.  This is just instruction to the kernel about how to
format and deliver the final IPsec encapsulated packets if this
`reqid` is used by an SP.

# Security policies of a basic tunnel

    root@brooks:~# ip xfrm policy
    src 192.168.1.4/32 dst 192.168.2.100/32
    	dir out priority 0 ptype main
    	tmpl src 0.0.0.0 dst 31.1.2.3
    		proto esp reqid 256 mode tunnel
    src 192.168.2.100/32 dst 192.168.1.4/32
    	dir in priority 0 ptype main
    	tmpl src 0.0.0.0 dst 0.0.0.0
    		proto esp reqid 256 mode tunnel

These are the SPs for the same tunnel as discussed above.

Let's go through the fields:
  - src, dst and dir: what traffic to match, can be a lot more
    complicated (only specific ports, only gre, etc.),
  - tmpl.reqid: which SA to serve this policy with,
  - tmpl.dst: needs to be specified only in out directed SPs.

# Commands to recreate these SAs and SPs

On side A we have the following IPs:
  - private local IP and UDP port: 192.168.1.4:61525
  - public NAT mapped IP and UDP port: 85.9.8.7:61525

On side B we have the following IPs:
  - private local: 192.168.2.100:61523
  - public NAT mapped IP and UDP port: 31.1.2.3:10421

Therefore we need the following two SAs and SPs on side A:

    ip xfrm state add \
      dst 192.168.1.4 src 31.1.2.3 \
      proto esp spi 0xdeadbeef reqid 256 flag af-unspec mode tunnel \
      aead 'rfc4106(gcm(aes))' 0x0000000000000000000000000000000000000000 96 \
      encap espinudp 10421 61525 0.0.0.0
    ip xfrm state add \
      src 192.168.1.4 dst 31.1.2.3 \
      proto esp spi 0xdeadbeef reqid 256 flag af-unspec mode tunnel \
      aead 'rfc4106(gcm(aes))' 0x0000000000000000000000000000000000000000 96 \
      encap espinudp 61525 10421 0.0.0.0

    ip xfrm policy add dst 192.168.1.4/32 src 192.168.2.100/32 dir in tmpl \
      proto esp mode tunnel reqid 256
    ip xfrm policy add src 192.168.1.4/32 dst 192.168.2.100/32 dir out tmpl \
      dst 31.1.2.3 \
      proto esp mode tunnel reqid 256

Symmetric to this, we need two SAs and SPs on side B:

    ip xfrm state add \
      dst 192.168.2.100 src 85.9.8.7 \
      proto esp spi 0xdeadbeef reqid 256 flag af-unspec mode tunnel \
      aead 'rfc4106(gcm(aes))' 0x0000000000000000000000000000000000000000 96 \
      encap espinudp 61525 61523 0.0.0.0
    ip xfrm state add \
      src 192.168.2.100 dst 85.9.8.7 \
      proto esp spi 0xdeadbeef reqid 256 flag af-unspec mode tunnel \
      aead 'rfc4106(gcm(aes))' 0x0000000000000000000000000000000000000000 96 \
      encap espinudp 61523 61525 0.0.0.0

    ip xfrm policy add dst 192.168.2.100/32 src 192.168.1.4/32 dir in tmpl \
      proto esp mode tunnel reqid 256
    ip xfrm policy add src 192.168.2.100/32 dst 192.168.1.4/32 dir out tmpl \
      dst 85.9.8.7 \
      proto esp mode tunnel reqid 256

I minimized the commands to their smallest that are still working.

Things to note:
  - on both sides the knowledge of *your own* public IP and port is not used,
  - the configuration doesn't look symmetric regarding side B's port
    number (61523 vs 10421), this is because the NAT translation on
    side B changed the port number, while side A's NAT translator left
    the port number intact.

# The C command

The final piece of secret sauce is a small daemon program, that you
have to keep running on both sides.  This is not an active piece (has
no loop in it and is not called per-packet), but its open socket
signals the kernel that it has to do the UDP decapsulation on the
socket's UDP port.

The original know-how on how the make these kernel calls is from the
following article:
http://techblog.newsnow.co.uk/2011/11/simple-udp-esp-encapsulation-nat-t-for.html

You can find the original perl script from the article under
`tools/orig_ipsec_decap.pl`.

After understanding the involved parts of the kernel, we simplified a
bit, rewrote it in C and added a command line argument, so you can
specify the UDP port without editing and recompilation.

Use it like this e.g. on side A:

    cd tools
    make
    ./ipsec_udp_decap 61525 31.1.2.3 10421

The tool's only important argument is the first one which specifies
the incoming UDP port for the packets that the kernel needs to
decapsulate.  The optional second and third argument just enables
RFC3948 NAT-Keepalive, so we don't lose our hard earned UDP hole if
there is no traffic in the tunnel.

# Network oddity of the day: asymmetric latency

You can test the IPsec tunnel with pings in both directions.  For me,
the tunnel was asymmetric (my guess is some router/NAT-translator on
the way).  In one direction pings RTT was 10-15ms, the other
150-200ms.  This was not just ping, using SSH I was able to feel the
latency in the slow direction.

On the other hand, just leaving a `ping -i 0.1` running in the fast
direction magically fixed the latency in the slow direction.  Sending
200Mbyte/day is acceptable nowadays, I guess.

# Useful commands

- `ip -s xfrm state`
- `ip -s xfrm policy`
- `ip xfrm monitor`
- `tcpdump -n -i wlan0 port 61525 or icmp`

Note about `tcpdump`:
https://wiki.strongswan.org/projects/strongswan/wiki/CorrectTrafficDump

# Closing notes

None of the discussed techniques are recommended for production: the
IKE daemon is responsible for rekeying frequently, something that one
will definitely not do by hand and therefore the tunnel built with
this technique will be less secure.

If you need tunnels in production behind NAT, you can look into
strongSwan's support for mediators.

# Useful links

The man page of `ip-xfrm`: http://man7.org/linux/man-pages/man8/ip-xfrm.8.html

Foo-over-UDP, to be understood and tested: https://lwn.net/Articles/614348/

IPsec by hand: https://backreference.org/2014/11/12/on-the-fly-ipsec-vpn-with-iproute2/
