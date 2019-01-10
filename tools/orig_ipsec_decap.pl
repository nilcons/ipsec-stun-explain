#!/usr/bin/perl -w

# Stolen from: http://techblog.newsnow.co.uk/2011/11/simple-udp-esp-encapsulation-nat-t-for.html

use strict;

use Socket qw( IPPROTO_IP IPPROTO_UDP AF_INET SOCK_DGRAM SOL_SOCKET SO_REUSEADDR INADDR_ANY sockaddr_in );

my $UDP_ENCAP = 100;

# UDP encapsulation types
my $UDP_ENCAP_ESPINUDP_NON_IKE = 1; # /* draft-ietf-ipsec-nat-t-ike-00/01 */
my $UDP_ENCAP_ESPINUDP = 2; # /* draft-ietf-ipsec-udp-encaps-06 */
my $UDP_ENCAP_L2TPINUDP = 3; # /* rfc2661 */

my $Sock;

socket( $Sock, AF_INET, SOCK_DGRAM, IPPROTO_UDP ) || die "::socket: $!";
setsockopt( $Sock, SOL_SOCKET, SO_REUSEADDR, pack( "l", 1 ) );

# struct sadb_x_policy {
# uint16_t sadb_x_policy_len;
# uint16_t sadb_x_policy_exttype;
# uint16_t sadb_x_policy_type;
# uint8_t sadb_x_policy_dir;
# uint8_t sadb_x_policy_reserved;
# uint32_t sadb_x_policy_id;
# uint32_t sadb_x_policy_priority;
# } __attribute__((packed));
# /* sizeof(struct sadb_x_policy) == 16 */

my $SADB_X_EXT_POLICY = 18;
my $IP_IPSEC_POLICY = 16;
my $IPSEC_POLICY_BYPASS = 4;
my $IPSEC_DIR_INBOUND = 1;
my $IPSEC_DIR_OUTBOUND = 2;

# policy.sadb_x_policy_len = sizeof(policy) / sizeof(u_int64_t);
# policy.sadb_x_policy_exttype = SADB_X_EXT_POLICY;
# policy.sadb_x_policy_type = IPSEC_POLICY_BYPASS;
# policy.sadb_x_policy_dir = IPSEC_DIR_OUTBOUND;

my $policy1 = pack("SSSCCLL", 2, $SADB_X_EXT_POLICY, $IPSEC_POLICY_BYPASS, $IPSEC_DIR_OUTBOUND, 0, 0, 0);
my $policy2 = pack("SSSCCLL", 2, $SADB_X_EXT_POLICY, $IPSEC_POLICY_BYPASS, $IPSEC_DIR_INBOUND, 0, 0, 0);

# See http://strongswan.sourcearchive.com/documentation/4.1.4/socket_8c-source.html
if( defined setsockopt( $Sock, IPPROTO_IP, $IP_IPSEC_POLICY, $policy1 ) ) {
   print "setsockopt:: policy OK\n"; }
else { print "setsockopt:: policy FAIL\n"; }

if( defined setsockopt( $Sock, IPPROTO_IP, $IP_IPSEC_POLICY, $policy2 ) ) {
   print "setsockopt:: policy OK\n"; }
else { print "setsockopt:: policy FAIL\n"; }

if( defined setsockopt( $Sock, IPPROTO_UDP, $UDP_ENCAP, $UDP_ENCAP_ESPINUDP) ) {
   print "setsockopt:: UDP_ENCAP OK\n"; }
else { print "setsockopt:: UDP_ENCAP FAIL\n"; }

bind( $Sock, sockaddr_in( 61525, INADDR_ANY ) ) || die "::bind: $!";

sleep;

1;
