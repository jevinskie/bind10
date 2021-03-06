// Copyright (C) 2011-2012 Internet Systems Consortium, Inc. ("ISC")
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
// REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
// INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
// LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
// OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
// PERFORMANCE OF THIS SOFTWARE.

#include <stdlib.h>
#include <time.h>

#include <asiolink/io_address.h>
#include <dhcp6/dhcp6_log.h>
#include <dhcp6/dhcp6_srv.h>
#include <dhcp/dhcp6.h>
#include <dhcp/iface_mgr.h>
#include <dhcp/option6_addrlst.h>
#include <dhcp/option6_iaaddr.h>
#include <dhcp/option6_ia.h>
#include <dhcp/pkt6.h>
#include <exceptions/exceptions.h>
#include <util/io_utilities.h>
#include <util/range_utilities.h>

using namespace isc;
using namespace isc::asiolink;
using namespace isc::dhcp;
using namespace isc::util;
using namespace std;

const std::string HARDCODED_LEASE = "2001:db8:1::1234:abcd";
const uint32_t HARDCODED_T1 = 1500; // in seconds
const uint32_t HARDCODED_T2 = 2600; // in seconds
const uint32_t HARDCODED_PREFERRED_LIFETIME = 3600; // in seconds
const uint32_t HARDCODED_VALID_LIFETIME = 7200; // in seconds
const std::string HARDCODED_DNS_SERVER = "2001:db8:1::1";

Dhcpv6Srv::Dhcpv6Srv(uint16_t port) {
    LOG_DEBUG(dhcp6_logger, DBG_DHCP6_START, DHCP6_OPEN_SOCKET).arg(port);

    // First call to instance() will create IfaceMgr (it's a singleton)
    // it may throw something if things go wrong
    try {

        if (IfaceMgr::instance().countIfaces() == 0) {
            LOG_ERROR(dhcp6_logger, DHCP6_NO_INTERFACES);
            shutdown_ = true;
            return;
        }

        IfaceMgr::instance().openSockets6(port);

        setServerID();

        /// @todo: instantiate LeaseMgr here once it is imlpemented.

    } catch (const std::exception &e) {
        LOG_ERROR(dhcp6_logger, DHCP6_SRV_CONSTRUCT_ERROR).arg(e.what());
        shutdown_ = true;
        return;
    }

    shutdown_ = false;
}

Dhcpv6Srv::~Dhcpv6Srv() {
    IfaceMgr::instance().closeSockets();
}

void Dhcpv6Srv::shutdown() {
    LOG_DEBUG(dhcp6_logger, DBG_DHCP6_BASIC, DHCP6_SHUTDOWN_REQUEST);
    shutdown_ = true;
}

bool Dhcpv6Srv::run() {
    while (!shutdown_) {
        /// @todo: calculate actual timeout once we have lease database
        int timeout = 1000;

        // client's message and server's response
        Pkt6Ptr query;
        Pkt6Ptr rsp;

        try {
            query = IfaceMgr::instance().receive6(timeout);
        } catch (const std::exception& e) {
            LOG_ERROR(dhcp6_logger, DHCP6_PACKET_RECEIVE_FAIL).arg(e.what());
        }

        if (query) {
            if (!query->unpack()) {
                LOG_DEBUG(dhcp6_logger, DBG_DHCP6_DETAIL,
                          DHCP6_PACKET_PARSE_FAIL);
                continue;
            }
            LOG_DEBUG(dhcp6_logger, DBG_DHCP6_DETAIL, DHCP6_PACKET_RECEIVED)
                      .arg(serverReceivedPacketName(query->getType()))
                      .arg(query->getType());
            LOG_DEBUG(dhcp6_logger, DBG_DHCP6_DETAIL_DATA, DHCP6_QUERY_DATA)
                      .arg(query->getType())
                      .arg(query->getBuffer().getLength())
                      .arg(query->toText());

            switch (query->getType()) {
            case DHCPV6_SOLICIT:
                rsp = processSolicit(query);
                break;

            case DHCPV6_REQUEST:
                rsp = processRequest(query);
                break;

            case DHCPV6_RENEW:
                rsp = processRenew(query);
                break;

            case DHCPV6_REBIND:
                rsp = processRebind(query);
                break;

            case DHCPV6_CONFIRM:
                rsp = processConfirm(query);
                break;

            case DHCPV6_RELEASE:
                rsp = processRelease(query);
                break;

            case DHCPV6_DECLINE:
                rsp = processDecline(query);
                break;

            case DHCPV6_INFORMATION_REQUEST:
                rsp = processInfRequest(query);
                break;

            default:
                // Only action is to output a message if debug is enabled,
                // and that will be covered by the debug statement before
                // the "switch" statement.
                ;
            }

            if (rsp) {
                rsp->setRemoteAddr(query->getRemoteAddr());
                rsp->setLocalAddr(query->getLocalAddr());
                rsp->setRemotePort(DHCP6_CLIENT_PORT);
                rsp->setLocalPort(DHCP6_SERVER_PORT);
                rsp->setIndex(query->getIndex());
                rsp->setIface(query->getIface());

                LOG_DEBUG(dhcp6_logger, DBG_DHCP6_DETAIL_DATA,
                          DHCP6_RESPONSE_DATA)
                          .arg(rsp->getType()).arg(rsp->toText());

                if (rsp->pack()) {
                    try {
                        IfaceMgr::instance().send(rsp);
                    } catch (const std::exception& e) {
                        LOG_ERROR(dhcp6_logger, DHCP6_PACKET_SEND_FAIL).arg(e.what());
                    }
                } else {
                    LOG_ERROR(dhcp6_logger, DHCP6_PACK_FAIL);
                }
            }
        }

        // TODO add support for config session (see src/bin/auth/main.cc)
        //      so this daemon can be controlled from bob
    }

    return (true);
}

void Dhcpv6Srv::setServerID() {

    /// @todo: DUID should be generated once and then stored, rather
    /// than generated each time

    /// @todo: This code implements support for DUID-LLT (the recommended one).
    /// We should eventually add support for other DUID types: DUID-LL, DUID-EN
    /// and DUID-UUID

    const IfaceMgr::IfaceCollection& ifaces = IfaceMgr::instance().getIfaces();

    // let's find suitable interface
    for (IfaceMgr::IfaceCollection::const_iterator iface = ifaces.begin();
         iface != ifaces.end(); ++iface) {
        // All the following checks could be merged into one multi-condition
        // statement, but let's keep them separated as perhaps one day
        // we will grow knobs to selectively turn them on or off. Also,
        // this code is used only *once* during first start on a new machine
        // and then server-id is stored. (or at least it will be once
        // DUID storage is implemente

        // I wish there was a this_is_a_real_physical_interface flag...

        // MAC address should be at least 6 bytes. Although there is no such
        // requirement in any RFC, all decent physical interfaces (Ethernet,
        // WiFi, Infiniband, etc.) have 6 bytes long MAC address. We want to
        // base our DUID on real hardware address, rather than virtual
        // interface that pretends that underlying IP address is its MAC.
        if (iface->getMacLen() < MIN_MAC_LEN) {
            continue;
        }

        // let's don't use loopback
        if (iface->flag_loopback_) {
            continue;
        }

        // let's skip downed interfaces. It is better to use working ones.
        if (!iface->flag_up_) {
            continue;
        }

        // some interfaces (like lo on Linux) report 6-bytes long
        // MAC adress 00:00:00:00:00:00. Let's not use such weird interfaces
        // to generate DUID.
        if (isRangeZero(iface->getMac(), iface->getMac() + iface->getMacLen())) {
            continue;
        }

        // Ok, we have useful MAC. Let's generate DUID-LLT based on
        // it. See RFC3315, Section 9.2 for details.

        // DUID uses seconds since midnight of 01-01-2000, time() returns
        // seconds since 01-01-1970. DUID_TIME_EPOCH substution corrects that.
        time_t seconds = time(NULL);
        seconds -= DUID_TIME_EPOCH;

        OptionBuffer srvid(8 + iface->getMacLen());
        writeUint16(DUID_LLT, &srvid[0]);
        writeUint16(HWTYPE_ETHERNET, &srvid[2]);
        writeUint32(static_cast<uint32_t>(seconds), &srvid[4]);
        memcpy(&srvid[0]+8, iface->getMac(), iface->getMacLen());

        serverid_ = OptionPtr(new Option(Option::V6, D6O_SERVERID,
                                         srvid.begin(), srvid.end()));
        return;
    }

    // if we reached here, there are no suitable interfaces found.
    // Either interface detection is not supported on this platform or
    // this is really weird box. Let's use DUID-EN instead.
    // See Section 9.3 of RFC3315 for details.

    OptionBuffer srvid(12);
    writeUint16(DUID_EN, &srvid[0]);
    writeUint32(ENTERPRISE_ID_ISC, &srvid[2]);

    // Length of the identifier is company specific. I hereby declare
    // ISC "standard" of 6 bytes long pseudo-random numbers.
    srandom(time(NULL));
    fillRandom(&srvid[6],&srvid[12]);

    serverid_ = OptionPtr(new Option(Option::V6, D6O_SERVERID,
                                     srvid.begin(), srvid.end()));
}

void Dhcpv6Srv::copyDefaultOptions(const Pkt6Ptr& question, Pkt6Ptr& answer) {
    // add client-id
    boost::shared_ptr<Option> clientid = question->getOption(D6O_CLIENTID);
    if (clientid) {
        answer->addOption(clientid);
    }

    // TODO: Should throw if there is no client-id (except anonymous INF-REQUEST)
}

void Dhcpv6Srv::appendDefaultOptions(const Pkt6Ptr& /*question*/, Pkt6Ptr& answer) {
    // TODO: question is currently unused, but we need it at least to know
    // message type we are answering

    // add server-id
    answer->addOption(getServerID());
}


void Dhcpv6Srv::appendRequestedOptions(const Pkt6Ptr& /*question*/, Pkt6Ptr& answer) {
    // TODO: question is currently unused, but we need to extract ORO from it
    // and act on its content. Now we just send DNS-SERVERS option.

    // add dns-servers option
    boost::shared_ptr<Option> dnsservers(new Option6AddrLst(D6O_NAME_SERVERS,
                                         IOAddress(HARDCODED_DNS_SERVER)));
    answer->addOption(dnsservers);
}

void Dhcpv6Srv::assignLeases(const Pkt6Ptr& question, Pkt6Ptr& answer) {
    /// TODO Rewrite this once LeaseManager is implemented.

    // answer client's IA (this is mostly a dummy,
    // so let's answer only first IA and hope there is only one)
    boost::shared_ptr<Option> ia_opt = question->getOption(D6O_IA_NA);
    if (ia_opt) {
        // found IA
        Option* tmp = ia_opt.get();
        Option6IA* ia_req = dynamic_cast<Option6IA*>(tmp);
        if (ia_req) {
            boost::shared_ptr<Option6IA>
                ia_rsp(new Option6IA(D6O_IA_NA, ia_req->getIAID()));
            ia_rsp->setT1(HARDCODED_T1);
            ia_rsp->setT2(HARDCODED_T2);
            boost::shared_ptr<Option6IAAddr>
                addr(new Option6IAAddr(D6O_IAADDR,
                                       IOAddress(HARDCODED_LEASE),
                                       HARDCODED_PREFERRED_LIFETIME,
                                       HARDCODED_VALID_LIFETIME));
            ia_rsp->addOption(addr);
            answer->addOption(ia_rsp);
        }
    }
}

Pkt6Ptr Dhcpv6Srv::processSolicit(const Pkt6Ptr& solicit) {
    Pkt6Ptr advertise(new Pkt6(DHCPV6_ADVERTISE, solicit->getTransid()));

    copyDefaultOptions(solicit, advertise);
    appendDefaultOptions(solicit, advertise);
    appendRequestedOptions(solicit, advertise);

    assignLeases(solicit, advertise);

    return (advertise);
}

Pkt6Ptr Dhcpv6Srv::processRequest(const Pkt6Ptr& request) {
    Pkt6Ptr reply(new Pkt6(DHCPV6_REPLY, request->getTransid()));

    copyDefaultOptions(request, reply);
    appendDefaultOptions(request, reply);
    appendRequestedOptions(request, reply);

    assignLeases(request, reply);

    return (reply);
}

Pkt6Ptr Dhcpv6Srv::processRenew(const Pkt6Ptr& renew) {
    /// @todo: Implement this
    Pkt6Ptr reply(new Pkt6(DHCPV6_REPLY, renew->getTransid()));
    return reply;
}

Pkt6Ptr Dhcpv6Srv::processRebind(const Pkt6Ptr& rebind) {
    /// @todo: Implement this
    Pkt6Ptr reply(new Pkt6(DHCPV6_REPLY, rebind->getTransid()));
    return reply;
}

Pkt6Ptr Dhcpv6Srv::processConfirm(const Pkt6Ptr& confirm) {
    /// @todo: Implement this
    Pkt6Ptr reply(new Pkt6(DHCPV6_REPLY, confirm->getTransid()));
    return reply;
}

Pkt6Ptr Dhcpv6Srv::processRelease(const Pkt6Ptr& release) {
    /// @todo: Implement this
    Pkt6Ptr reply(new Pkt6(DHCPV6_REPLY, release->getTransid()));
    return reply;
}

Pkt6Ptr Dhcpv6Srv::processDecline(const Pkt6Ptr& decline) {
    /// @todo: Implement this
    Pkt6Ptr reply(new Pkt6(DHCPV6_REPLY, decline->getTransid()));
    return reply;
}

Pkt6Ptr Dhcpv6Srv::processInfRequest(const Pkt6Ptr& infRequest) {
    /// @todo: Implement this
    Pkt6Ptr reply(new Pkt6(DHCPV6_REPLY, infRequest->getTransid()));
    return reply;
}

const char*
Dhcpv6Srv::serverReceivedPacketName(uint8_t type) {
    static const char* CONFIRM = "CONFIRM";
    static const char* DECLINE = "DECLINE";
    static const char* INFORMATION_REQUEST = "INFORMATION_REQUEST";
    static const char* REBIND = "REBIND";
    static const char* RELEASE = "RELEASE";
    static const char* RENEW = "RENEW";
    static const char* REQUEST = "REQUEST";
    static const char* SOLICIT = "SOLICIT";
    static const char* UNKNOWN = "UNKNOWN";

    switch (type) {
    case DHCPV6_CONFIRM:
        return (CONFIRM);

    case DHCPV6_DECLINE:
        return (DECLINE);

    case DHCPV6_INFORMATION_REQUEST:
        return (INFORMATION_REQUEST);

    case DHCPV6_REBIND:
        return (REBIND);

    case DHCPV6_RELEASE:
        return (RELEASE);

    case DHCPV6_RENEW:
        return (RENEW);

    case DHCPV6_REQUEST:
        return (REQUEST);

    case DHCPV6_SOLICIT:
        return (SOLICIT);

    default:
        ;
    }
    return (UNKNOWN);
}
