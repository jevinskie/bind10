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

#include <stdint.h>

#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <dns/name.h>
#include <dns/message.h>
#include <dns/rdata.h>
#include <dns/rrclass.h>
#include <dns/rrtype.h>
#include <dns/rdataclass.h>
#include <dns/rrsetlist.h>

#include "query.h"
#include "data_source.h"
#include "data_source_sqlite3.h"

using namespace std;
using namespace isc::dns;
using namespace isc::dns::rdata;
using namespace isc::auth;

namespace {
static const char* SQLITE_DBFILE_EXAMPLE = "testdata/test.sqlite3";

static const string sigdata_common(" 20100322084538 20100220084538 "
                                   "33495 example.com. FAKEFAKEFAKEFAKE");
static const string dnskey1_data(" AwEAAcOUBllYc1hf7ND9uDy+Yz1BF3sI0m4q"
                                 "NGV7WcTD0WEiuV7IjXgHE36fCmS9QsUxSSOV"
                                 "o1I/FMxI2PJVqTYHkXFBS7AzLGsQYMU7UjBZ"
                                 "SotBJ6Imt5pXMu+lEDNy8TOUzG3xm7g0qcbW"
                                 "YF6qCEfvZoBtAqi5Rk7Mlrqs8agxYyMx");
static const string dnskey2_data(" AwEAAe5WFbxdCPq2jZrZhlMj7oJdff3W7syJ"
                                 "tbvzg62tRx0gkoCDoBI9DPjlOQG0UAbj+xUV"
                                 "4HQZJStJaZ+fHU5AwVNT+bBZdtV+NujSikhd"
                                 "THb4FYLg2b3Cx9NyJvAVukHp/91HnWuG4T36"
                                 "CzAFrfPwsHIrBz9BsaIQ21VRkcmj7DswfI/i"
                                 "DGd8j6bqiODyNZYQ+ZrLmF0KIJ2yPN3iO6Zq"
                                 "23TaOrVTjB7d1a/h31ODfiHAxFHrkY3t3D5J"
                                 "R9Nsl/7fdRmSznwtcSDgLXBoFEYmw6p86Acv"
                                 "RyoYNcL1SXjaKVLG5jyU3UR+LcGZT5t/0xGf"
                                 "oIK/aKwENrsjcKZZj660b1M=");

static const Name zone_name("example.com");
static const Name nomatch_name("example.org");
static const Name www_name("www.example.com");
static const Name www_upper_name("WWW.EXAMPLE.COM");

typedef enum {
    NORMAL,
    EXACT,
    ADDRESS,
    REFERRAL
} FindMode;

class Sqlite3DataSourceTest : public ::testing::Test {
protected:
    Sqlite3DataSourceTest() : message(Message::PARSE),
                              query(NULL), rrclass(RRClass::IN()),
                              rrtype(RRType::A()), rrttl(RRTTL(3600)),
                              find_flags(0), rrset_matched(0), rrset_count(0)
    {
        data_source.init(SQLITE_DBFILE_EXAMPLE);

        // the data source will ignore the message, and the encapsulating
        // query object so the content doesn't matter.
        // (it's a bad practice, but is a different issue)
        message.addQuestion(Question(Name("example.org"), rrclass, rrtype));
        query = new Query(message, true);

        common_a_data.push_back("192.0.2.1");
        common_sig_data.push_back("A 5 3 3600" + sigdata_common);

        www_nsec_data.push_back("example.com. A RRSIG NSEC");
        www_nsec_sig_data.push_back("NSEC 5 3 7200" + sigdata_common);

        apex_soa_data.push_back("master.example.com. admin.example.com. "
                                "1234 3600 1800 2419200 7200");
        apex_soa_sig_data.push_back("SOA 5 2 3600" + sigdata_common);
        apex_ns_data.push_back("dns01.example.com.");
        apex_ns_data.push_back("dns02.example.com.");
        apex_ns_data.push_back("dns03.example.com.");
        apex_ns_sig_data.push_back("NS 5 2 3600" + sigdata_common);
        apex_mx_data.push_back("10 mail.example.com.");
        apex_mx_data.push_back("20 mail.subzone.example.com.");
        apex_mx_sig_data.push_back("MX 5 2 3600" + sigdata_common);
        apex_nsec_data.push_back("cname-ext.example.com. "
                                 "NS SOA MX RRSIG NSEC DNSKEY");
        apex_nsec_sig_data.push_back("NSEC 5 2 7200" + sigdata_common);
        apex_dnskey_data.push_back("256 3 5" + dnskey1_data);
        apex_dnskey_data.push_back("257 3 5" + dnskey2_data);
        // this one is special (using different key):
        apex_dnskey_sig_data.push_back("DNSKEY 5 2 3600 20100322084538 "
                                       "20100220084538 4456 example.com. "
                                       "FAKEFAKEFAKEFAKE");
        apex_dnskey_sig_data.push_back("DNSKEY 5 2 3600" + sigdata_common);

        wild_a_data.push_back("192.0.2.255");
        dname_data.push_back("sql1.example.com.");
        dname_sig_data.push_back("DNAME 5 3 3600" + sigdata_common);
        cname_data.push_back("cnametest.example.org.");
        cname_sig_data.push_back("CNAME 5 3 3600" + sigdata_common);
        cname_nsec_data.push_back("mail.example.com. CNAME RRSIG NSEC");
        cname_nsec_sig_data.push_back("NSEC 5 3 7200" + sigdata_common);
        delegation_ns_data.push_back("ns1.subzone.example.com.");
        delegation_ns_data.push_back("ns2.subzone.example.com.");
        delegation_ds_data.push_back("33313 5 1 "
                                     "FDD7A2C11AA7F55D50FBF9B7EDDA2322C541A8DE");
        delegation_ds_data.push_back("33313 5 2 "
                                     "0B99B7006F496D135B01AB17EDB469B4BE9E"
                                     "1973884DEA757BC4E3015A8C3AB3");
        delegation_ds_sig_data.push_back("DS 5 3 3600" + sigdata_common);
        delegation_nsec_data.push_back("*.wild.example.com. NS DS RRSIG NSEC");
        delegation_nsec_sig_data.push_back("NSEC 5 3 7200" + sigdata_common);
        child_a_data.push_back("192.0.2.100");
        child_sig_data.push_back("A 5 4 3600 20100322084536 "
                                 "20100220084536 12447 sql1.example.com. "
                                 "FAKEFAKEFAKEFAKE");
    }
    ~Sqlite3DataSourceTest() { delete query; }
    Sqlite3DataSrc data_source;
    Message message;
    Query* query;
    // we allow these to be modified in the test
    RRClass rrclass;
    RRType rrtype;
    RRTTL rrttl;
    RRsetList result_sets;
    uint32_t find_flags;
    unsigned rrset_matched;
    unsigned rrset_count;

    vector<RRType> types;
    vector<RRTTL> ttls;
    vector<const vector<string>* > answers;
    vector<const vector<string>* > signatures;

    vector<RRType> expected_types;
    vector<string> common_a_data;
    vector<string> common_sig_data;
    vector<string> www_nsec_data;
    vector<string> www_nsec_sig_data;
    vector<string> apex_soa_data;
    vector<string> apex_soa_sig_data;
    vector<string> apex_ns_data;
    vector<string> apex_ns_sig_data;
    vector<string> apex_mx_data;
    vector<string> apex_mx_sig_data;
    vector<string> apex_nsec_data;
    vector<string> apex_nsec_sig_data;
    vector<string> apex_dnskey_data;
    vector<string> apex_dnskey_sig_data;
    vector<string> wild_a_data;
    vector<string> dname_data;
    vector<string> dname_sig_data;
    vector<string> cname_data;
    vector<string> cname_sig_data;
    vector<string> cname_nsec_data;
    vector<string> cname_nsec_sig_data;
    vector<string> delegation_ns_data;
    vector<string> delegation_ns_sig_data;
    vector<string> delegation_ds_data;
    vector<string> delegation_ds_sig_data;
    vector<string> delegation_nsec_data;
    vector<string> delegation_nsec_sig_data;
    vector<string> child_a_data;
    vector<string> child_sig_data;
};

void
checkRRset(RRsetPtr rrset, const Name& expected_name,
           const RRClass& expected_class, const RRType& expected_type,
           const RRTTL& expected_rrttl, const vector<string>& expected_data,
           const vector<string>* expected_sig_data)
{
    EXPECT_EQ(expected_name, rrset->getName());
    EXPECT_EQ(expected_class, rrset->getClass());
    EXPECT_EQ(expected_type, rrset->getType());
    EXPECT_EQ(expected_rrttl, rrset->getTTL());

    RdataIteratorPtr rdata_iterator = rrset->getRdataIterator();
    rdata_iterator->first();
    vector<string>::const_iterator data_it = expected_data.begin();
    for (; data_it != expected_data.end(); ++data_it) {
        EXPECT_FALSE(rdata_iterator->isLast());
        if (rdata_iterator->isLast()) {
            // buggy case, should stop here
            break;
        }
        EXPECT_EQ(0, (rdata_iterator->getCurrent()).compare(
                      *createRdata(expected_type, expected_class, *data_it)));
        rdata_iterator->next();
    }

    if (expected_sig_data != NULL) {
        RRsetPtr sig_rrset = rrset->getRRsig();
        EXPECT_FALSE(NULL == sig_rrset);
        // Note: we assume the TTL for RRSIG is the same as that of the
        // RRSIG target.
        checkRRset(sig_rrset, expected_name, expected_class, RRType::RRSIG(),
                   expected_rrttl, *expected_sig_data, NULL);
    } else {
        EXPECT_TRUE(NULL == rrset->getRRsig());
    }

    EXPECT_TRUE(rdata_iterator->isLast());
}

void
checkFind(FindMode mode, const Sqlite3DataSrc& data_source, const Query& query,
          const Name& expected_name, const Name* zone_name,
          const RRClass& expected_class, const RRType& expected_type,
          const vector<RRTTL>& expected_ttls, const uint32_t expected_flags,
          const vector<RRType>& expected_types,
          const vector<const vector<string>* >& expected_answers,
          const vector<const vector<string>* >& expected_signatures)
{
    RRsetList result_sets;
    uint32_t find_flags;
    unsigned int rrset_matched = 0;
    unsigned int rrset_count = 0;

    switch (mode) {
    case NORMAL:
        EXPECT_EQ(DataSrc::SUCCESS,
                  data_source.findRRset(query, expected_name, expected_class,
                                        expected_type, result_sets, find_flags,
                                        zone_name));
        break;
    case EXACT:
        EXPECT_EQ(DataSrc::SUCCESS,
                  data_source.findExactRRset(query, expected_name,
                                             expected_class, expected_type,
                                             result_sets, find_flags,
                                             zone_name));
        break;
    case ADDRESS:
        EXPECT_EQ(DataSrc::SUCCESS,
                  data_source.findAddrs(query, expected_name, expected_class,
                                        result_sets, find_flags, zone_name));
        break;
    case REFERRAL:
        EXPECT_EQ(DataSrc::SUCCESS,
                  data_source.findReferral(query, expected_name, expected_class,
                                           result_sets, find_flags, zone_name));
        break;
    }
    EXPECT_EQ(expected_flags, find_flags);
    RRsetList::iterator it = result_sets.begin();
    for (; it != result_sets.end(); ++it) {
        vector<RRType>::const_iterator found_type =
            find(expected_types.begin(), expected_types.end(),
                 (*it)->getType());
        // there should be a match
        EXPECT_TRUE(found_type != expected_types.end());
        if (found_type != expected_types.end()) {
            unsigned int index = distance(expected_types.begin(), found_type);
            checkRRset(*it, expected_name, expected_class, (*it)->getType(),
                       expected_ttls[index], *expected_answers[index],
                       expected_signatures[index]);
            ++rrset_matched;
        }
        ++rrset_count;
    }
    EXPECT_EQ(expected_types.size(), rrset_count);
    EXPECT_EQ(expected_types.size(), rrset_matched);
}

void
checkFind(FindMode mode, const Sqlite3DataSrc& data_source, const Query& query,
          const Name& expected_name, const Name* zone_name,
          const RRClass& expected_class, const RRType& expected_type,
          const RRTTL& expected_rrttl, const uint32_t expected_flags,
          const vector<string>& expected_data,
          const vector<string>* expected_sig_data)
{
    vector<RRType> types;
    vector<RRTTL> ttls;
    vector<const vector<string>* > answers;
    vector<const vector<string>* > signatures;

    types.push_back(expected_type);
    ttls.push_back(expected_rrttl);
    answers.push_back(&expected_data);
    signatures.push_back(expected_sig_data);

    checkFind(mode, data_source, query, expected_name, zone_name, expected_class,
              expected_type, ttls, expected_flags, types, answers,
              signatures);
}

TEST_F(Sqlite3DataSourceTest, close) {
    EXPECT_EQ(DataSrc::SUCCESS, data_source.close());
}

TEST_F(Sqlite3DataSourceTest, findClosestEnclosure) {
    NameMatch name_match(www_name);
    data_source.findClosestEnclosure(name_match);
    EXPECT_EQ(zone_name, *name_match.closestName());
    EXPECT_EQ(&data_source, name_match.bestDataSrc());
}

TEST_F(Sqlite3DataSourceTest, findClosestEnclosureNoMatch) {
    NameMatch name_match(nomatch_name);
    data_source.findClosestEnclosure(name_match);
    EXPECT_EQ(NULL, name_match.closestName());
    EXPECT_EQ(NULL, name_match.bestDataSrc());
}

TEST_F(Sqlite3DataSourceTest, findRRsetNormal) {
    // Without specifying the zone name, and then with the zone name
    checkFind(NORMAL, data_source, *query, www_name, NULL, rrclass, rrtype,
              rrttl, 0, common_a_data, &common_sig_data);

    checkFind(NORMAL, data_source, *query, www_name, &zone_name, rrclass,
              rrtype, rrttl, 0, common_a_data, &common_sig_data);

    // With a zone name that doesn't match
    EXPECT_EQ(DataSrc::SUCCESS,
              data_source.findRRset(*query, www_name, rrclass, rrtype,
                                    result_sets, find_flags, &nomatch_name));
    EXPECT_EQ(DataSrc::NO_SUCH_ZONE, find_flags);
    EXPECT_TRUE(result_sets.begin() == result_sets.end()); // should be empty
}

TEST_F(Sqlite3DataSourceTest, findRRsetNormalANY) {
    types.push_back(RRType::A());
    types.push_back(RRType::NSEC());
    ttls.push_back(RRTTL(3600));
    ttls.push_back(RRTTL(7200));
    answers.push_back(&common_a_data);
    answers.push_back(&www_nsec_data);
    signatures.push_back(&common_sig_data);
    signatures.push_back(&www_nsec_sig_data);

    rrtype = RRType::ANY();
    checkFind(NORMAL, data_source, *query, www_name, NULL, rrclass, rrtype,
              ttls, 0, types, answers, signatures);

    checkFind(NORMAL, data_source, *query, www_name, &zone_name, rrclass,
              rrtype, ttls, 0, types, answers, signatures);
}

// Case insensitive lookup
TEST_F(Sqlite3DataSourceTest, findRRsetNormalCase) {
    checkFind(NORMAL, data_source, *query, www_upper_name, NULL, rrclass,
              rrtype, rrttl, 0, common_a_data, &common_sig_data);

    checkFind(NORMAL, data_source, *query, www_upper_name, &zone_name, rrclass,
              rrtype, rrttl, 0, common_a_data, &common_sig_data);

    EXPECT_EQ(DataSrc::SUCCESS,
              data_source.findRRset(*query, www_upper_name, rrclass, rrtype,
                                    result_sets, find_flags, &nomatch_name));
    EXPECT_EQ(DataSrc::NO_SUCH_ZONE, find_flags);
    EXPECT_TRUE(result_sets.begin() == result_sets.end()); // should be empty
}

TEST_F(Sqlite3DataSourceTest, findRRsetApexNS) {
    rrtype = RRType::NS();
    checkFind(NORMAL, data_source, *query, zone_name, NULL, rrclass, rrtype,
              rrttl, DataSrc::REFERRAL, apex_ns_data, &apex_ns_sig_data);

    checkFind(NORMAL, data_source, *query, zone_name, &zone_name, rrclass,
              rrtype, rrttl, DataSrc::REFERRAL, apex_ns_data,
              &apex_ns_sig_data);

    EXPECT_EQ(DataSrc::SUCCESS,
              data_source.findRRset(*query, zone_name, rrclass, rrtype,
                                    result_sets, find_flags, &nomatch_name));
    EXPECT_EQ(DataSrc::NO_SUCH_ZONE, find_flags);
    EXPECT_TRUE(result_sets.begin() == result_sets.end()); // should be empty
}

TEST_F(Sqlite3DataSourceTest, findRRsetApexANY) {
    types.push_back(RRType::SOA());
    types.push_back(RRType::NS());
    types.push_back(RRType::MX());
    types.push_back(RRType::NSEC());
    types.push_back(RRType::DNSKEY());
    ttls.push_back(rrttl); // SOA TTL
    ttls.push_back(rrttl); // NS TTL
    ttls.push_back(rrttl); // MX TTL
    ttls.push_back(RRTTL(7200)); // NSEC TTL
    ttls.push_back(rrttl); // DNSKEY TTL
    answers.push_back(&apex_soa_data);
    answers.push_back(&apex_ns_data);
    answers.push_back(&apex_mx_data);
    answers.push_back(&apex_nsec_data);
    answers.push_back(&apex_dnskey_data);
    signatures.push_back(&apex_soa_sig_data);
    signatures.push_back(&apex_ns_sig_data);
    signatures.push_back(&apex_mx_sig_data);
    signatures.push_back(&apex_nsec_sig_data);
    signatures.push_back(&apex_dnskey_sig_data);

    rrtype = RRType::ANY();
    checkFind(NORMAL, data_source, *query, zone_name, NULL, rrclass, rrtype,
              ttls, 0, types, answers, signatures);

    checkFind(NORMAL, data_source, *query, zone_name, &zone_name, rrclass,
              rrtype, ttls, 0, types, answers, signatures);
}

TEST_F(Sqlite3DataSourceTest, findRRsetApexNXRRSET) {
    rrtype = RRType::AAAA();
    EXPECT_EQ(DataSrc::SUCCESS,
              data_source.findRRset(*query, zone_name, rrclass, rrtype,
                                    result_sets, find_flags, &zone_name));
    // there's an NS RRset at the apex name, so the REFERRAL flag should be
    // set, too.
    EXPECT_EQ(DataSrc::TYPE_NOT_FOUND | DataSrc::REFERRAL, find_flags);
    EXPECT_TRUE(result_sets.begin() == result_sets.end());

    // Same test, without specifying the zone name
    EXPECT_EQ(DataSrc::SUCCESS,
              data_source.findRRset(*query, zone_name, rrclass, rrtype,
                                    result_sets, find_flags, NULL));
    // there's an NS RRset at the apex name, so the REFERRAL flag should be
    // set, too.
    EXPECT_EQ(DataSrc::TYPE_NOT_FOUND | DataSrc::REFERRAL, find_flags);
    EXPECT_TRUE(result_sets.begin() == result_sets.end());
}

// Matching a wildcard node.  There's nothing special for the data source API
// point of view, but perform minimal tests anyway.
TEST_F(Sqlite3DataSourceTest, findRRsetWildcard) {
    const Name qname("*.wild.example.com");
    checkFind(NORMAL, data_source, *query, qname, NULL, rrclass,
              rrtype, rrttl, 0, wild_a_data, &common_sig_data);
    checkFind(NORMAL, data_source, *query, qname, &zone_name, rrclass,
              rrtype, rrttl, 0, wild_a_data, &common_sig_data);
}

TEST_F(Sqlite3DataSourceTest, findRRsetEmptyNode) {
    // foo.bar.example.com exists, but bar.example.com doesn't have any data.
    const Name qname("bar.example.com");

    EXPECT_EQ(DataSrc::SUCCESS,
              data_source.findRRset(*query, qname, rrclass, rrtype,
                                    result_sets, find_flags, NULL));
    EXPECT_EQ(DataSrc::TYPE_NOT_FOUND, find_flags);
    EXPECT_TRUE(result_sets.begin() == result_sets.end());

    EXPECT_EQ(DataSrc::SUCCESS,
              data_source.findRRset(*query, qname, rrclass, rrtype,
                                    result_sets, find_flags, &zone_name));
    EXPECT_EQ(DataSrc::TYPE_NOT_FOUND, find_flags);
    EXPECT_TRUE(result_sets.begin() == result_sets.end());
}

// There's nothing special about DNAME lookup for the data source API
// point of view, but perform minimal tests anyway.
TEST_F(Sqlite3DataSourceTest, findRRsetDNAME) {
    const Name qname("dname.example.com");

    rrtype = RRType::DNAME();
    checkFind(NORMAL, data_source, *query, qname, NULL, rrclass,
              rrtype, rrttl, 0, dname_data, &dname_sig_data);
    checkFind(NORMAL, data_source, *query, qname, &zone_name, rrclass,
              rrtype, rrttl, 0, dname_data, &dname_sig_data);
}

TEST_F(Sqlite3DataSourceTest, findRRsetCNAME) {
    const Name qname("foo.example.com");

    // This qname only has the CNAME (+ sigs).  CNAME query is not different
    // from ordinary queries.
    rrtype = RRType::CNAME();
    checkFind(NORMAL, data_source, *query, qname, NULL, rrclass,
              rrtype, rrttl, 0, cname_data, &cname_sig_data);
    checkFind(NORMAL, data_source, *query, qname, &zone_name, rrclass,
              rrtype, rrttl, 0, cname_data, &cname_sig_data);

    // queries for (ordinary) different RR types that match the CNAME.
    // CNAME_FOUND flag is set, and the CNAME RR is returned instead of A
    rrtype = RRType::A();
    types.push_back(RRType::CNAME());
    ttls.push_back(rrttl);
    answers.push_back(&cname_data);
    signatures.push_back(&cname_sig_data);
    checkFind(NORMAL, data_source, *query, qname, NULL, rrclass,
              rrtype, ttls, DataSrc::CNAME_FOUND, types, answers, signatures);
    checkFind(NORMAL, data_source, *query, qname, &zone_name, rrclass,
              rrtype, ttls, DataSrc::CNAME_FOUND, types, answers, signatures);

    // NSEC query that match the CNAME.
    // CNAME_FOUND flag is NOT set, and the NSEC RR is returned instead of
    // CNAME.
    rrtype = RRType::NSEC();
    checkFind(NORMAL, data_source, *query, qname, NULL, rrclass,
              rrtype, RRTTL(7200), 0, cname_nsec_data, &cname_nsec_sig_data);
    checkFind(NORMAL, data_source, *query, qname, &zone_name, rrclass,
              rrtype, RRTTL(7200), 0, cname_nsec_data, &cname_nsec_sig_data);
}

TEST_F(Sqlite3DataSourceTest, findRRsetDelegation) {
    const Name qname("www.subzone.example.com");

    // query for a name under a zone cut.  From the data source API point
    // of view this is no different than "NXDOMAIN".
    EXPECT_EQ(DataSrc::SUCCESS,
              data_source.findRRset(*query, qname, rrclass, rrtype,
                                    result_sets, find_flags, NULL));
    EXPECT_EQ(DataSrc::NAME_NOT_FOUND, find_flags);
    EXPECT_TRUE(result_sets.begin() == result_sets.end());
}

TEST_F(Sqlite3DataSourceTest, findRRsetDelegationAtZoneCut) {
    const Name qname("subzone.example.com");

    // query for a name *at* a zone cut.  It matches the NS RRset for the
    // delegation.

    // For non-NS ordinary queries, "no type" should be set too, and no RRset is
    // returned.
    EXPECT_EQ(DataSrc::SUCCESS,
              data_source.findRRset(*query, qname, rrclass, rrtype,
                                    result_sets, find_flags, NULL));
    EXPECT_EQ(DataSrc::TYPE_NOT_FOUND | DataSrc::REFERRAL, find_flags);
    EXPECT_TRUE(result_sets.begin() == result_sets.end());

    EXPECT_EQ(DataSrc::SUCCESS,
              data_source.findRRset(*query, qname, rrclass, rrtype,
                                    result_sets, find_flags, &zone_name));
    EXPECT_EQ(DataSrc::TYPE_NOT_FOUND | DataSrc::REFERRAL, find_flags);
    EXPECT_TRUE(result_sets.begin() == result_sets.end());

    // For NS query, RRset is returned with the REFERRAL flag.  No RRSIG should
    // be provided.
    rrtype = RRType::NS();
    checkFind(NORMAL, data_source, *query, qname, NULL, rrclass,
              rrtype, rrttl, DataSrc::REFERRAL, delegation_ns_data, NULL);
    checkFind(NORMAL, data_source, *query, qname, &zone_name, rrclass,
              rrtype, rrttl, DataSrc::REFERRAL, delegation_ns_data, NULL);

    // For ANY query.  What should we do?
#if 0
    rrtype = RRType::ANY();
    EXPECT_EQ(DataSrc::SUCCESS,
              data_source.findRRset(*query, qname, rrclass, rrtype,
                                    result_sets, find_flags, NULL));
    EXPECT_EQ(DataSrc::REFERRAL, find_flags);
#endif

    // For NSEC query.  What should we do?  Probably return the NSEC + RRSIG
    // without REFERRAL.  But it currently doesn't act like so.
#if 0
    rrtype = RRType::NSEC();
    checkFind(NORMAL, data_source, *query, qname, NULL, rrclass,
              rrtype, RRTTL(7200), 0, delegation_nsec_data,
              &delegation_nsec_sig_data);
    checkFind(NORMAL, data_source, *query, qname, &zone_name, rrclass,
              rrtype, RRTTL(7200), 0, delegation_nsec_data,
              &delegation_nsec_sig_data);
#endif    
}

TEST_F(Sqlite3DataSourceTest, findRRsetInChildZone) {
    const Name qname("www.sql1.example.com");
    const Name child_zone_name("sql1.example.com");

    // If we don't specify the zone, the data source should identify the
    // best matching zone.
    checkFind(NORMAL, data_source, *query, qname, NULL, rrclass,
              rrtype, rrttl, 0, child_a_data, &child_sig_data);

    // If we specify the parent zone, it's treated as NXDOMAIN because it's
    // under a zone cut.
    EXPECT_EQ(DataSrc::SUCCESS,
              data_source.findRRset(*query, qname, rrclass, rrtype,
                                    result_sets, find_flags, &zone_name));
    EXPECT_EQ(DataSrc::NAME_NOT_FOUND, find_flags);
    EXPECT_TRUE(result_sets.begin() == result_sets.end());

    // If we specify the child zone, it should be the same as the first case.
    checkFind(NORMAL, data_source, *query, qname, &child_zone_name, rrclass,
              rrtype, rrttl, 0, child_a_data, &child_sig_data);
}

TEST_F(Sqlite3DataSourceTest, findExactRRset) {
    // Normal case.  No different than findRRset.
    checkFind(EXACT, data_source, *query, www_name, &zone_name, rrclass, rrtype,
              rrttl, 0, common_a_data, &common_sig_data);
}

TEST_F(Sqlite3DataSourceTest, findExactRRsetCNAME) {
    const Name qname("foo.example.com");

    // This qname only has the CNAME (+ sigs).  In this case it should be
    // no different than findRRset for CNAME query.
    rrtype = RRType::CNAME();
    checkFind(NORMAL, data_source, *query, qname, &zone_name, rrclass,
              rrtype, rrttl, 0, cname_data, &cname_sig_data);

    // queries for (ordinary) different RR types that match the CNAME.
    // "type not found" set, but CNAME and its sig are returned (awkward,
    // but that's how it currently works).
    rrtype = RRType::A();
    types.push_back(RRType::CNAME());
    ttls.push_back(rrttl);
    answers.push_back(&cname_data);
    signatures.push_back(&cname_sig_data);
    checkFind(EXACT, data_source, *query, qname, &zone_name, rrclass,
              rrtype, ttls, DataSrc::TYPE_NOT_FOUND, types, answers,
              signatures);
}

#if 0 // this should work, but doesn't.  maybe the loadzone tool is broken?
TEST_F(Sqlite3DataSourceTest, findExactRRsetReferral) {
    // A referral lookup searches for NS, DS, or DNAME, or their sigs.
    const Name qname("sql1.example.com");

    types.push_back(RRType::NS());
    types.push_back(RRType::DS());
    ttls.push_back(rrttl);
    ttls.push_back(rrttl);
    answers.push_back(&apex_ns_data);
    answers.push_back(&delegation_ds_data);
    signatures.push_back(NULL);
    signatures.push_back(&delegation_ds_sig_data);
    // Note: zone_name matters here because we need to perform the search
    // in the parent zone.
    checkFind(REFERRAL, data_source, *query, qname, &zone_name, rrclass,
              rrtype, ttls, 0, types, answers, signatures);
}
#endif

}