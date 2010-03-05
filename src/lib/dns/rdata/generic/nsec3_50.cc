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

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <vector>

#include "base32.h"
#include "buffer.h"
#include "hex.h"
#include "messagerenderer.h"
#include "name.h"
#include "rrtype.h"
#include "rrttl.h"
#include "rdata.h"
#include "rdataclass.h"
#include <boost/lexical_cast.hpp>

#include <stdio.h>
#include <time.h>

using namespace std;

// BEGIN_ISC_NAMESPACE
// BEGIN_RDATA_NAMESPACE

struct NSEC3Impl {
    // straightforward representation of NSEC3 RDATA fields
    NSEC3Impl(uint8_t hash, uint8_t flags, uint16_t iterations,
              vector<uint8_t>salt, vector<uint8_t>next,
              vector<uint8_t> typebits) :
        hash_(hash), flags_(flags), iterations_(iterations),
        salt_(salt), next_(next), typebits_(typebits)
    {}

    uint8_t hash_;
    uint8_t flags_;
    uint16_t iterations_;
    vector<uint8_t> salt_;
    vector<uint8_t> next_;
    vector<uint8_t> typebits_;
};

NSEC3::NSEC3(const string& nsec3_str) :
    impl_(NULL)
{
    istringstream iss(nsec3_str);
    unsigned int hash, flags, iterations;
    string salthex;

    iss >> hash >> flags >> iterations >> salthex;
    if (iss.bad() || iss.fail()) {
        dns_throw(InvalidRdataText, "Invalid NSEC3 text");
    }
    if (hash > 0xf) {
        dns_throw(InvalidRdataText, "NSEC3 hash out of range");
    }
    if (flags > 0xff) {
        dns_throw(InvalidRdataText, "NSEC3 flags out of range");
    }
    if (iterations > 0xffff) {
        dns_throw(InvalidRdataText, "NSEC3 iterations out of range");
    }

    vector<uint8_t> salt;
    decodeHex(salthex, salt);

    string nextstr;
    iss >> setw(32) >> nextstr;
    vector<uint8_t> next;
    if (iss.bad() || iss.fail()) {
        dns_throw(InvalidRdataText, "Invalid NSEC3 hash");
    }
    decodeBase32(nextstr, next);

    uint8_t bitmap[8 * 1024];       // 64k bits
    vector<uint8_t> typebits;

    memset(bitmap, 0, sizeof(bitmap));
    do { 
        string type;
        int code;
        iss >> type;
        code = RRType(type).getCode();
        bitmap[code / 8] |= (0x80 >> (code % 8));
    } while(!iss.eof());

    for (int window = 0; window < 256; window++) {
        int octet;
        for (octet = 31; octet >= 0; octet--) {
            if (bitmap[window * 32 + octet] != 0) {
                break;
            }
        }
        if (octet < 0)
            continue;
        typebits.push_back(window);
        typebits.push_back(octet + 1);
        for (int i = 0; i <= octet; i++) {
            typebits.push_back(bitmap[window * 32 + i]);
        }
    }

    impl_ = new NSEC3Impl(hash, flags, iterations, salt, next, typebits);
}

NSEC3::NSEC3(InputBuffer& buffer, size_t rdata_len)
{
    if (rdata_len < 5) {
        dns_throw(InvalidRdataLength, "NSEC3 too short");
    }

    uint8_t hash = buffer.readUint8();
    uint8_t flags = buffer.readUint8();
    uint16_t iterations = buffer.readUint16();
    rdata_len -= 4;

    uint8_t saltlen = buffer.readUint8();
    --rdata_len;

    if (rdata_len < saltlen) {
        dns_throw(InvalidRdataLength, "NSEC3 salt too short");
    }

    vector<uint8_t> salt(saltlen);
    buffer.readData(&salt[0], saltlen);
    rdata_len -= saltlen;

    uint8_t nextlen = buffer.readUint8();
    --rdata_len;

    if (rdata_len < nextlen) {
        dns_throw(InvalidRdataLength, "NSEC3 next hash too short");
    }

    vector<uint8_t> next(nextlen);
    buffer.readData(&next[0], nextlen);
    rdata_len -= nextlen;

    if (rdata_len == 0) {
        dns_throw(InvalidRdataLength, "NSEC3 type bitmap too short");
    }

    // FIXIT: we cannot naively copy the data because the bitmaps have
    // semantics and other part of this class assumes they are valid.
    vector<uint8_t> typebits(rdata_len);
    buffer.readData(&typebits[0], rdata_len);

    impl_ = new NSEC3Impl(hash, flags, iterations, salt, next, typebits);
}

NSEC3::NSEC3(const NSEC3& source) :
    impl_(new NSEC3Impl(*source.impl_))
{}

NSEC3&
NSEC3::operator=(const NSEC3& source)
{
    if (impl_ == source.impl_) {
        return (*this);
    }

    NSEC3Impl* newimpl = new NSEC3Impl(*source.impl_);
    delete impl_;
    impl_ = newimpl;

    return (*this);
}

NSEC3::~NSEC3()
{
    delete impl_;
}

string
NSEC3::toText() const
{
    ostringstream s;
    int len = 0;
    for (int i = 0; i < impl_->typebits_.size(); i += len) {
        assert(i + 2 <= impl_->typebits_.size());
        int window = impl_->typebits_[i];
        len = impl_->typebits_[i + 1];
        assert(len >= 0 && len < 32);
        i += 2;
        for (int j = 0; j < len; j++) {
            if (impl_->typebits_[i + j] == 0) {
                continue;
            }
            for (int k = 0; k < 8; k++) {
                if ((impl_->typebits_[i + j] & (0x80 >> k)) == 0) {
                    continue;
                }
                int t = window * 256 + j * 8 + k;
                s << " " << RRType(t).toText();
            }
        }
    }

    using namespace boost;
    return (lexical_cast<string>(static_cast<int>(impl_->hash_)) +
        " " + lexical_cast<string>(static_cast<int>(impl_->flags_)) +
        " " + lexical_cast<string>(static_cast<int>(impl_->iterations_)) +
        " " + encodeHex(impl_->salt_) +
        " " + encodeBase32(impl_->next_) + s.str());
}

void
NSEC3::toWire(OutputBuffer& buffer) const
{
    buffer.writeUint8(impl_->hash_);
    buffer.writeUint8(impl_->flags_);
    buffer.writeUint16(impl_->iterations_);
    buffer.writeUint8(impl_->salt_.size());
    buffer.writeData(&impl_->salt_[0], impl_->salt_.size());
    buffer.writeUint8(impl_->next_.size());
    buffer.writeData(&impl_->next_[0], impl_->next_.size());
    buffer.writeData(&impl_->typebits_[0], impl_->typebits_.size());
}

void
NSEC3::toWire(MessageRenderer& renderer) const
{
    renderer.writeUint8(impl_->hash_);
    renderer.writeUint8(impl_->flags_);
    renderer.writeUint16(impl_->iterations_);
    renderer.writeUint8(impl_->salt_.size());
    renderer.writeData(&impl_->salt_[0], impl_->salt_.size());
    renderer.writeUint8(impl_->next_.size());
    renderer.writeData(&impl_->next_[0], impl_->next_.size());
    renderer.writeData(&impl_->typebits_[0], impl_->typebits_.size());
}

int
NSEC3::compare(const Rdata& other) const
{
    const NSEC3& other_nsec3 = dynamic_cast<const NSEC3&>(other);

    if (impl_->hash_ != other_nsec3.impl_->hash_) {
        return (impl_->hash_ < other_nsec3.impl_->hash_ ? -1 : 1);
    }
    if (impl_->flags_ != other_nsec3.impl_->flags_) {
        return (impl_->flags_ < other_nsec3.impl_->flags_ ? -1 : 1);
    }
    if (impl_->iterations_ != other_nsec3.impl_->iterations_) {
        return (impl_->iterations_ < other_nsec3.impl_->iterations_ ? -1 : 1);
    }

    size_t this_len = impl_->salt_.size();
    size_t other_len = other_nsec3.impl_->salt_.size();
    size_t cmplen = min(this_len, other_len);
    int cmp = memcmp(&impl_->salt_[0], &other_nsec3.impl_->salt_[0], cmplen);
    if (cmp != 0) {
        return (cmp);
    } else if (this_len < other_len) {
        return (-1);
    } else if (this_len > other_len) {
        return (1);
    }

    this_len = impl_->salt_.size();
    other_len = other_nsec3.impl_->salt_.size();
    cmplen = min(this_len, other_len);
    cmp = memcmp(&impl_->next_[0], &other_nsec3.impl_->next_[0], cmplen);
    if (cmp != 0) {
        return (cmp);
    } else if (this_len < other_len) {
        return (-1);
    } else if (this_len > other_len) {
        return (1);
    }

    this_len = impl_->typebits_.size();
    other_len = other_nsec3.impl_->typebits_.size();
    cmplen = min(this_len, other_len);
    cmp = memcmp(&impl_->typebits_[0], &other_nsec3.impl_->typebits_[0],
                 cmplen);
    if (cmp != 0) {
        return (cmp);
    } else if (this_len < other_len) {
        return (-1);
    } else if (this_len > other_len) {
        return (1);
    } else {
        return (0);
    }
}

uint8_t
NSEC3::getHash() const {
    return impl_->hash_;
}

uint8_t
NSEC3::getFlags() const {
    return impl_->flags_;
}

uint16_t
NSEC3::getIterations() const {
    return impl_->iterations_;
}

vector<uint8_t>
NSEC3::getSalt() const {
    return impl_->salt_;
}

// END_RDATA_NAMESPACE
// END_ISC_NAMESPACE