// Copyright (C) 2012  Internet Systems Consortium, Inc. ("ISC")
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

#include <string>

#include <gtest/gtest.h>

#include <boost/scoped_ptr.hpp>

#include <dns/nsec3hash.h>
#include <dns/rdataclass.h>

using boost::scoped_ptr;
using namespace std;
using namespace isc::dns;
using namespace isc::dns::rdata;

namespace {
typedef scoped_ptr<NSEC3Hash> NSEC3HashPtr;

// Commonly used NSEC3 suffix, defined to reduce amount of type
const char* const nsec3_common = "2T7B4G4VSA5SMI47K61MV5BV1A22BOJR A RRSIG";

class NSEC3HashTest : public ::testing::Test {
protected:
    NSEC3HashTest() :
        test_hash(NSEC3Hash::create(generic::NSEC3PARAM("1 0 12 aabbccdd"))),
        test_hash_nsec3(NSEC3Hash::create(generic::NSEC3
                                          ("1 0 12 aabbccdd " +
                                           string(nsec3_common))))
    {}

    // An NSEC3Hash object commonly used in tests.  Parameters are borrowed
    // from the RFC5155 example.  Construction of this object implicitly
    // checks a successful case of the creation.
    NSEC3HashPtr test_hash;

    // Similar to test_hash, but created from NSEC3 RR.
    NSEC3HashPtr test_hash_nsec3;
};

TEST_F(NSEC3HashTest, unknownAlgorithm) {
    EXPECT_THROW(NSEC3HashPtr(
                     NSEC3Hash::create(
                         generic::NSEC3PARAM("2 0 12 aabbccdd"))),
                     UnknownNSEC3HashAlgorithm);
    EXPECT_THROW(NSEC3HashPtr(
                     NSEC3Hash::create(
                         generic::NSEC3("2 0 12 aabbccdd " +
                                        string(nsec3_common)))),
                     UnknownNSEC3HashAlgorithm);
}

// Common checks for NSEC3 hash calculation
void
calculateCheck(NSEC3Hash& hash) {
    // A couple of normal cases from the RFC5155 example.
    EXPECT_EQ("0P9MHAVEQVM6T7VBL5LOP2U3T2RP3TOM",
              hash.calculate(Name("example")));
    EXPECT_EQ("35MTHGPGCU1QG68FAB165KLNSNK3DPVL",
              hash.calculate(Name("a.example")));

    // Check case-insensitiveness
    EXPECT_EQ("0P9MHAVEQVM6T7VBL5LOP2U3T2RP3TOM",
              hash.calculate(Name("EXAMPLE")));
}

TEST_F(NSEC3HashTest, calculate) {
    {
        SCOPED_TRACE("calculate check with NSEC3PARAM based hash");
        calculateCheck(*test_hash);
    }
    {
        SCOPED_TRACE("calculate check with NSEC3 based hash");
        calculateCheck(*test_hash_nsec3);
    }

    // Some boundary cases: 0-iteration and empty salt.  Borrowed from the
    // .com zone data.
    EXPECT_EQ("CK0POJMG874LJREF7EFN8430QVIT8BSM",
              NSEC3HashPtr(NSEC3Hash::create(generic::NSEC3PARAM("1 0 0 -")))
              ->calculate(Name("com")));

    // Using unusually large iterations, something larger than the 8-bit range.
    // (expected hash value generated by BIND 9's dnssec-signzone)
    EXPECT_EQ("COG6A52MJ96MNMV3QUCAGGCO0RHCC2Q3",
              NSEC3HashPtr(NSEC3Hash::create(
                               generic::NSEC3PARAM("1 0 256 AABBCCDD")))
              ->calculate(Name("example.org")));
}

// Common checks for match cases
template <typename RDATAType>
void
matchCheck(NSEC3Hash& hash, const string& postfix) {
    // If all parameters match, it's considered to be matched.
    EXPECT_TRUE(hash.match(RDATAType("1 0 12 aabbccdd" + postfix)));

    // Algorithm doesn't match
    EXPECT_FALSE(hash.match(RDATAType("2 0 12 aabbccdd" + postfix)));
    // Iterations doesn't match
    EXPECT_FALSE(hash.match(RDATAType("1 0 1 aabbccdd" + postfix)));
    // Salt doesn't match
    EXPECT_FALSE(hash.match(RDATAType("1 0 12 aabbccde" + postfix)));
    // Salt doesn't match: the other has an empty salt
    EXPECT_FALSE(hash.match(RDATAType("1 0 12 -" + postfix)));
    // Flags don't matter
    EXPECT_TRUE(hash.match(RDATAType("1 1 12 aabbccdd" + postfix)));
}

TEST_F(NSEC3HashTest, matchWithNSEC3) {
    {
        SCOPED_TRACE("match NSEC3PARAM based hash against NSEC3 parameters");
        matchCheck<generic::NSEC3>(*test_hash, " " + string(nsec3_common));
    }
    {
        SCOPED_TRACE("match NSEC3 based hash against NSEC3 parameters");
        matchCheck<generic::NSEC3>(*test_hash_nsec3,
                                   " " + string(nsec3_common));
    }
}

TEST_F(NSEC3HashTest, matchWithNSEC3PARAM) {
    {
        SCOPED_TRACE("match NSEC3PARAM based hash against NSEC3 parameters");
        matchCheck<generic::NSEC3PARAM>(*test_hash, "");
    }
    {
        SCOPED_TRACE("match NSEC3 based hash against NSEC3 parameters");
        matchCheck<generic::NSEC3PARAM>(*test_hash_nsec3, "");
    }
}

} // end namespace