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
*   config.c - Contains functions to load and parse config
*              file, and functions to configure the daemon.
*/

#include "igmpproxy.h"

// Structure to keep configuration for VIFs...
struct vifconfig {
    char*               name;
    short               state;
    int                 ratelimit;
    int                 threshold;

    // Keep allowed nets for VIF.
    struct SubnetList*  allowednets;

    // Allowed Groups
    struct SubnetList*  allowedgroups;

    // Next config in list...
    struct vifconfig*   next;
};

// Structure to keep vif configuration
struct vifconfig *vifconf;

// Keeps common settings...
static struct Config commonConfig;

// Prototypes...
struct vifconfig *parsePhyintToken(void);
struct SubnetList *parseSubnetAddress(char *addrstr);

/**
*   Initializes common config..
*/
static void initCommonConfig(void) {
    commonConfig.robustnessValue = DEFAULT_ROBUSTNESS;
    commonConfig.queryInterval = INTERVAL_QUERY;
    commonConfig.queryResponseInterval = INTERVAL_QUERY_RESPONSE;

    // The defaults are calculated from other settings.
    commonConfig.startupQueryInterval = (unsigned int)(INTERVAL_QUERY / 4);
    commonConfig.startupQueryCount = DEFAULT_ROBUSTNESS;

    // Default values for leave intervals...
    commonConfig.lastMemberQueryInterval = INTERVAL_QUERY_RESPONSE;
    commonConfig.lastMemberQueryCount    = DEFAULT_ROBUSTNESS;

    // If 1, a leave message is sent upstream on leave messages from downstream.
    commonConfig.fastUpstreamLeave = 0;

    // Default size of hash table is 32 bytes (= 256 bits) and can store
    // up to the 256 non-collision hosts, approximately half of /24 subnet
    commonConfig.downstreamHostsHashTableSize = 32;

    // aimwang: default value
    commonConfig.defaultInterfaceState = IF_STATE_DISABLED;
    commonConfig.rescanVif = 0;
}

/**
*   Returns a pointer to the common config...
*/
struct Config *getCommonConfig(void) {
    return &commonConfig;
}

/**
*   Loads the configuration from file, and stores the config in
*   respective holders...
*/
int loadConfig(char *configFile) {
    struct vifconfig  *tmpPtr;
    struct vifconfig  **currPtr = &vifconf;
    char *token;

    // Initialize common config
    initCommonConfig();

    // Test config file reader...
    if(!openConfigFile(configFile)) {
        my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, 0, "Unable to open configfile from %s", configFile);
    }

    // Get first token...
    token = nextConfigToken();
    if(token == NULL) {
        my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, 0, "Config file was empty.");
    }

    // Loop until all configuration is read.
    while ( token != NULL ) {
        // Check token...
        if(strcmp("phyint", token)==0) {
            // Got a phyint token... Call phyint parser
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: Got a phyint token.");
            tmpPtr = parsePhyintToken();
            if(tmpPtr == NULL) {
                // Unparsable token... Exit...
                closeConfigFile();
                my_log(LOG_WARNING, COLOR_CODE_WHITE, 0, "Unknown token '%s' in configfile", token);
                return 0;
            } else {

                my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "IF name : %s", tmpPtr->name);
                my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Next ptr : %x", tmpPtr->next);
                my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Ratelimit : %d", tmpPtr->ratelimit);
                my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Threshold : %d", tmpPtr->threshold);
                my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "State : %d", tmpPtr->state);
                my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Allowednet ptr : %x", tmpPtr->allowednets);

                // Insert config, and move temppointer to next location...
                *currPtr = tmpPtr;
                currPtr = &tmpPtr->next;
            }
        }
        else if(strcmp("quickleave", token)==0) {
            // Got a quickleave token....
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: Quick leave mode enabled.");
            commonConfig.fastUpstreamLeave = 1;

            // Read next token...
            token = nextConfigToken();
            continue;
        }
        else if(strcmp("hashtablesize", token)==0) {
            // Got a hashtablesize token...
            token = nextConfigToken();
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: hashtablesize for quickleave is %s.", token);
            if(!commonConfig.fastUpstreamLeave) {
                closeConfigFile();
                my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, 0, "Config: hashtablesize is specified but quickleave not enabled.");
                return 0;
            }
            int intToken = atoi(token);
            if(intToken < 1 || intToken > 536870912) {
                closeConfigFile();
                my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, 0, "Config: hashtablesize must be between 1 and 536870912 bytes.");
                return 0;
            }
            commonConfig.downstreamHostsHashTableSize = intToken;

            // Read next token...
            token = nextConfigToken();
            continue;
        }
        else if(strcmp("defaultdown", token)==0) {
            // Got a defaultdown token...
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: interface Default as down stream.");
            commonConfig.defaultInterfaceState = IF_STATE_DOWNSTREAM;

            // Read next token...
            token = nextConfigToken();
            continue;
        }
        else if(strcmp("rescanvif", token)==0) {
            // Got a rescanvif token...
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: Need detect new interface.");
            commonConfig.rescanVif = 1;

            // Read next token...
            token = nextConfigToken();
            continue;
        }
        else if(strcmp("chroot", token)==0) {
            // path is in next token
            token = nextConfigToken();

            if (snprintf(commonConfig.chroot, sizeof(commonConfig.chroot), "%s",
              token) >= (int)sizeof(commonConfig.chroot))
                my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, 0, "Config: chroot is truncated");

            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: chroot set to %s",
              commonConfig.chroot);
            token = nextConfigToken();
            continue;
        }
        else if(strcmp("user", token)==0) {
            // username is in next token
            token = nextConfigToken();

            if (snprintf(commonConfig.user, sizeof(commonConfig.user), "%s",
              token) >= (int)sizeof(commonConfig.user))
                my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, 0, "Config: user is truncated");

            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: user set to %s", commonConfig.user);
            token = nextConfigToken();
            continue;
        } else {
            // Unparsable token... Exit...
            closeConfigFile();
            my_log(LOG_WARNING, COLOR_CODE_WHITE, 0, "Unknown token '%s' in configfile", token);
            return 0;
        }
        // Get token that was not recognized by phyint parser.
        token = getCurrentConfigToken();
    }

    // Close the configfile...
    closeConfigFile();

    return 1;
}

/**
*   Appends extra VIF configuration from config file.
*/
void configureVifs(void) {
    unsigned Ix;
    struct IfDesc *Dp;
    struct vifconfig *confPtr;

    // If no config is available, just return...
    if(vifconf == NULL) {
        return;
    }

    // Loop through all VIFs...
    for ( Ix = 0; (Dp = getIfByIx(Ix)); Ix++ ) {
        if ( Dp->InAdr.s_addr && ! (Dp->Flags & IFF_LOOPBACK) ) {

            // Now try to find a matching config...
            for( confPtr = vifconf; confPtr; confPtr = confPtr->next) {

                // I the VIF names match...
                if(strcmp(Dp->Name, confPtr->name)==0) {
                    struct SubnetList *vifLast;

                    my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Found config for %s", Dp->Name);


                    // Set the VIF state
                    Dp->state = confPtr->state;

                    Dp->threshold = confPtr->threshold;
                    Dp->ratelimit = confPtr->ratelimit;

                    // Go to last allowed net on VIF...
                    for(vifLast = Dp->allowednets; vifLast->next; vifLast = vifLast->next);

                    // Insert the configured nets...
                    vifLast->next = confPtr->allowednets;

                    Dp->allowedgroups = confPtr->allowedgroups;

                    break;
                }
            }
        }
    }
}


/**
*   Internal function to parse phyint config
*/
struct vifconfig *parsePhyintToken(void) {
    struct vifconfig  *tmpPtr;
    struct SubnetList **anetPtr, **agrpPtr;
    char *token;
    short parseError = 0;

    // First token should be the interface name....
    token = nextConfigToken();

    // Sanitycheck the name...
    if(token == NULL) return NULL;
    if(strlen(token) >= IF_NAMESIZE) return NULL;
    my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: IF: Config for interface %s.", token);

    // Allocate memory for configuration...
    tmpPtr = (struct vifconfig*)malloc(sizeof(struct vifconfig));
    if(tmpPtr == NULL) {
        my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, 0, "Out of memory.");
    }

    // Set default values...
    tmpPtr->next = NULL;    // Important to avoid seg fault...
    tmpPtr->ratelimit = 0;
    tmpPtr->threshold = 1;
    tmpPtr->state = commonConfig.defaultInterfaceState;
    tmpPtr->allowednets = NULL;
    tmpPtr->allowedgroups = NULL;

    // Make a copy of the token to store the IF name
    tmpPtr->name = strdup( token );
    if(tmpPtr->name == NULL) {
        my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, 0, "Out of memory.");
    }

    // Set the altnet pointer to the allowednets pointer.
    anetPtr = &tmpPtr->allowednets;
    agrpPtr = &tmpPtr->allowedgroups;

    // Parse the rest of the config..
    token = nextConfigToken();
    while(token != NULL) {
        if(strcmp("altnet", token)==0) {
            // Altnet...
            token = nextConfigToken();
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: IF: Got altnet token %s.",token);

            *anetPtr = parseSubnetAddress(token);
            if(*anetPtr == NULL) {
                parseError = 1;
                my_log(LOG_WARNING, COLOR_CODE_WHITE, 0, "Unable to parse subnet address.");
                break;
            } else {
                anetPtr = &(*anetPtr)->next;
            }
        }
        else if(strcmp("whitelist", token)==0) {
            // Whitelist
            token = nextConfigToken();
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: IF: Got whitelist token %s.", token);

            *agrpPtr = parseSubnetAddress(token);
            if(*agrpPtr == NULL) {
                free(tmpPtr->name);
                free(tmpPtr);
                my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, 0, "Unable to parse subnet address.");
            } else {
                (*agrpPtr)->allow = true;
                agrpPtr = &(*agrpPtr)->next;
            }
        }
        else if(strcmp("blacklist", token)==0) {
            // Blacklist
            token = nextConfigToken();
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: IF: Got blacklist token %s.", token);

            *agrpPtr = parseSubnetAddress(token);
            if(*agrpPtr == NULL) {
                free(tmpPtr->name);
                free(tmpPtr);
                my_log(LOG_ERR, COLOR_CODE_BRIGHT_RED, 0, "Unable to parse subnet address.");
            } else {
                (*agrpPtr)->allow = false;
                agrpPtr = &(*agrpPtr)->next;
            }
        }
        else if(strcmp("upstream", token)==0) {
            // Upstream
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: IF: Got upstream token.");
            tmpPtr->state = IF_STATE_UPSTREAM;
        }
        else if(strcmp("downstream", token)==0) {
            // Downstream
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: IF: Got downstream token.");
            tmpPtr->state = IF_STATE_DOWNSTREAM;
        }
        else if(strcmp("disabled", token)==0) {
            // Disabled
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: IF: Got disabled token.");
            tmpPtr->state = IF_STATE_DISABLED;
        }
        else if(strcmp("ratelimit", token)==0) {
            // Ratelimit
            token = nextConfigToken();
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: IF: Got ratelimit token '%s'.", token);
            tmpPtr->ratelimit = atoi( token );
            if(tmpPtr->ratelimit < 0) {
                my_log(LOG_WARNING, COLOR_CODE_WHITE, 0, "Ratelimit must be 0 or more.");
                parseError = 1;
                break;
            }
        }
        else if(strcmp("threshold", token)==0) {
            // Threshold
            token = nextConfigToken();
            my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: IF: Got threshold token '%s'.", token);
            tmpPtr->threshold = atoi( token );
            if(tmpPtr->threshold <= 0 || tmpPtr->threshold > 255) {
                my_log(LOG_WARNING, COLOR_CODE_WHITE, 0, "Threshold must be between 1 and 255.");
                parseError = 1;
                break;
            }
        }
        else {
            // Unknown token. Break...
            break;
        }
        token = nextConfigToken();
    }

    // Clean up after a parseerror...
    if(parseError) {
        free(tmpPtr->name);
        free(tmpPtr);
        tmpPtr = NULL;
    }

    return tmpPtr;
}

/**
*   Parses a subnet address string on the format
*   a.b.c.d/n into a SubnetList entry.
*/
struct SubnetList *parseSubnetAddress(char *addrstr) {
    struct SubnetList   *tmpSubnet;
    char                *tmpStr;
    uint32_t            addr = 0x00000000;
    uint32_t            mask = 0xFFFFFFFF;

    // First get the network part of the address...
    tmpStr = strtok(addrstr, "/");
    addr = inet_addr(tmpStr);

    tmpStr = strtok(NULL, "/");
    if(tmpStr != NULL) {
        int bitcnt = atoi(tmpStr);
        if(bitcnt < 0 || bitcnt > 32) {
            my_log(LOG_WARNING, COLOR_CODE_WHITE, 0, "The bits part of the address is invalid : %d.",tmpStr);
            return NULL;
        }

        if (bitcnt == 0)
            mask = 0;
        else
            mask <<= (32 - bitcnt);
    }

    if(addr == (uint32_t)-1) {
        my_log(LOG_WARNING, COLOR_CODE_WHITE, 0, "Unable to parse address token '%s'.", addrstr);
        return NULL;
    }

    tmpSubnet = (struct SubnetList*) malloc(sizeof(struct SubnetList));
    tmpSubnet->subnet_addr = addr;
    tmpSubnet->subnet_mask = ntohl(mask);
    tmpSubnet->next = NULL;

    my_log(LOG_DEBUG, COLOR_CODE_WHITE, 0, "Config: IF: Altnet: Parsed altnet to %s.",
            inetFmts(tmpSubnet->subnet_addr, tmpSubnet->subnet_mask,s1));

    return tmpSubnet;
}
