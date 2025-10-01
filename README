# IGMPproxy - Movistar+ IPTV Edition

A specialized fork of igmpproxy designed specifically for Movistar+ IPTV service in Spain. This version includes workarounds to address compatibility issues with Movistar's multicast infrastructure.

## Overview

This is a fork of the original igmpproxy multicast router, modified to work reliably with Movistar+ IPTV service. The original igmpproxy is a simple multicast router that uses only the IGMP protocol for dynamic multicast traffic routing.

**Important Note**: This fork is specifically tailored for Movistar+ IPTV service and is not intended for general use or upstream contribution to the original project.

## Supported Operating Systems

- Linux
- FreeBSD
- NetBSD
- OpenBSD
- DragonFly BSD

## License

This software is released under the GNU GPL license v2 or later. See details in COPYING.

## Movistar+ Specific Modifications

### Problem Statement

Movistar+ IPTV service has a critical compatibility issue with standard IGMP proxy implementations:

- **Missing IGMP Queries**: Movistar's upstream routers do not send IGMP Membership Queries
- **Timeout Issue**: Without periodic queries, group memberships timeout after 260 seconds
- **Service Interruption**: TV streams freeze every ~4.5 minutes, making the service unusable
- **Workaround Required**: Users must change channels to restart streams temporarily

### Technical Solution

I've implemented in this fork a workaround that maintains active multicast group memberships without relying on upstream IGMP queries:

#### 1. Periodic Membership Refresh
- **Automated Refresh**: Sends periodic IGMP Membership Reports upstream every 60 seconds
- **State-Aware Processing**: Only refreshes groups that are actively joined upstream
- **Efficient Filtering**: Skips groups without downstream listeners to minimize network traffic

#### 2. Flicker-Free Implementation
- **IGMP Reports**: Uses standard IGMP Membership Reports instead of kernel-level Leave/Join operations
- **No Stream Interruption**: Maintains continuous multicast streams without flickering
- **Transparent Operation**: The workaround is completely invisible to end users

#### 3. Configurable Parameters
- **Initial Delay**: 10-second startup delay before beginning refresh cycles
- **Refresh Interval**: 60-second intervals between membership refreshes
- **Enable/Disable**: Can be easily disabled via compile-time configuration

### Architecture

The solution works by implementing a periodic timer that sends IGMP Membership Reports for all active multicast groups upstream. This prevents the 260-second timeout that occurs when upstream routers don't send periodic queries.

```
Movistar+ Backend Router
         |
         | (No IGMP Queries sent)
         |
    Home Router (igmpproxy)
         |
    ┌────┼────┐
    |    |    |
   D1   D2   D3  (Decos/Set-top boxes)
```

**Key Components**:
- **Membership Refresh Timer**: Periodic function that refreshes upstream memberships
- **State Management**: Tracks which groups are actively joined upstream
- **IGMP Report Generation**: Sends standard IGMP Membership Reports upstream
- **Efficient Processing**: Only processes groups with active downstream listeners

## Installation and Usage

### Compilation

```bash
git clone https://github.com/LuisPalacios/igmpproxy.git
cd igmpproxy
./autogen.sh
./configure
make
```

### Installation

```bash
# Remove system igmpproxy to avoid conflicts
sudo apt purge igmpproxy

# Install custom version
sudo make install
```

### Configuration

Create `/etc/systemd/system/igmpproxy.service`:

```ini
[Unit]
Description=Movistar+ Compatible IGMP Proxy
Documentation=https://github.com/LuisPalacios/igmpproxy
After=network-online.target

[Service]
EnvironmentFile=-/etc/default/igmpproxy
ExecStart=/usr/local/sbin/igmpproxy $IGMPPROXY_OPTS
ExecReload=/bin/kill -HUP $MAINPID
Restart=always
RestartSec=30
KillMode=control-group

[Install]
WantedBy=multi-user.target
```

Create `/etc/default/igmpproxy`:

```bash
# Options for Movistar+ compatible igmpproxy
IGMPPROXY_OPTS="-n /etc/igmpproxy.conf"
```

Create `/etc/igmpproxy.conf`:

```conf
# Movistar+ IPTV Configuration
# Upstream: vlan2 (Movistar+ IPTV network)
# Downstream: vlan10 (Local LAN with Decos)
quickleave
phyint vlan2 upstream ratelimit 0 threshold 3
    altnet 172.0.0.0/8
phyint vlan10 downstream ratelimit 0 threshold 3
phyint lo disabled
phyint ppp0 disabled
phyint vlan3 disabled
phyint vlan6 disabled
```

### Service Management

```bash
sudo systemctl daemon-reload
sudo systemctl enable igmpproxy
sudo systemctl start igmpproxy
```

## Technical Details

### IGMP Protocol Behavior

In standard IGMP implementations:
- **Join**: When a client wants to receive a multicast stream, it sends a Join message
- **Leave**: When no clients are interested, a Leave message is sent upstream
- **Query**: Upstream routers periodically send Queries to check active memberships
- **Report**: Clients respond to Queries with Reports indicating active subscriptions

### Movistar+ Issue

Movistar's infrastructure lacks the Query/Report cycle:
- No periodic IGMP Queries are sent from upstream
- Group memberships timeout after 260 seconds without refresh
- Standard igmpproxy implementations cannot maintain active memberships
- Results in service interruption every ~4.5 minutes

### Solution Implementation

This fork addresses the issue by:
1. **Proactive Refresh**: Sends periodic IGMP Membership Reports upstream
2. **Standard Compliance**: Uses proper IGMP protocol mechanisms
3. **Efficiency**: Only refreshes groups with active downstream listeners
4. **Reliability**: Prevents timeout-based service interruptions

## Configuration Options

The workaround can be configured via compile-time defines in `src/igmpproxy.h`:

- `MOVISTAR_REFRESH_INITIAL_DELAY`: Initial delay before starting refresh cycles (default: 10 seconds)
- `MOVISTAR_REFRESH_INTERVAL`: Interval between refresh cycles (default: 60 seconds)
- `MOVISTAR_WORKAROUND_ENABLED`: Enable/disable the workaround (default: 1)

## Limitations and Considerations

- **Movistar+ Specific**: This fork is optimized specifically for Movistar+ IPTV service
- **Not for General Use**: Should not be used with other IPTV providers or general multicast routing
- **Performance Impact**: Minimal overhead due to efficient state management
- **Network Traffic**: Adds periodic IGMP reports (negligible bandwidth impact)

## Troubleshooting

### Common Issues

1. **Service Not Starting**: Check configuration file syntax and interface names
2. **No Multicast Traffic**: Verify upstream interface configuration and network connectivity
3. **Stream Interruptions**: Ensure the workaround is enabled and timers are functioning

### Debug Information

Enable verbose logging to monitor the refresh mechanism:

```bash
# Run with debug output
sudo /usr/local/sbin/igmpproxy -vv /etc/igmpproxy.conf
```

## Contributing

This fork is maintained specifically for Movistar+ IPTV compatibility and is not intended for upstream contribution to the original igmpproxy project. Issues and improvements related to Movistar+ compatibility are welcome.

## Original Project

This fork is based on the original igmpproxy project by Johnny Egeland. For general multicast routing needs, please refer to the original project.

## Disclaimer

This software is provided as-is for Movistar+ IPTV compatibility. Use at your own risk. The maintainer is not responsible for any service interruptions or issues arising from the use of this modified software.