/*
**  igmpproxy - IGMP proxy based multicast router
**  Copyright (C) 2005 Johnny Egeland <johnny@rlo.org>
**
**  This program is free software; you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation; either version 2 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with this program; if not, write to the Free Software
**  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
**
**----------------------------------------------------------------------------
**
**  This software is derived work from the following software. The original
**  source code has been modified from it's original state by the author
**  of igmpproxy.
**
**  smcroute 0.92 - Copyright (C) 2001 Carsten Schill <carsten@cschill.de>
**  - Licensed under the GNU General Public License, either version 2 or
**    any later version.
**
**  mrouted 3.9-beta3 - Copyright (C) 2002 by The Board of Trustees of
**  Leland Stanford Junior University.
**  - Licensed under the 3-clause BSD license, see Stanford.txt file.
**
*/
/**
*   igmpproxy.h - Header file for common includes.
*/

#include "config.h"
#include "os.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <stdbool.h>
#include <time.h>
#include <grp.h>
#include <limits.h>
#include <pwd.h>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/select.h>

#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/*
 * Limit on length of route data
 */
#define MAX_IP_PACKET_LEN	576
#define MIN_IP_HEADER_LEN	20
#define MAX_IP_HEADER_LEN	60
#define IP_HEADER_RAOPT_LEN	24

#define MAX_UPS_VIFS    8

// Useful macros..
#define VCMC( Vc )  (sizeof( Vc ) / sizeof( (Vc)[ 0 ] ))
#define VCEP( Vc )  (&(Vc)[ VCMC( Vc ) ])

// Bit manipulation macros...
#define BIT_ZERO(X)      ((X) = 0)
#define BIT_SET(X,n)     ((X) |= 1 << (n))
#define BIT_CLR(X,n)     ((X) &= ~(1 << (n)))
#define BIT_TST(X,n)     ((X) & 1 << (n))


//#################################################################################
//  Globals
//#################################################################################

/*
 * External declarations for global variables and functions.
 */
#define RECV_BUF_SIZE 8192
extern char     *recv_buf;
extern char     *send_buf;

extern char     s1[];
extern char     s2[];
extern char		s3[];
extern char		s4[];

//#################################################################################
//  LUIS
//#################################################################################

/*
 * To colorize the log messages
 */
// Define color codes
#define COLOR_CODE_RESET          0
#define COLOR_CODE_RED            1
#define COLOR_CODE_GREEN          2
#define COLOR_CODE_YELLOW         3
#define COLOR_CODE_BLUE           4
#define COLOR_CODE_MAGENTA        5
#define COLOR_CODE_CYAN           6
#define COLOR_CODE_WHITE          7
#define COLOR_CODE_BRIGHT_RED     8
#define COLOR_CODE_BRIGHT_GREEN   9
#define COLOR_CODE_BRIGHT_YELLOW  10
#define COLOR_CODE_BRIGHT_BLUE    11
#define COLOR_CODE_BRIGHT_MAGENTA 12
#define COLOR_CODE_BRIGHT_CYAN    13
#define COLOR_CODE_BRIGHT_WHITE   14

/*
 * Movistar+ IPTV Workaround Configuration
 * 
 * These defines control the periodic refresh mechanism used to work around
 * the fact that Movistar+ upstream routers do not send IGMP Membership Queries.
 * Without periodic queries, group memberships timeout after 260 seconds,
 * causing TV streams to freeze every ~4.5 minutes.
 * 
 * The workaround sends periodic IGMP Membership Reports upstream to maintain
 * active group memberships and prevent timeouts.
 */

// Initial delay before starting periodic refresh (seconds)
#define MOVISTAR_REFRESH_INITIAL_DELAY    10

// Interval between periodic refresh cycles (seconds)
// Should be less than 260 seconds to prevent upstream timeouts
#define MOVISTAR_REFRESH_INTERVAL         60

// Enable/disable the Movistar+ workaround
#define MOVISTAR_WORKAROUND_ENABLED       1

// Map color codes to ANSI escape sequences
/**
const char* color_codes[] = {
    "\x1b[0m",   // Reset
    "\x1b[31m",  // Red
    "\x1b[32m",  // Green
    "\x1b[33m",  // Yellow
    "\x1b[34m",  // Blue
    "\x1b[35m",  // Magenta
    "\x1b[36m",  // Cyan
    "\x1b[37m",  // White
    "\x1b[91m",  // Bright Red
    "\x1b[92m",  // Bright Green
    "\x1b[93m",  // Bright Yellow
    "\x1b[94m",  // Bright Blue
    "\x1b[95m",  // Bright Magenta
    "\x1b[96m",  // Bright Cyan
    "\x1b[97m"   // Bright White
};
*/

//#################################################################################
//  Lib function prototypes.
//#################################################################################

/* syslog.c
 */
extern bool Log2Stderr;           // Log to stderr instead of to syslog
extern int  LogLevel;             // Log threshold, LOG_WARNING .... LOG_DEBUG

void my_log( int Severity, int color_code, int Errno, const char *FmtSt, ...);

/* ifvc.c
 */
#define MAX_IF         40     // max. number of interfaces recognized

// Interface states
#define IF_STATE_DISABLED      0   // Interface should be ignored.
#define IF_STATE_UPSTREAM      1   // Interface is the upstream interface
#define IF_STATE_DOWNSTREAM    2   // Interface is a downstream interface
#define IF_STATE_LOST          3   // aimwang: Temp from downstream to hidden
#define IF_STATE_HIDDEN        4   // aimwang: Interface is hidden

// Multicast default values...
#define DEFAULT_ROBUSTNESS     2
#define DEFAULT_THRESHOLD      1
#define DEFAULT_RATELIMIT      0

// Define timer constants (in seconds...)
#define INTERVAL_QUERY          125
#define INTERVAL_QUERY_RESPONSE  10
//#define INTERVAL_QUERY_RESPONSE  10

#define ROUTESTATE_NOTJOINED            0   // The group corresponding to route is not joined
#define ROUTESTATE_JOINED               1   // The group corresponding to route is joined
#define ROUTESTATE_CHECK_LAST_MEMBER    2   // The router is checking for hosts



// Linked list of networks...
struct SubnetList {
    uint32_t            subnet_addr;
    uint32_t            subnet_mask;
    struct SubnetList   *next;
    bool                allow;
};

struct IfDesc {
    char                Name[IF_NAMESIZE];
    struct in_addr      InAdr;          /* == 0 for non IP interfaces */
    int                 ifIndex;
    short               Flags;
    short               state;
    struct SubnetList*  allowednets;
    struct SubnetList*  allowedgroups;
    unsigned int        robustness;
    unsigned char       threshold;   /* ttl limit */
    unsigned int        ratelimit;
    unsigned int        index;
};

// Keeps common configuration settings
struct Config {
    unsigned int        robustnessValue;
    unsigned int        queryInterval;
    unsigned int        queryResponseInterval;
    // Used on startup..
    unsigned int        startupQueryInterval;
    unsigned int        startupQueryCount;
    // Last member probe...
    unsigned int        lastMemberQueryInterval;
    unsigned int        lastMemberQueryCount;
    // Set if upstream leave messages should be sent instantly..
    unsigned short      fastUpstreamLeave;
    // Size in bytes of hash table of downstream hosts used for fast leave
    unsigned int        downstreamHostsHashTableSize;
    //~ aimwang added
    // Set if nneed to detect new interface.
    unsigned short	rescanVif;
    // Set if not detect new interface for down stream.
    unsigned short	defaultInterfaceState;	// 0: disable, 2: downstream
    //~ aimwang added done
    char                chroot[PATH_MAX];
    char                user[LOGIN_NAME_MAX];
};

// Holds the indeces of the upstream IF...
extern int upStreamIfIdx[MAX_UPS_VIFS];

/* ifvc.c
 */
void rebuildIfVc( void );
void buildIfVc( void );
struct IfDesc *getIfByName( const char *IfName );
struct IfDesc *getIfByIx( unsigned Ix );
struct IfDesc *getIfByAddress( uint32_t Ix );
struct IfDesc *getIfByVifIndex( unsigned vifindex );
int isAdressValidForIf(struct IfDesc* intrface, uint32_t ipaddr);

/* mroute-api.c
 */
struct MRouteDesc {
    struct in_addr  OriginAdr, McAdr;
    short           InVif;
    uint8_t         TtlVc[MAXVIFS];
};

// IGMP socket as interface for the mrouted API
// - receives the IGMP messages
extern int MRouterFD;

int enableMRouter( void );
void disableMRouter( void );
void addVIF( struct IfDesc *Dp );
void delVIF( struct IfDesc *Dp );
int addMRoute( struct MRouteDesc * Dp );
int delMRoute( struct MRouteDesc * Dp );
int getVifIx( struct IfDesc *IfDp );

/* config.c
 */
int loadConfig(char *configFile);
void configureVifs(void);
struct Config *getCommonConfig(void);

/* igmp.c
*/
extern uint32_t allhosts_group;
extern uint32_t allrouters_group;
extern uint32_t alligmp3_group;
void initIgmp(void);
void acceptIgmp(int);
void sendIgmp (uint32_t, uint32_t, int, int, uint32_t, int, int);

/* lib.c
 */
char   *fmtInAdr( char *St, struct in_addr InAdr );
char   *inetFmt(uint32_t addr, char *s);
char   *inetFmts(uint32_t addr, uint32_t mask, char *s);
uint16_t inetChksum(uint16_t *addr, int len);

/* kern.c
 */
void k_set_rcvbuf(int bufsize, int minsize);
void k_hdr_include(int hdrincl);
void k_set_ttl(int t);
void k_set_loop(int l);
void k_set_if(uint32_t ifa, int ifidx);
void k_join(struct IfDesc *ifd, uint32_t grp);
void k_leave(struct IfDesc *ifd, uint32_t grp);

/* rttable.c
 */
void initRouteTable(void);
void clearAllRoutes(void);
int insertRoute(uint32_t group, int ifx, uint32_t src);
int activateRoute(uint32_t group, uint32_t originAddr, int upstrVif);
void ageActiveRoutes(void);
void setRouteLastMemberMode(uint32_t group, uint32_t src);
int lastMemberGroupAge(uint32_t group);
int interfaceInRoute(int32_t group, int Ix);
/*
 * Movistar+ IPTV Workaround Functions
 * 
 * These functions implement a workaround for Movistar+ IPTV service where
 * upstream routers do not send IGMP Membership Queries. The workaround
 * sends periodic IGMP Membership Reports upstream to maintain active
 * group memberships and prevent timeouts.
 */
void sendDecoJoinsUpstream(void *data);
void setupTimerNextDecoJoinsUpstream(int interval);

/* request.c
 */
void acceptGroupReport(uint32_t src, uint32_t group);
void acceptLeaveMessage(uint32_t src, uint32_t group);
void sendGeneralMembershipQuery(void);

/* callout.c 
*/
typedef void (*timer_f)(void *);

void callout_init(void);
void free_all_callouts(void);
void age_callout_queue(int);
int timer_nextTimer(void);
int timer_setTimer(int, timer_f, void *);
int timer_clearTimer(int);
int timer_leftTimer(int);

/* confread.c
 */
#define MAX_TOKEN_LENGTH    30

int openConfigFile(char *filename);
void closeConfigFile(void);
char* nextConfigToken(void);
char* getCurrentConfigToken(void);
