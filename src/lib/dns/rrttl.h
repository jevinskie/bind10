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

#ifndef __RRTTL_H
#define __RRTTL_H 1

#include <stdint.h>

#include <exceptions/exceptions.h>

namespace isc {
namespace util {
class InputBuffer;
class OutputBuffer;
}

namespace dns {

// forward declarations
class AbstractMessageRenderer;

///
/// \brief A standard DNS module exception that is thrown if an RRTTL object
/// is being constructed from an unrecognized string.
///
class InvalidRRTTL : public Exception {
public:
    InvalidRRTTL(const char* file, size_t line, const char* what) :
        isc::Exception(file, line, what) {}
};

///
/// \brief A standard DNS module exception that is thrown if an RRTTL object
/// is being constructed from a incomplete (too short) wire-format data.
///
class IncompleteRRTTL : public Exception {
public:
    IncompleteRRTTL(const char* file, size_t line, const char* what) :
        isc::Exception(file, line, what) {}
};

///
/// The \c RRTTL class encapsulates TTLs used in DNS resource records.
///
/// This is a straightforward class; an \c RRTTL object simply maintains a
/// 32-bit unsigned integer corresponding to the TTL value.  The main purpose
/// of this class is to provide convenient interfaces to convert a textual
/// representation into the integer TTL value and vice versa, and to handle
/// wire-format representations.
class RRTTL {
public:
    ///
    /// \name Constructors and Destructor
    ///
    /// Note: We use the default copy constructor and the default copy
    /// assignment operator intentionally.
    //@{
    /// Constructor from an integer TTL value.
    ///
    /// This constructor never throws an exception.
    ///
    /// \param ttlval An 32-bit integer of the RRTTL.
    explicit RRTTL(uint32_t ttlval) : ttlval_(ttlval) {}
    /// Constructor from a string.
    ///
    /// This version of the implementation only accepts decimal TTL values in
    /// seconds.
    /// In a near future version, we'll extend it so that we can accept more
    /// convenient ones such as "2H" or "1D".
    ///
    /// If the given string is not recognized as a valid representation of
    /// an RR TTL, an exception of class \c InvalidRRTTL will be thrown.
    ///
    /// \param ttlstr A string representation of the \c RRTTL
    explicit RRTTL(const std::string& ttlstr);
    /// Constructor from wire-format data.
    ///
    /// The \c buffer parameter normally stores a complete DNS message
    /// containing the RRTTL to be constructed.  The current read position of
    /// the buffer points to the head of the type.
    ///
    /// If the given data does not large enough to contain a 16-bit integer,
    /// an exception of class \c IncompleteRRTTL will be thrown.
    ///
    /// \param buffer A buffer storing the wire format data.
    explicit RRTTL(isc::util::InputBuffer& buffer);
    ///
    //@}

    ///
    /// \name Converter methods
    ///
    //@{
    /// \brief Convert the \c RRTTL to a string.
    ///
    /// This version of implementation simply converts the TTL value into the
    /// numeric textual representation.  We may introduce more human-readable
    /// format depending on the context in future versions.
    ///
    /// If resource allocation in rendering process fails, a corresponding
    /// standard exception will be thrown.
    ///
    /// \return A string representation of the \c RRTTL.
    const std::string toText() const;
    /// \brief Render the \c RRTTL in the wire format.
    ///
    /// This method renders the TTL value in network byte order via \c renderer,
    /// which encapsulates output buffer and other rendering contexts.
    ///
    /// If resource allocation in rendering process fails, a corresponding
    /// standard exception will be thrown.
    ///
    /// \param renderer DNS message rendering context that encapsulates the
    /// output buffer in which the RRTTL is to be stored.
    void toWire(AbstractMessageRenderer& renderer) const;
    /// \brief Render the \c RRTTL in the wire format.
    ///
    /// This method renders the TTL value in network byte order into the
    /// \c buffer.
    ///
    /// If resource allocation in rendering process fails, a corresponding
    /// standard exception will be thrown.
    ///
    /// \param buffer An output buffer to store the wire data.
    void toWire(isc::util::OutputBuffer& buffer) const;
    //@}

    ///
    /// \name Getter Methods
    ///
    //@{
    /// \brief Returns the TTL value as a 32-bit unsigned integer.
    ///
    /// This method never throws an exception.
    ///
    /// \return An 32-bit integer corresponding to the RRTTL.
    uint32_t getValue() const { return (ttlval_); }
    //@}

    ///
    /// \name Comparison methods
    ///
    /// Comparison between two \c RRTTL objects is performed in a
    /// straightforward way, that is, comparing the corresponding TTL values
    /// (which is the result of the \c getValue() method) as 32-bit unsigned
    /// integers.
    //@{
    /// \brief Return true iff two RRTTLs are equal.
    ///
    /// This method never throws an exception.
    ///
    /// \param other the \c RRTTL object to compare against.
    bool equals(const RRTTL& other) const
    { return (ttlval_ == other.ttlval_); }
    /// \brief Same as \c equals().
    bool operator==(const RRTTL& other) const
    { return (ttlval_ == other.ttlval_); }
    /// \brief Return true iff two RRTTLs are not equal.
    ///
    /// This method never throws an exception.
    ///
    /// \param other the \c RRTTL object to compare against.
    bool nequals(const RRTTL& other) const
    { return (ttlval_ != other.ttlval_); }
    /// \brief Same as \c nequals().
    bool operator!=(const RRTTL& other) const
    { return (ttlval_ != other.ttlval_); }
    /// \brief Less-than or equal comparison for RRTTL against \c other.
    ///
    /// This method never throws an exception.
    ///
    /// \param other the \c RRTTL object to compare against.
    /// \return true if \c this RRTTL is less than or equal to the \c other;
    /// otherwise false.
    bool leq(const RRTTL& other) const
    { return (ttlval_ <= other.ttlval_); }

    /// Same as \c leq()
    bool operator<=(const RRTTL& other) const
    { return (ttlval_ <= other.ttlval_); }

    /// \brief Greater-than or equal comparison for RRTTL against \c other.
    ///
    /// This method never throws an exception.
    ///
    /// \param other the \c RRTTL object to compare against.
    /// \return true if \c this RRTTL is greater than or equal to the \c other;
    /// otherwise false.
    bool geq(const RRTTL& other) const
    { return (ttlval_ >= other.ttlval_); }

    /// Same as \c geq()
    bool operator>=(const RRTTL& other) const
    { return (ttlval_ >= other.ttlval_); }

    /// \brief Less-than comparison for RRTTL against \c other.
    ///
    /// This method never throws an exception.
    ///
    /// \param other the \c RRTTL object to compare against.
    /// \return true if \c this RRTTL is less than the \c other;
    /// otherwise false.
    bool lthan(const RRTTL& other) const
    { return (ttlval_ < other.ttlval_); }

    /// Same as \c lthan()
    bool operator<(const RRTTL& other) const
    { return (ttlval_ < other.ttlval_); }

    /// \brief Greater-than comparison for RRTTL against \c other.
    ///
    /// This method never throws an exception.
    ///
    /// \param other the \c RRTTL object to compare against.
    /// \return true if \c this RRTTL is greater than the \c other;
    /// otherwise false.
    bool gthan(const RRTTL& other) const
    { return (ttlval_ > other.ttlval_); }

    /// Same as \c gthan()
    bool operator>(const RRTTL& other) const
    { return (ttlval_ > other.ttlval_); }
    //@}

private:
    uint32_t ttlval_;
};

///
/// \brief Insert the \c RRTTL as a string into stream.
///
/// This method convert the \c rrttl into a string and inserts it into the
/// output stream \c os.
///
/// This function overloads the global operator<< to behave as described in
/// ostream::operator<< but applied to \c RRTTL objects.
///
/// \param os A \c std::ostream object on which the insertion operation is
/// performed.
/// \param rrttl The \c RRTTL object output by the operation.
/// \return A reference to the same \c std::ostream object referenced by
/// parameter \c os after the insertion operation.
std::ostream&
operator<<(std::ostream& os, const RRTTL& rrttl);
}
}
#endif  // __RRTTL_H

// Local Variables: 
// mode: c++
// End: 
