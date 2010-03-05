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

#include <string>

#include <time.h>

#include <dns/dnstime.h>

#include <gtest/gtest.h>

using namespace std;
using namespace isc::dns;

namespace {

TEST(DNSTimeTest, fromText)
{
    // We need test cases after overwrapping.
    //DNSSECTimeFromText("99991231235959");

    // These are bogus and should be rejected
    //EXPECT_THROW(DNSSECTimeFromText("2011 101120000"), InvalidTime);
    //EXPECT_THROW(DNSSECTimeFromText("201101011200-0"), InvalidTime);
    // Short length
    EXPECT_THROW(DNSSECTimeFromText("20100223"), InvalidTime);

    // Leap year checks
    EXPECT_THROW(DNSSECTimeFromText("20110229120000"), InvalidTime);
    EXPECT_THROW(DNSSECTimeFromText("21000229120000"), InvalidTime);
    EXPECT_NO_THROW(DNSSECTimeFromText("20000229120000"));
    EXPECT_NO_THROW(DNSSECTimeFromText("20120229120000"));

    // unusual case: this implementation allows SS=60 for "leap seconds"
    EXPECT_NO_THROW(DNSSECTimeFromText("20110101120060"));

    // Out of range parameters
    EXPECT_THROW(DNSSECTimeFromText("19100223214617"), InvalidTime); // YY<1970
    EXPECT_THROW(DNSSECTimeFromText("20110001120000"), InvalidTime); // MM=00
    EXPECT_THROW(DNSSECTimeFromText("20111301120000"), InvalidTime); // MM=13
    EXPECT_THROW(DNSSECTimeFromText("20110100120000"), InvalidTime); // DD=00
    EXPECT_THROW(DNSSECTimeFromText("20110132120000"), InvalidTime); // DD=32
    EXPECT_THROW(DNSSECTimeFromText("20110431120000"), InvalidTime); // 'Apr31'
    EXPECT_THROW(DNSSECTimeFromText("20110101250000"), InvalidTime); // HH=25
    EXPECT_THROW(DNSSECTimeFromText("20110101126000"), InvalidTime); // mm=60
    EXPECT_THROW(DNSSECTimeFromText("20110101120061"), InvalidTime); // SS=61
}

TEST(DNSTimeTest, fromTime)
{
    // Jan 1, Year 10,000.  What should we do?
    //EXPECT_EQ("", DNSSECTimeToText(253402300800));
}
}