# Copyright (C) 2012  Internet Systems Consortium, Inc. ("ISC")
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SYSTEMS CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SYSTEMS CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

'''This module returns system information.'''

import os
import sys
import re
import subprocess
import os.path
import platform

class SysInfo:
    def __init__(self):
        self._num_processors = -1
        self._endianness = 'Unknown'
        self._hostname = ''
        self._platform_name = 'Unknown'
        self._platform_version = 'Unknown'
        self._platform_machine = 'Unknown'
        self._platform_is_smp = False
        self._uptime = -1
        self._loadavg = [-1.0, -1.0, -1.0]
        self._mem_total = -1
        self._mem_free = -1
        self._mem_cached = -1
        self._mem_buffers = -1
        self._mem_swap_total = -1
        self._mem_swap_free = -1
        self._platform_distro = 'Unknown'
        self._net_interfaces = 'Unknown'
        self._net_routing_table = 'Unknown'
        self._net_stats = 'Unknown'
        self._net_connections = 'Unknown'

    def get_num_processors(self):
        """Returns the number of processors. This is the number of
        hyperthreads when hyper-threading is enabled.
        """
        return self._num_processors

    def get_endianness(self):
        """Returns 'big' or 'little'."""
        return self._endianness

    def get_platform_hostname(self):
        """Returns the hostname of the system."""
        return self._hostname

    def get_platform_name(self):
        """Returns the platform name (uname -s)."""
        return self._platform_name

    def get_platform_version(self):
        """Returns the platform version (uname -v)."""
        return self._platform_version

    def get_platform_machine(self):
        """Returns the platform machine architecture."""
        return self._platform_machine

    def get_platform_is_smp(self):
        """Returns True if an SMP kernel is being used, False otherwise."""
        return self._platform_is_smp

    def get_platform_distro(self):
        """Returns the name of the OS distribution in use."""
        return self._platform_distro

    def get_uptime(self):
        """Returns the uptime in seconds."""
        return self._uptime

    def get_loadavg(self):
        """Returns the load average as 3 floating point values in an array."""
        return self._loadavg

    def get_mem_total(self):
        """Returns the total amount of memory in bytes."""
        return self._mem_total

    def get_mem_free(self):
        """Returns the amount of free memory in bytes."""
        return self._mem_free

    def get_mem_cached(self):
        """Returns the amount of cached memory in bytes."""
        return self._mem_cached

    def get_mem_buffers(self):
        """Returns the amount of buffer in bytes."""
        return self._mem_buffers

    def get_mem_swap_total(self):
        """Returns the total amount of swap in bytes."""
        return self._mem_swap_total

    def get_mem_swap_free(self):
        """Returns the amount of free swap in bytes."""
        return self._mem_swap_free

    def get_net_interfaces(self):
        """Returns information about network interfaces (as a multi-line string)."""
        return self._net_interfaces

    def get_net_routing_table(self):
        """Returns information about network routing table (as a multi-line string)."""
        return self._net_routing_table

    def get_net_stats(self):
        """Returns network statistics (as a multi-line string)."""
        return self._net_stats

    def get_net_connections(self):
        """Returns network connection information (as a multi-line string)."""
        return self._net_connections

class SysInfoLinux(SysInfo):
    """Linux implementation of the SysInfo class.
    See the base class documentation for more information.
    """
    def __init__(self):
        super().__init__()

        self._num_processors = os.sysconf('SC_NPROCESSORS_CONF')
        self._endianness = sys.byteorder

        with open('/proc/sys/kernel/hostname') as f:
            self._hostname = f.read().strip()

        u = os.uname()
        self._platform_name = u[0]
        self._platform_version = u[2]
        self._platform_machine = u[4]

        with open('/proc/version') as f:
            self._platform_is_smp = ' SMP ' in f.read().strip()

        with open('/proc/uptime') as f:
            u = f.read().strip().split(' ')
            self._uptime = int(round(float(u[0])))

        with open('/proc/loadavg') as f:
            l = f.read().strip().split(' ')
            self._loadavg = [float(l[0]), float(l[1]), float(l[2])]

        with open('/proc/meminfo') as f:
            m = f.readlines()
            for line in m:
                r = re.match('^MemTotal:\s+(.*)\s*kB', line)
                if r:
                    self._mem_total = int(r.group(1).strip()) * 1024
                    continue
                r = re.match('^MemFree:\s+(.*)\s*kB', line)
                if r:
                    self._mem_free = int(r.group(1).strip()) * 1024
                    continue
                r = re.match('^Cached:\s+(.*)\s*kB', line)
                if r:
                    self._mem_cached = int(r.group(1).strip()) * 1024
                    continue
                r = re.match('^Buffers:\s+(.*)\s*kB', line)
                if r:
                    self._mem_buffers = int(r.group(1).strip()) * 1024
                    continue
                r = re.match('^SwapTotal:\s+(.*)\s*kB', line)
                if r:
                    self._mem_swap_total = int(r.group(1).strip()) * 1024
                    continue
                r = re.match('^SwapFree:\s+(.*)\s*kB', line)
                if r:
                    self._mem_swap_free = int(r.group(1).strip()) * 1024
                    continue

        self._platform_distro = None

        try:
            s = subprocess.check_output(['lsb_release', '-a'])
            for line in s.decode('utf-8').split('\n'):
                r = re.match('^Description:(.*)', line)
                if r:
                    self._platform_distro = r.group(1).strip()
                    break
        except (subprocess.CalledProcessError, OSError):
            pass

        if self._platform_distro is None:
            files = ['/etc/debian_release',
                     '/etc/debian_version',
                     '/etc/SuSE-release',
                     '/etc/UnitedLinux-release',
                     '/etc/mandrake-release',
                     '/etc/gentoo-release',
                     '/etc/fedora-release',
                     '/etc/redhat-release',
                     '/etc/redhat_version',
                     '/etc/slackware-release',
                     '/etc/slackware-version',
                     '/etc/arch-release',
                     '/etc/lsb-release',
                     '/etc/mageia-release']
            for fn in files:
                if os.path.exists(fn):
                    with open(fn) as f:
                        self._platform_distro = f.read().strip()
                    break

        if self._platform_distro is None:
            self._platform_distro = 'Unknown'

        self._net_interfaces = None

        try:
            s = subprocess.check_output(['ip', 'addr'])
            self._net_interfaces = s.decode('utf-8')
        except (subprocess.CalledProcessError, OSError):
            pass

        if self._net_interfaces is None:
            self._net_interfaces = 'Unknown'

        self._net_routing_table = None

        try:
            s = subprocess.check_output(['ip', 'route'])
            self._net_routing_table = s.decode('utf-8')
            self._net_routing_table += '\n'
            s = subprocess.check_output(['ip', '-f', 'inet6', 'route'])
            self._net_routing_table += s.decode('utf-8')
        except (subprocess.CalledProcessError, OSError):
            pass

        if self._net_routing_table is None:
            self._net_routing_table = 'Unknown'

        self._net_stats = None

        try:
            s = subprocess.check_output(['netstat', '-s'])
            self._net_stats = s.decode('utf-8')
        except (subprocess.CalledProcessError, OSError):
            pass

        if self._net_stats is None:
            self._net_stats = 'Unknown'

        self._net_connections = None

        try:
            s = subprocess.check_output(['netstat', '-apn'])
            self._net_connections = s.decode('utf-8')
        except (subprocess.CalledProcessError, OSError):
            pass

        if self._net_connections is None:
            self._net_connections = 'Unknown'

class SysInfoTestcase(SysInfo):
    def __init__(self):
        super().__init__()
        self._endianness = 'bigrastafarian'
        self._platform_name = 'b10test'
        self._uptime = 131072

def SysInfoFromFactory():
    osname = platform.system()
    if osname == 'Linux':
        return SysInfoLinux()
    elif osname == 'BIND10Testcase':
        return SysInfoTestcase()
    else:
        return SysInfo()
