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

#include "igmpproxy.h"

/* We need a temporary copy to not break strict aliasing rules */
static inline uint32_t s_addr_from_sockaddr(const struct sockaddr *addr) {
    struct sockaddr_in addr_in;
    memcpy(&addr_in, addr, sizeof(addr_in));
    return addr_in.sin_addr.s_addr;
}

struct IfDesc IfDescVc[ MAX_IF ], *IfDescEp = IfDescVc;

/* aimwang: add for detect interface and rebuild IfVc record */
/***************************************************
 * TODO:    Only need run me when detect downstream changed.
 *          For example: /etc/ppp/ip-up & ip-down can touch a file /tmp/ppp_changed
 *          So I can check if the file exist then run me and delete the file.
 ***************************************************/
void rebuildIfVc () {
    struct ifreq IfVc[ sizeof( IfDescVc ) / sizeof( IfDescVc[ 0 ] )  ];
    struct ifreq *IfEp;
    struct ifconf IoCtlReq;
    struct IfDesc *Dp;
    struct ifreq  *IfPt, *IfNext;
    uint32_t addr, subnet, mask;
    int Sock;

    // Get the config.
    struct Config *config = getCommonConfig();

    if ( (Sock = socket( AF_INET, SOCK_DGRAM, 0 )) < 0 )
        my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, errno, "RAW socket open" );

    // aimwang: set all downstream IF as lost, for check IF exist or gone.
    for (Dp = IfDescVc; Dp < IfDescEp; Dp++) {
        if (Dp->state == IF_STATE_DOWNSTREAM) {
            Dp->state = IF_STATE_LOST;
        }
    }

    IoCtlReq.ifc_buf = (void *)IfVc;
    IoCtlReq.ifc_len = sizeof( IfVc );

    if ( ioctl( Sock, SIOCGIFCONF, &IoCtlReq ) < 0 )
        my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, errno, "ioctl SIOCGIFCONF" );

    IfEp = (void *)((char *)IfVc + IoCtlReq.ifc_len);

    for ( IfPt = IfVc; IfPt < IfEp; IfPt = IfNext ) {
        struct ifreq IfReq;
        char FmtBu[ 32 ];

        IfNext = (struct ifreq *)((char *)&IfPt->ifr_addr +
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
                IfPt->ifr_addr.sa_len
#else
                sizeof(struct sockaddr_in)
#endif
        );
        if (IfNext < IfPt + 1)
            IfNext = IfPt + 1;

        for (Dp = IfDescVc; Dp < IfDescEp; Dp++) {
            if (0 == strcmp(Dp->Name, IfPt->ifr_name)) {
                break;
            }
        }

        if (Dp == IfDescEp) {
            strncpy( Dp->Name, IfPt->ifr_name, sizeof( IfDescEp->Name ) );
        }

        if ( IfPt->ifr_addr.sa_family != AF_INET ) {
            if (Dp == IfDescEp) {
                IfDescEp++;
            }
            Dp->InAdr.s_addr = 0;  /* mark as non-IP interface */
            continue;
        }

        // Get the interface adress...
        Dp->InAdr.s_addr = s_addr_from_sockaddr(&IfPt->ifr_addr);
        addr = Dp->InAdr.s_addr;

        memcpy( IfReq.ifr_name, Dp->Name, sizeof( IfReq.ifr_name ) );

        if (ioctl(Sock, SIOCGIFINDEX, &IfReq ) < 0)
            my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, errno, "ioctl SIOCGIFINDEX for %s", IfReq.ifr_name);
        Dp->ifIndex = IfReq.ifr_ifindex;

        // Get the subnet mask...
        if (ioctl(Sock, SIOCGIFNETMASK, &IfReq ) < 0)
            my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, errno, "ioctl SIOCGIFNETMASK for %s", IfReq.ifr_name);
        mask = s_addr_from_sockaddr(&IfReq.ifr_addr); // Do not use ifr_netmask as it is not available on freebsd
        subnet = addr & mask;

        if ( ioctl( Sock, SIOCGIFFLAGS, &IfReq ) < 0 )
            my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, errno, "ioctl SIOCGIFFLAGS" );
        Dp->Flags = IfReq.ifr_flags;

        if (0x10d1 == Dp->Flags)
        {
            if ( ioctl( Sock, SIOCGIFDSTADDR, &IfReq ) < 0 )
                my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, errno, "ioctl SIOCGIFDSTADDR for %s", IfReq.ifr_name);
            addr = s_addr_from_sockaddr(&IfReq.ifr_dstaddr);
            subnet = addr & mask;
        }

        if (Dp == IfDescEp) {
            // Insert the verified subnet as an allowed net...
            Dp->allowednets = (struct SubnetList *)malloc(sizeof(struct SubnetList));
            if(IfDescEp->allowednets == NULL) {
                my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, 0, "Out of memory !");
            }
            Dp->allowednets->next = NULL;
            Dp->state         = IF_STATE_DOWNSTREAM;
            Dp->robustness    = DEFAULT_ROBUSTNESS;
            Dp->threshold     = DEFAULT_THRESHOLD;   /* ttl limit */
            Dp->ratelimit     = DEFAULT_RATELIMIT;
        }

        // Set the network address for the IF..
        Dp->allowednets->subnet_mask = mask;
        Dp->allowednets->subnet_addr = subnet;

        // Set the state for the IF...
        if (Dp->state == IF_STATE_LOST) {
            Dp->state         = IF_STATE_DOWNSTREAM;
        }

        // when IF become enabeld from downstream, addVIF to enable its VIF
        if (Dp->state == IF_STATE_HIDDEN) {
            my_log(LOG_NOTICE, COLOR_CODE_WHITE, 0, "%s [Hidden -> Downstream]", Dp->Name);
            Dp->state = IF_STATE_DOWNSTREAM;
            addVIF(Dp);
            k_join(Dp, allrouters_group);
        }

        // addVIF when found new IF
        if (Dp == IfDescEp) {
            my_log(LOG_NOTICE, COLOR_CODE_WHITE, 0, "%s [New]", Dp->Name);
            Dp->state = config->defaultInterfaceState;
            addVIF(Dp);
            k_join(Dp, allrouters_group);
            IfDescEp++;
        }

        // Debug log the result...
        my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "rebuildIfVc: Interface %s Index: %d Addr: %s, Flags: 0x%04x, Network: %s",
            Dp->Name,
            Dp->ifIndex,
            fmtInAdr( FmtBu, Dp->InAdr ),
            Dp->Flags,
            inetFmts(subnet, mask, s1));
    }

    // aimwang: search not longer exist IF, set as hidden and call delVIF
    for (Dp = IfDescVc; Dp < IfDescEp; Dp++) {
        if (IF_STATE_LOST == Dp->state) {
            my_log(LOG_NOTICE, COLOR_CODE_WHITE, 0, "%s [Downstream -> Hidden]", Dp->Name);
            Dp->state = IF_STATE_HIDDEN;
            k_leave(Dp, allrouters_group);
            delVIF(Dp);
        }
    }

    close( Sock );
}

/*
** Builds up a vector with the interface of the machine. Calls to the other functions of
** the module will fail if they are called before the vector is build.
**
*/
void buildIfVc(void) {
    struct ifreq IfVc[ sizeof( IfDescVc ) / sizeof( IfDescVc[ 0 ] )  ];
    struct ifreq *IfEp;
    struct Config *config = getCommonConfig();

    int Sock;

    if ( (Sock = socket( AF_INET, SOCK_DGRAM, 0 )) < 0 )
        my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, errno, "RAW socket open" );

    /* get If vector
     */
    {
        struct ifconf IoCtlReq;

        IoCtlReq.ifc_buf = (void *)IfVc;
        IoCtlReq.ifc_len = sizeof( IfVc );

        if ( ioctl( Sock, SIOCGIFCONF, &IoCtlReq ) < 0 )
            my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, errno, "ioctl SIOCGIFCONF" );

        IfEp = (void *)((char *)IfVc + IoCtlReq.ifc_len);
    }

    /* loop over interfaces and copy interface info to IfDescVc
     */
    {
        struct ifreq  *IfPt, *IfNext;

        // Temp keepers of interface params...
        uint32_t addr, subnet, mask;

        for ( IfPt = IfVc; IfPt < IfEp; IfPt = IfNext ) {
            struct ifreq IfReq;
            char FmtBu[ 32 ];

            IfNext = (struct ifreq *)((char *)&IfPt->ifr_addr +
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
                    IfPt->ifr_addr.sa_len
#else
                    sizeof(struct sockaddr_in)
#endif
            );
            if (IfNext < IfPt + 1)
                IfNext = IfPt + 1;

            strncpy( IfDescEp->Name, IfPt->ifr_name, sizeof( IfDescEp->Name ) );

            // Currently don't set any allowed nets...
            //IfDescEp->allowednets = NULL;

            // Set the index to -1 by default.
            IfDescEp->index = (unsigned int)-1;

            /* don't retrieve more info for non-IP interfaces
             */
            if ( IfPt->ifr_addr.sa_family != AF_INET ) {
                IfDescEp->InAdr.s_addr = 0;  /* mark as non-IP interface */
                IfDescEp++;
                continue;
            }

            // Get the interface adress...
            IfDescEp->InAdr.s_addr = s_addr_from_sockaddr(&IfPt->ifr_addr);
            addr = IfDescEp->InAdr.s_addr;

            memcpy( IfReq.ifr_name, IfDescEp->Name, sizeof( IfReq.ifr_name ) );

            if (ioctl(Sock, SIOCGIFINDEX, &IfReq ) < 0)
                my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, errno, "ioctl SIOCGIFINDEX for %s", IfReq.ifr_name);
            IfDescEp->ifIndex = IfReq.ifr_ifindex;

            // Get the subnet mask...
            if (ioctl(Sock, SIOCGIFNETMASK, &IfReq ) < 0)
                my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, errno, "ioctl SIOCGIFNETMASK for %s", IfReq.ifr_name);
            mask = s_addr_from_sockaddr(&IfReq.ifr_addr); // Do not use ifr_netmask as it is not available on freebsd
            subnet = addr & mask;

            /* get if flags
            **
            ** typical flags:
            ** lo    0x0049 -> Running, Loopback, Up
            ** ethx  0x1043 -> Multicast, Running, Broadcast, Up
            ** ipppx 0x0091 -> NoArp, PointToPoint, Up
            ** grex  0x00C1 -> NoArp, Running, Up
            ** ipipx 0x00C1 -> NoArp, Running, Up
            */
            if ( ioctl( Sock, SIOCGIFFLAGS, &IfReq ) < 0 )
                my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, errno, "ioctl SIOCGIFFLAGS" );

            IfDescEp->Flags = IfReq.ifr_flags;

            // aimwang: when pppx get dstaddr for use
            if (0x10d1 == IfDescEp->Flags)
            {
                if ( ioctl( Sock, SIOCGIFDSTADDR, &IfReq ) < 0 )
                    my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, errno, "ioctl SIOCGIFDSTADDR for %s", IfReq.ifr_name);
                addr = s_addr_from_sockaddr(&IfReq.ifr_dstaddr);
                subnet = addr & mask;
            }

            // Insert the verified subnet as an allowed net...
            IfDescEp->allowednets = (struct SubnetList *)malloc(sizeof(struct SubnetList));
            if(IfDescEp->allowednets == NULL) my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, 0, "Out of memory !");

            // Create the network address for the IF..
            IfDescEp->allowednets->next = NULL;
            IfDescEp->allowednets->subnet_mask = mask;
            IfDescEp->allowednets->subnet_addr = subnet;

            // Set the default params for the IF...
            IfDescEp->state         = config->defaultInterfaceState;
            IfDescEp->robustness    = DEFAULT_ROBUSTNESS;
            IfDescEp->threshold     = DEFAULT_THRESHOLD;   /* ttl limit */
            IfDescEp->ratelimit     = DEFAULT_RATELIMIT;

            // Debug log the result...
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "buildIfVc: Interface %s Index: %d Addr: %s, Flags: 0x%04x, Network: %s",
                 IfDescEp->Name,
                 IfDescEp->ifIndex,
                 fmtInAdr( FmtBu, IfDescEp->InAdr ),
                 IfDescEp->Flags,
                 inetFmts(subnet,mask, s1));

            IfDescEp++;
        }
    }

    close( Sock );
}

/*
** Returns a pointer to the IfDesc of the interface 'IfName'
**
** returns: - pointer to the IfDesc of the requested interface
**          - NULL if no interface 'IfName' exists
**
*/
struct IfDesc *getIfByName( const char *IfName ) {
    struct IfDesc *Dp;

    for ( Dp = IfDescVc; Dp < IfDescEp; Dp++ )
        if ( ! strcmp( IfName, Dp->Name ) )
            return Dp;

    return NULL;
}

/*
** Returns a pointer to the IfDesc of the interface 'Ix'
**
** returns: - pointer to the IfDesc of the requested interface
**          - NULL if no interface 'Ix' exists
**
*/
struct IfDesc *getIfByIx( unsigned Ix ) {
    struct IfDesc *Dp = &IfDescVc[ Ix ];
    return Dp < IfDescEp ? Dp : NULL;
}

/**
*   Returns a pointer to the IfDesc whose subnet matches
*   the supplied IP adress. The IP must match a interfaces
*   subnet, or any configured allowed subnet on a interface.
*/
struct IfDesc *getIfByAddress( uint32_t ipaddr ) {

    struct IfDesc       *Dp;
    struct SubnetList   *currsubnet;
    struct IfDesc       *res = NULL;
    uint32_t            last_subnet_mask = 0;

    for ( Dp = IfDescVc; Dp < IfDescEp; Dp++ ) {
        // Loop through all registered allowed nets of the VIF...
        for(currsubnet = Dp->allowednets; currsubnet != NULL; currsubnet = currsubnet->next) {
            // Check if the ip falls in under the subnet....
            if(currsubnet->subnet_mask > last_subnet_mask && (ipaddr & currsubnet->subnet_mask) == currsubnet->subnet_addr) {
                res = Dp;
                last_subnet_mask = currsubnet->subnet_mask;
            }
        }
    }
    return res;
}


/**
*   Returns a pointer to the IfDesc whose subnet matches
*   the supplied IP adress. The IP must match a interfaces
*   subnet, or any configured allowed subnet on a interface.
*/
struct IfDesc *getIfByVifIndex( unsigned vifindex ) {
    struct IfDesc       *Dp;
    if(vifindex>0) {
        for ( Dp = IfDescVc; Dp < IfDescEp; Dp++ ) {
            if(Dp->index == vifindex) {
                return Dp;
            }
        }
    }
    return NULL;
}


/**
*   Function that checks if a given ipaddress is a valid
*   address for the supplied VIF.
*/
int isAdressValidForIf( struct IfDesc* intrface, uint32_t ipaddr ) {
    struct SubnetList   *currsubnet;

    if(intrface == NULL) {
        return 0;
    }

    // Loop through all registered allowed nets of the VIF...
    for(currsubnet = intrface->allowednets; currsubnet != NULL; currsubnet = currsubnet->next) {
        // Check if the ip falls in under the subnet....
        if((ipaddr & currsubnet->subnet_mask) == (currsubnet->subnet_addr& currsubnet->subnet_mask)) {
            return 1;
        }
    }
    return 0;
}
