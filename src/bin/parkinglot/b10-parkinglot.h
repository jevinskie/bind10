// Copyright (C) 2009  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef __PARKINGLOT_H
#define __PARKINGLOT_H 1

#include "zoneset.h"
#include <cc/data.h>
#include "data_source_plot.h"

class ParkingLot {
public:
    explicit ParkingLot(int port);
    virtual ~ParkingLot() {}
    int getSocket() { return (sock); }
    void processMessage();
    void command(std::pair<std::string,isc::data::ElementPtr>);
    void serve(std::string zone_name);

    isc::data::ElementPtr updateConfig(isc::data::ElementPtr config);
private:

    isc::dns::DataSourceParkingLot data_source;
    int sock;
};

#endif // __PARKINGLOT_H

// Local Variables:
// mode: c++
// End:
