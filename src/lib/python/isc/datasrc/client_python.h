// Copyright (C) 2011  Internet Systems Consortium, Inc. ("ISC")
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

#ifndef __PYTHON_DATASRC_CLIENT_H
#define __PYTHON_DATASRC_CLIENT_H 1

#include <datasrc/client_list.h>

#include <Python.h>

namespace isc {
namespace datasrc {
class DataSourceClient;

namespace python {

extern PyTypeObject datasourceclient_type;

/// \brief Create a DataSourceClient python object
///
/// Unlike many similar functions, this one does not create a copied instance
/// of the passed object. It wraps the given one. This is why the name is
/// different than the usual.
///
/// \param client The client to wrap.
/// \param life_keeper An optional object which keeps the client pointer valid.
///     The object will be kept inside the wrapper too, making sure that the
///     client is not destroyed sooner than the python object. The keeper thing
///     is designed to acommodate the interface of the ClientList.
PyObject*
wrapDataSourceClient(DataSourceClient* client,
                     const boost::shared_ptr<ClientList::FindResult::
                     LifeKeeper>& life_keeper = boost::shared_ptr<ClientList::
                     FindResult::LifeKeeper>());

} // namespace python
} // namespace datasrc
} // namespace isc
#endif // __PYTHON_DATASRC_CLIENT_H

// Local Variables:
// mode: c++
// End:
