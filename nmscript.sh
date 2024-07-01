#!/usr/bin/bash
nmcli -o -f IN-USE,BARS device wifi | awk '/^\*/{if (NR!=1) {print $2}}' > /tmp/rwm/nm2
cp /tmp/rwm/nm2 /tmp/rwm/nm
