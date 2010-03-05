// Copyright (C) 2010  Internet Systems Consortium, Inc. ("ISC")
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

// $Id$

#include <dns/buffer.h>
#include <dns/messagerenderer.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rrclass.h>
#include <dns/rrtype.h>

#include <gtest/gtest.h>

#include "unittest_util.h"
#include "rdata_unittest.h"

using isc::UnitTestUtil;
using namespace std;
using namespace isc::dns;
using namespace isc::dns::rdata;

namespace {
class Rdata_DNAME_Test : public RdataTest {
    // there's nothing to specialize
};

const generic::DNAME rdata_dname("dn.example.com");
const generic::DNAME rdata_dname2("dn2.example.com");
const uint8_t wiredata_dname[] = {
    0x02, 0x64, 0x6e, 0x07, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x03,
    0x63, 0x6f, 0x6d, 0x00 };
const uint8_t wiredata_dname2[] = {
    // first name: dn.example.com.
    0x02, 0x64, 0x6e, 0x07, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x03,
    0x63, 0x6f, 0x6d, 0x00,
    // second name: dn2.example.com.  all labels except the first should be
    // compressed.
    0x03, 0x64, 0x6e, 0x32, 0xc0, 0x03 };

TEST_F(Rdata_DNAME_Test, createFromText)
{
    EXPECT_EQ(0, rdata_dname.compare(generic::DNAME("dn.example.com")));
    // explicitly add a trailing dot.  should be the same RDATA.
    EXPECT_EQ(0, rdata_dname.compare(generic::DNAME("dn.example.com.")));
    // should be case sensitive.
    EXPECT_EQ(0, rdata_dname.compare(generic::DNAME("DN.EXAMPLE.COM")));
    // RDATA of a class-independent type should be recognized for any
    // "unknown" class.
    EXPECT_EQ(0, rdata_dname.compare(*createRdata(RRType("DNAME"),
                                                  RRClass(65000),
                                                  "dn.example.com")));
}

TEST_F(Rdata_DNAME_Test, createFromWire)
{
    EXPECT_EQ(0, rdata_dname.compare(
                  *rdataFactoryFromFile(RRType("DNAME"), RRClass("IN"),
                                        "testdata/rdata_dname_fromWire")));
    // RDLENGTH is too short
    EXPECT_THROW(rdataFactoryFromFile(RRType("DNAME"), RRClass("IN"),
                                      "testdata/rdata_dname_fromWire", 18),
                 InvalidRdataLength);
    // RDLENGTH is too long
    EXPECT_THROW(rdataFactoryFromFile(RRType("DNAME"), RRClass("IN"),
                                      "testdata/rdata_dname_fromWire", 36),
                 InvalidRdataLength);
    // incomplete name.  the error should be detected in the name constructor
    EXPECT_THROW(rdataFactoryFromFile(RRType("DNAME"), RRClass("IN"),
                                      "testdata/rdata_dname_fromWire", 71),
                 IncompleteName);

    EXPECT_EQ(0, generic::DNAME("dn2.example.com").compare(
                  *rdataFactoryFromFile(RRType("DNAME"), RRClass("IN"),
                                        "testdata/rdata_dname_fromWire", 55)));
    EXPECT_THROW(*rdataFactoryFromFile(RRType("DNAME"), RRClass("IN"),
                                       "testdata/rdata_dname_fromWire", 63),
                 InvalidRdataLength);
}

TEST_F(Rdata_DNAME_Test, toWireBuffer)
{
    rdata_dname.toWire(obuffer);
    EXPECT_PRED_FORMAT4(UnitTestUtil::matchWireData,
                        obuffer.getData(), obuffer.getLength(),
                        wiredata_dname, sizeof(wiredata_dname));
}

TEST_F(Rdata_DNAME_Test, toWireRenderer)
{
    rdata_dname.toWire(renderer);
    EXPECT_PRED_FORMAT4(UnitTestUtil::matchWireData,
                        obuffer.getData(), obuffer.getLength(),
                        wiredata_dname, sizeof(wiredata_dname));
    rdata_dname2.toWire(renderer);
    EXPECT_PRED_FORMAT4(UnitTestUtil::matchWireData,
                        obuffer.getData(), obuffer.getLength(),
                        wiredata_dname2, sizeof(wiredata_dname2));
}

TEST_F(Rdata_DNAME_Test, toText)
{
    EXPECT_EQ("dn.example.com.", rdata_dname.toText());
}

TEST_F(Rdata_DNAME_Test, getDname)
{
    EXPECT_EQ(Name("dn.example.com."), rdata_dname.getDname());
}
}