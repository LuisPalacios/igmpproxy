# IGMPproxy 

A simple mulitcast router that only uses the IGMP protocol.

Supported operating systems:
 - Linux
 - FreeBSD
 - NetBSD
 - OpenBSD
 - DragonFly BSD

This software is released under the GNU GPL license v2 or later. See details in COPYING.

## Patch for M+

M+ upstream router doesn't send Membership Query's. The side effect on igmpproxy is that 
without query's it will not report (join) again upstream. What will happen is that the
group membership upstream will timeout after 260secs, so every 4'30" it will stop sending
traffic and your TV stream will freeze. 

One option is to change channel and it will start again, for another 4'30", which makes it
unusable. 

A long term solution is to replicate every Join from our local LAN (from the Decos) from 
time to time, so it avoids upstream to stop sending traffice, however I've make a quick 
and dirty patch to simple copy/paste all Join's from downstream, making this igmpproxy
unefficient in general, but working for this particular case without any problem. 

So, don't use this version on any other use case than Movistar+. If you are using 
just for the IPTV service at home, feel free to implement this dirty hack that breaks
efficiency but works like a champ.

Compilation
===========

Clone

```zsh
git clone https://github.com/LuisPalacios/igmpproxy.git
```

Build

```zsh
./autogen.sh
./configure
make
```

Installation
============

Notice it will be installed under `/usr/local`, so if you have a previous installation
it will not be modified, anyway I recommend to remove the OS official version
to avoid conflicts. 

Remove OS official version

```zsh
% sudo apt purge igmpproxy
```

Install

```zsh
% make install
:
/usr/bin/mkdir -p '/usr/local/sbin'
/usr/bin/install -c igmpproxy '/usr/local/sbin'
:
/usr/bin/mkdir -p '/usr/local/etc'
/usr/bin/install -c -m 644 igmpproxy.conf '/usr/local/etc'
```

Configuration
==============

Create `/etc/systemd/system/igmpproxy.service`

```conf
[Unit]
Description=Custom igmpproxy service
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

Create `/etc/default/igmpproxy`

```conf
# Options for my custom igmpproxy
# source https://github.com/LuisPalacios/igmpproxy
IGMPPROXY_OPTS="-n /etc/igmpproxy.conf"
```

Create `/etc/igmpproxy.conf`

```conf
# Sample configuration file for Movistar+
# Upstream: vlan2 (M+ iptv)
# Downstream: vlan10 (local LAN)
quickleave
phyint vlan2 upstream  ratelimit 0  threshold 3
    altnet 172.0.0.0/8
phyint vlan10 downstream ratelimit 0  threshold 3
phyint lo disabled
phyint ppp0 disabled
phyint vlan3 disabled
phyint vlan6 disabled
```

Run
==============

```zsh
systemctl daemon-reload
systemctl enable igmpproxy
systemctl start igmpproxy
```

### Rationale for the patch

In IGMPv2 there is subscription management, to deliver IPTV efficiently, ensuring that
streams are transmitted only when and where they are needed. This optimizes bandwidth 
and network resources.

Decos frequently send joins to various groups: it starts with 239.0.2.30:22222 OPCH 
and then targets others such as 239.0.2.2, 239.0.2.129-134, 239.0.2.154-155. At a 
given moment it asks for the channel you want to watch, which is in 239.0.[0,3,4,5,6,7,8,9].*

Imagine this setup, with 3 decos. If the `igmpproxy` would make copy/paste of the
joins/leaves it would not be enough, you would multiply by three the control traffic 
in the upstream. Here Router Movistar is the router in the backend cloud of Movistar.
The Router igmpproxy is your router at home (not the original from M+)


```text
Router
Movistar
  |
  |
Router
igmpproxy
 |  |  | 
 |  |  |
D1  D2  D3
````

**Join**: When a deco wants to start receiving a specific stream, it sends a Join message
to the corresponding multicast group. The igmpproxy receives this Join and, if it was not
previously subscribed to that group in the operator's network, sends a Join message to 
the main multicast router (upstream). The proxy keeps a control table of which sites he is subscribed to.

**Leave**: If **all** decos in your network are no longer interested in a multicast stream
(i.e. there are no more active subscribers for that group in your network), igmpproxy detects
this (due to deco leaves or inactivity) and sends a **Leave** message to the upstream
router to stop sending that specific content.

**Query**: The upstream router occasionally sends **Queries** to check what subscriptions are 
active on the networks below it (like your home network). When igmpproxy receives a Query:
it should respond with a **Report** indicating all multicast groups to which its set-top 
boxes are still actively subscribed. This informs the upstream router that those specific
streams are still needed and should continue to be sent.

What I've seen is that the "Movistar Router" does not send queries, so after 260" timeout
kick's in and stream stops.
