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
#include <string.h>

#include <string>
#include <vector>

#include "buffer.h"
#include "messagerenderer.h"
#include "rdata.h"
#include "rdataclass.h"

using namespace std;

// BEGIN_ISC_NAMESPACE
// BEGIN_RDATA_NAMESPACE

TXT::TXT(InputBuffer& buffer, size_t rdata_len)
{
    uint8_t len;

    // TBD: this is a simple, incomplete implementation that only supports
    // a single character-string.
    len = buffer.readUint8();
    vector<uint8_t> data(len + 1);
    data[0] = len;
    buffer.readData(&data[0] + 1, len);
    string_list_.push_back(data);
}

TXT::TXT(const std::string& txtstr)
{
    size_t length = txtstr.size();
    size_t pos_begin = 0;

    if (length > 1 && txtstr[0] == '"' && txtstr[length - 1] == '"') {
        pos_begin = 1;
        length -= 2;
    }
    if (length > MAX_CHARSTRING_LEN) {
        dns_throw(CharStringTooLong, "");
    }

    vector<uint8_t> data;
    data.reserve(length + 1);
    data.push_back(length);
    data.insert(data.end(), txtstr.begin() + pos_begin,
                txtstr.begin() + pos_begin + length);
    string_list_.push_back(data);
}

TXT::TXT(const TXT& other) :
    string_list_(other.string_list_)
{}

void
TXT::toWire(OutputBuffer& buffer) const
{
    for (vector<vector<uint8_t> >::const_iterator it = string_list_.begin();
         it != string_list_.end();
         ++it)
    {
        buffer.writeData(&(*it)[0], (*it).size());
    }
}

void
TXT::toWire(MessageRenderer& renderer) const
{
    for (vector<vector<uint8_t> >::const_iterator it = string_list_.begin();
         it != string_list_.end();
         ++it)
    {
        renderer.writeData(&(*it)[0], (*it).size());
    }
}

string
TXT::toText() const
{
    string s;

    // XXX: this implementation is not entirely correct.  for example, it
    // should escape double-quotes if they appear in the character string.
    for (vector<vector<uint8_t> >::const_iterator it = string_list_.begin();
         it != string_list_.end();
         ++it)
    {
        if (!s.empty()) {
            s.push_back(' ');
        }
        s.push_back('"');
        s.insert(s.end(), (*it).begin() + 1, (*it).end());
        s.push_back('"');
    }

    return (s);
}

int
TXT::compare(const Rdata& other) const
{
    const TXT& other_txt = dynamic_cast<const TXT&>(other);

    // This implementation is not efficient.  Revisit this (TBD).
    OutputBuffer this_buffer(0);
    toWire(this_buffer);
    size_t this_len = this_buffer.getLength();

    OutputBuffer other_buffer(0);
    other_txt.toWire(other_buffer);
    size_t other_len = other_buffer.getLength();

    size_t cmplen = min(this_len, other_len);
    int cmp = memcmp(this_buffer.getData(), other_buffer.getData(), cmplen);
    if (cmp != 0) {
        return (cmp);
    } else {
        return ((this_len == other_len) ? 0 :
                (this_len < other_len) ? -1 : 1);
    }
}

// END_RDATA_NAMESPACE
// END_ISC_NAMESPACE