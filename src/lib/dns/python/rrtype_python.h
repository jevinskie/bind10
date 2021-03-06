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

#ifndef __PYTHON_RRTYPE_H
#define __PYTHON_RRTYPE_H 1

#include <Python.h>

namespace isc {
namespace dns {
class RRType;

namespace python {

extern PyObject* po_InvalidRRType;
extern PyObject* po_IncompleteRRType;

extern PyTypeObject rrtype_type;

/// This is a simple shortcut to create a python RRType object (in the
/// form of a pointer to PyObject) with minimal exception safety.
/// On success, it returns a valid pointer to PyObject with a reference
/// counter of 1; if something goes wrong it throws an exception (it never
/// returns a NULL pointer).
/// This function is expected to be called within a try block
/// followed by necessary setup for python exception.
PyObject* createRRTypeObject(const RRType& source);

/// \brief Checks if the given python object is a RRType object
///
/// \exception PyCPPWrapperException if obj is NULL
///
/// \param obj The object to check the type of
/// \return true if the object is of type RRType, false otherwise
bool PyRRType_Check(PyObject* obj);

/// \brief Returns a reference to the RRType object contained within the given
///        Python object.
///
/// \note The given object MUST be of type RRType; this can be checked with
///       either the right call to ParseTuple("O!"), or with PyRRType_Check()
///
/// \note This is not a copy; if the RRType is needed when the PyObject
/// may be destroyed, the caller must copy it itself.
///
/// \param rrtype_obj The rrtype object to convert
const RRType& PyRRType_ToRRType(const PyObject* rrtype_obj);


} // namespace python
} // namespace dns
} // namespace isc
#endif // __PYTHON_RRTYPE_H

// Local Variables:
// mode: c++
// End:
