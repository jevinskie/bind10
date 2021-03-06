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

#include <config.h>
#include <iostream>
#include <sstream>
#include <arpa/inet.h>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <gtest/gtest.h>

#include <asiolink/io_address.h>
#include <dhcp/option.h>
#include <dhcp/pkt6.h>
#include <dhcp/dhcp6.h>

using namespace std;
using namespace isc;
using namespace isc::asiolink;
using namespace isc::dhcp;

namespace {
// empty class for now, but may be extended once Addr6 becomes bigger
class Pkt6Test : public ::testing::Test {
public:
    Pkt6Test() {
    }
};

TEST_F(Pkt6Test, constructor) {
    uint8_t data[] = { 0, 1, 2, 3, 4, 5 };
    Pkt6 * pkt1 = new Pkt6(data, sizeof(data) );

    EXPECT_EQ(6, pkt1->getData().size());
    EXPECT_EQ(0, memcmp( &pkt1->getData()[0], data, sizeof(data)));

    delete pkt1;
}

/// @brief returns captured actual SOLICIT packet
///
/// Captured SOLICIT packet with transid=0x3d79fb and options: client-id,
/// in_na, dns-server, elapsed-time, option-request
/// This code was autogenerated (see src/bin/dhcp6/tests/iface_mgr_unittest.c),
/// but we spent some time to make is less ugly than it used to be.
///
/// @return pointer to Pkt6 that represents received SOLICIT
Pkt6* capture1() {
    Pkt6* pkt;
    uint8_t data[98];
    data[0]  = 1;
    data[1]  = 1;       data[2]  = 2;     data[3] = 3;      data[4]  = 0;
    data[5]  = 1;       data[6]  = 0;     data[7] = 14;     data[8]  = 0;
    data[9]  = 1;       data[10] = 0;     data[11] = 1;     data[12] = 21;
    data[13] = 158;     data[14] = 60;    data[15] = 22;    data[16] = 0;
    data[17] = 30;      data[18] = 140;   data[19] = 155;   data[20] = 115;
    data[21] = 73;      data[22] = 0;     data[23] = 3;     data[24] = 0;
    data[25] = 40;      data[26] = 0;     data[27] = 0;     data[28] = 0;
    data[29] = 1;       data[30] = 255;   data[31] = 255;   data[32] = 255;
    data[33] = 255;     data[34] = 255;   data[35] = 255;   data[36] = 255;
    data[37] = 255;     data[38] = 0;     data[39] = 5;     data[40] = 0;
    data[41] = 24;      data[42] = 32;    data[43] = 1;     data[44] = 13;
    data[45] = 184;     data[46] = 0;     data[47] = 1;     data[48] = 0;
    data[49] = 0;       data[50] = 0;     data[51] = 0;     data[52] = 0;
    data[53] = 0;       data[54] = 0;     data[55] = 0;     data[56] = 18;
    data[57] = 52;      data[58] = 255;   data[59] = 255;   data[60] = 255;
    data[61] = 255;     data[62] = 255;   data[63] = 255;   data[64] = 255;
    data[65] = 255;     data[66] = 0;     data[67] = 23;    data[68] = 0;
    data[69] = 16;      data[70] = 32;    data[71] = 1;     data[72] = 13;
    data[73] = 184;     data[74] = 0;     data[75] = 1;     data[76] = 0;
    data[77] = 0;       data[78] = 0;     data[79] = 0;     data[80] = 0;
    data[81] = 0;       data[82] = 0;     data[83] = 0;     data[84] = 221;
    data[85] = 221;     data[86] = 0;     data[87] = 8;     data[88] = 0;
    data[89] = 2;       data[90] = 0;     data[91] = 100;   data[92] = 0;
    data[93] = 6;       data[94] = 0;     data[95] = 2;     data[96] = 0;
    data[97] = 23;

    pkt = new Pkt6(data, sizeof(data));
    pkt->setRemotePort(546);
    pkt->setRemoteAddr(IOAddress("fe80::21e:8cff:fe9b:7349"));
    pkt->setLocalPort(0);
    pkt->setLocalAddr(IOAddress("ff02::1:2"));
    pkt->setIndex(2);
    pkt->setIface("eth0");

    return (pkt);
}


TEST_F(Pkt6Test, unpack_solicit1) {
    Pkt6* sol = capture1();

    ASSERT_EQ(true, sol->unpack());

    // check for length
    EXPECT_EQ(98, sol->len() );

    // check for type
    EXPECT_EQ(DHCPV6_SOLICIT, sol->getType() );

    // check that all present options are returned
    EXPECT_TRUE(sol->getOption(D6O_CLIENTID)); // client-id is present
    EXPECT_TRUE(sol->getOption(D6O_IA_NA));    // IA_NA is present
    EXPECT_TRUE(sol->getOption(D6O_ELAPSED_TIME));  // elapsed is present
    EXPECT_TRUE(sol->getOption(D6O_NAME_SERVERS));
    EXPECT_TRUE(sol->getOption(D6O_ORO));

    // let's check that non-present options are not returned
    EXPECT_FALSE(sol->getOption(D6O_SERVERID)); // server-id is missing
    EXPECT_FALSE(sol->getOption(D6O_IA_TA));
    EXPECT_FALSE(sol->getOption(D6O_IAADDR));

    delete sol;
}

TEST_F(Pkt6Test, packUnpack) {

    Pkt6* parent = new Pkt6(DHCPV6_SOLICIT, 0x020304);

    OptionPtr opt1(new Option(Option::V6, 1));
    OptionPtr opt2(new Option(Option::V6, 2));
    OptionPtr opt3(new Option(Option::V6, 100));
    // let's not use zero-length option type 3 as it is IA_NA

    parent->addOption(opt1);
    parent->addOption(opt2);
    parent->addOption(opt3);

    EXPECT_EQ(DHCPV6_SOLICIT, parent->getType());

    // calculated length should be 16
    EXPECT_EQ(Pkt6::DHCPV6_PKT_HDR_LEN + 3 * Option::OPTION6_HDR_LEN,
              parent->len());

    EXPECT_TRUE(parent->pack());

    EXPECT_EQ(Pkt6::DHCPV6_PKT_HDR_LEN + 3 * Option::OPTION6_HDR_LEN,
              parent->len());

    // create second packet,based on assembled data from the first one
    Pkt6* clone = new Pkt6(static_cast<const uint8_t*>(parent->getBuffer().getData()),
                           parent->getBuffer().getLength());

    // now recreate options list
    EXPECT_TRUE( clone->unpack() );

    // transid, message-type should be the same as before
    EXPECT_EQ(parent->getTransid(), parent->getTransid());
    EXPECT_EQ(DHCPV6_SOLICIT, clone->getType());

    EXPECT_TRUE( clone->getOption(1));
    EXPECT_TRUE( clone->getOption(2));
    EXPECT_TRUE( clone->getOption(100));
    EXPECT_FALSE( clone->getOption(4));

    delete parent;
    delete clone;
}

TEST_F(Pkt6Test, addGetDelOptions) {
    Pkt6* parent = new Pkt6(DHCPV6_SOLICIT, random() );

    OptionPtr opt1(new Option(Option::V6, 1));
    OptionPtr opt2(new Option(Option::V6, 2));
    OptionPtr opt3(new Option(Option::V6, 2));

    parent->addOption(opt1);
    parent->addOption(opt2);

    // getOption() test
    EXPECT_EQ(opt1, parent->getOption(1));
    EXPECT_EQ(opt2, parent->getOption(2));

    // expect NULL
    EXPECT_EQ(OptionPtr(), parent->getOption(4));

    // now there are 2 options of type 2
    parent->addOption(opt3);

    // let's delete one of them
    EXPECT_EQ(true, parent->delOption(2));

    // there still should be the other option 2
    EXPECT_NE(OptionPtr(), parent->getOption(2));

    // let's delete the other option 2
    EXPECT_EQ(true, parent->delOption(2));

    // no more options with type=2
    EXPECT_EQ(OptionPtr(), parent->getOption(2));

    // let's try to delete - should fail
    EXPECT_TRUE(false ==  parent->delOption(2));

    delete parent;
}

TEST_F(Pkt6Test, Timestamp) {
    boost::scoped_ptr<Pkt6> pkt(new Pkt6(DHCPV6_SOLICIT, 0x020304));

    // Just after construction timestamp is invalid
    ASSERT_TRUE(pkt->getTimestamp().is_not_a_date_time());

    // Update packet time.
    pkt->updateTimestamp();

    // Get updated packet time.
    boost::posix_time::ptime ts_packet = pkt->getTimestamp();

    // After timestamp is updated it should be date-time.
    ASSERT_FALSE(ts_packet.is_not_a_date_time());

    // Check current time.
    boost::posix_time::ptime ts_now =
        boost::posix_time::microsec_clock::universal_time();

    // Calculate period between packet time and now.
    boost::posix_time::time_period ts_period(ts_packet, ts_now);

    // Duration should be positive or zero.
    EXPECT_TRUE(ts_period.length().total_microseconds() >= 0);
}

}
