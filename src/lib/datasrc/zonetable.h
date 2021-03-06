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

#ifndef __ZONETABLE_H
#define __ZONETABLE_H 1

#include <util/memory_segment.h>

#include <dns/rrset.h>

#include <datasrc/zone.h>

#include <boost/shared_ptr.hpp>

namespace isc {
namespace dns {
class Name;
class RRClass;
}

namespace datasrc {

/// \brief A set of authoritative zones.
///
/// \c ZoneTable class is primarily intended to be used as a backend for the
/// \c MemoryDataSrc class, but is exposed as a separate class in case some
/// application wants to use it directly (e.g. for a customized data source
/// implementation).
///
/// For more descriptions about its struct and interfaces, please refer to the
/// corresponding struct and interfaces of \c MemoryDataSrc.
class ZoneTable {
public:
    struct FindResult {
        FindResult(result::Result param_code, const ZoneFinderPtr param_zone) :
            code(param_code), zone(param_zone)
        {}
        const result::Result code;
        const ZoneFinderPtr zone;
    };
    ///
    /// \name Constructors and Destructor.
    ///
    /// \b Note:
    /// The copy constructor and the assignment operator are intentionally
    /// defined as private, making this class non copyable.
    //@{
private:
    ZoneTable(const ZoneTable& source);
    ZoneTable& operator=(const ZoneTable& source);

    /// Constructor.
    ///
    /// An object of this class is always expected to be created by the
    /// allocator (\c create()), so the constructor is hidden as private.
    ///
    /// This constructor internally involves resource allocation, and if
    /// it fails, a corresponding standard exception will be thrown.
    /// It never throws an exception otherwise.
    ZoneTable(util::MemorySegment& mem_sgmt);

    /// The destructor.
    ///
    /// An object of this class is always expected to be destroyed explicitly
    /// by \c destroy(), so the constructor is hidden as private.
    ~ZoneTable();
    //@}

public:
    /// \brief Allocate and construct \c ZoneTable
    ///
    /// This static method allocates memory for a new \c ZoneTable object
    /// from the given memory segment, constructs the object, and returns
    /// a pointer to it.
    ///
    /// \throw std::bad_alloc Memory allocation fails.
    ///
    /// \param mem_sgmt A \c MemorySegment from which memory for the new
    /// \c ZoneTable is allocated.
    static ZoneTable* create(util::MemorySegment& mem_sgmt);

    /// \brief Destruct and deallocate \c ZoneTable
    ///
    /// \throw none
    ///
    /// \param mem_sgmt The \c MemorySegment that allocated memory for
    /// \c ztable.
    /// \param ztable A non NULL pointer to a valid \c ZoneTable object
    /// that was originally created by the \c create() method (the behavior
    /// is undefined if this condition isn't met).
    static void destroy(util::MemorySegment& mem_sgmt, ZoneTable* ztable);

    /// Add a \c Zone to the \c ZoneTable.
    ///
    /// \c Zone must not be associated with a NULL pointer; otherwise
    /// an exception of class \c InvalidParameter will be thrown.
    /// If internal resource allocation fails, a corresponding standard
    /// exception will be thrown.
    /// This method never throws an exception otherwise.
    ///
    /// \param zone A \c Zone object to be added.
    /// \return \c result::SUCCESS If the zone is successfully
    /// added to the zone table.
    /// \return \c result::EXIST The zone table already contains
    /// zone of the same origin.
    result::Result addZone(util::MemorySegment& mem_sgmt, ZoneFinderPtr zone);

    /// Remove a \c Zone of the given origin name from the \c ZoneTable.
    ///
    /// This method never throws an exception.
    ///
    /// \param origin The origin name of the zone to be removed.
    /// \return \c result::SUCCESS If the zone is successfully
    /// removed from the zone table.
    /// \return \c result::NOTFOUND The zone table does not
    /// store the zone that matches \c origin.
    result::Result removeZone(const isc::dns::Name& origin);

    /// Find a \c Zone that best matches the given name in the \c ZoneTable.
    ///
    /// It searches the internal storage for a \c Zone that gives the
    /// longest match against \c name, and returns the result in the
    /// form of a \c FindResult object as follows:
    /// - \c code: The result code of the operation.
    ///   - \c result::SUCCESS: A zone that gives an exact match
    ///    is found
    ///   - \c result::PARTIALMATCH: A zone whose origin is a
    ///    super domain of \c name is found (but there is no exact match)
    ///   - \c result::NOTFOUND: For all other cases.
    /// - \c zone: A "Boost" shared pointer to the found \c Zone object if one
    ///  is found; otherwise \c NULL.
    ///
    /// This method never throws an exception.
    ///
    /// \param name A domain name for which the search is performed.
    /// \return A \c FindResult object enclosing the search result (see above).
    FindResult findZone(const isc::dns::Name& name) const;

private:
    struct ZoneTableImpl;
    ZoneTableImpl* impl_;
};
}
}
#endif  // __ZONETABLE_H

// Local Variables:
// mode: c++
// End:
