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

#include <exceptions/exceptions.h>

#include <datasrc/memory/memory_client.h>
#include <datasrc/memory/logger.h>
#include <datasrc/memory/zone_data.h>
#include <datasrc/memory/rdata_serialization.h>
#include <datasrc/memory/rdataset.h>
#include <datasrc/memory/domaintree.h>
#include <datasrc/memory/segment_object_holder.h>
#include <datasrc/memory/treenode_rrset.h>
#include <datasrc/memory/zone_finder.h>

#include <util/memory_segment_local.h>

#include <datasrc/data_source.h>
#include <datasrc/factory.h>
#include <datasrc/result.h>

#include <dns/name.h>
#include <dns/nsec3hash.h>
#include <dns/rdataclass.h>
#include <dns/rrclass.h>
#include <dns/rrsetlist.h>
#include <dns/masterload.h>

#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/noncopyable.hpp>

#include <algorithm>
#include <map>
#include <utility>
#include <cctype>
#include <cassert>

using namespace std;
using namespace isc::dns;
using namespace isc::dns::rdata;
using namespace isc::datasrc::memory;
using boost::scoped_ptr;

namespace isc {
namespace datasrc {
namespace memory {

using detail::SegmentObjectHolder;

namespace {
// Some type aliases
typedef DomainTree<std::string> FileNameTree;
typedef DomainTreeNode<std::string> FileNameNode;

// A functor type used for loading.
typedef boost::function<void(ConstRRsetPtr)> LoadCallback;

} // end of anonymous namespace

/// Implementation details for \c InMemoryClient hidden from the public
/// interface.
///
/// For now, \c InMemoryClient only contains a \c ZoneTable object, which
/// consists of (pointers to) \c InMemoryZoneFinder objects, we may add more
/// member variables later for new features.
class InMemoryClient::InMemoryClientImpl {
private:
    // The deleter for the filenames stored in the tree.
    struct FileNameDeleter {
        FileNameDeleter() {}
        void operator()(std::string* filename) const {
            delete filename;
        }
    };

public:
    InMemoryClientImpl(util::MemorySegment& mem_sgmt, RRClass rrclass) :
        mem_sgmt_(mem_sgmt),
        rrclass_(rrclass),
        zone_count_(0),
        zone_table_(ZoneTable::create(mem_sgmt_, rrclass)),
        file_name_tree_(FileNameTree::create(mem_sgmt_, false))
    {}
    ~InMemoryClientImpl() {
        FileNameDeleter deleter;
        FileNameTree::destroy(mem_sgmt_, file_name_tree_, deleter);

        ZoneTable::destroy(mem_sgmt_, zone_table_, rrclass_);
    }

    util::MemorySegment& mem_sgmt_;
    const RRClass rrclass_;
    unsigned int zone_count_;
    ZoneTable* zone_table_;
    FileNameTree* file_name_tree_;

    // Common process for zone load.
    // rrset_installer is a functor that takes another functor as an argument,
    // and expected to call the latter for each RRset of the zone.  How the
    // sequence of the RRsets is generated depends on the internal
    // details  of the loader: either from a textual master file or from
    // another data source.
    // filename is the file name of the master file or empty if the zone is
    // loaded from another data source.
    result::Result load(const Name& zone_name, const string& filename,
                        boost::function<void(LoadCallback)> rrset_installer);

    // Add the necessary magic for any wildcard contained in 'name'
    // (including itself) to be found in the zone.
    //
    // In order for wildcard matching to work correctly in the zone finder,
    // we must ensure that a node for the wildcarding level exists in the
    // backend ZoneTree.
    // E.g. if the wildcard name is "*.sub.example." then we must ensure
    // that "sub.example." exists and is marked as a wildcard level.
    // Note: the "wildcarding level" is for the parent name of the wildcard
    // name (such as "sub.example.").
    //
    // We also perform the same trick for empty wild card names possibly
    // contained in 'name' (e.g., '*.foo.example' in 'bar.*.foo.example').
    void addWildcards(const Name& zone_name, ZoneData& zone_data,
                      const Name& name)
    {
        Name wname(name);
        const unsigned int labels(wname.getLabelCount());
        const unsigned int origin_labels(zone_name.getLabelCount());
        for (unsigned int l = labels;
             l > origin_labels;
             --l, wname = wname.split(1)) {
            if (wname.isWildcard()) {
                LOG_DEBUG(logger, DBG_TRACE_DATA,
                          DATASRC_MEMORY_MEM_ADD_WILDCARD).arg(name);

                // Ensure a separate level exists for the "wildcarding" name,
                // and mark the node as "wild".
                ZoneNode* node;
                zone_data.insertName(mem_sgmt_, wname.split(1), &node);
                node->setFlag(ZoneData::WILDCARD_NODE);

                // Ensure a separate level exists for the wildcard name.
                // Note: for 'name' itself we do this later anyway, but the
                // overhead should be marginal because wildcard names should
                // be rare.
                zone_data.insertName(mem_sgmt_, wname, &node);
            }
        }
    }

    /*
     * Does some checks in context of the data that are already in the zone.
     * Currently checks for forbidden combinations of RRsets in the same
     * domain (CNAME+anything, DNAME+NS).
     *
     * If such condition is found, it throws AddError.
     */
    void contextCheck(const Name& zone_name, const AbstractRRset& rrset,
                      const RdataSet* set) const
    {
        // Ensure CNAME and other type of RR don't coexist for the same
        // owner name except with NSEC, which is the only RR that can coexist
        // with CNAME (and also RRSIG, which is handled separately)
        if (rrset.getType() == RRType::CNAME()) {
            for (const RdataSet* sp = set; sp != NULL; sp = sp->getNext()) {
                if (sp->type != RRType::NSEC()) {
                    LOG_ERROR(logger, DATASRC_MEMORY_MEM_CNAME_TO_NONEMPTY).
                        arg(rrset.getName());
                    isc_throw(AddError, "CNAME can't be added with "
                              << sp->type << " RRType for "
                              << rrset.getName());
                }
            }
        } else if ((rrset.getType() != RRType::NSEC()) &&
                   (RdataSet::find(set, RRType::CNAME()) != NULL)) {
            LOG_ERROR(logger,
                      DATASRC_MEMORY_MEM_CNAME_COEXIST).arg(rrset.getName());
            isc_throw(AddError, "CNAME and " << rrset.getType() <<
                      " can't coexist for " << rrset.getName());
        }

        /*
         * Similar with DNAME, but it must not coexist only with NS and only in
         * non-apex domains.
         * RFC 2672 section 3 mentions that it is implied from it and RFC 2181
         */
        if (rrset.getName() != zone_name &&
            // Adding DNAME, NS already there
            ((rrset.getType() == RRType::DNAME() &&
              RdataSet::find(set, RRType::NS()) != NULL) ||
            // Adding NS, DNAME already there
            (rrset.getType() == RRType::NS() &&
             RdataSet::find(set, RRType::DNAME()) != NULL)))
        {
            LOG_ERROR(logger, DATASRC_MEMORY_MEM_DNAME_NS).arg(rrset.getName());
            isc_throw(AddError, "DNAME can't coexist with NS in non-apex "
                "domain " << rrset.getName());
        }
    }

    // Validate rrset before adding it to the zone.  If something is wrong
    // it throws an exception.  It doesn't modify the zone, and provides
    // the strong exception guarantee.
    void addValidation(const Name& zone_name, const ConstRRsetPtr rrset) {
        if (!rrset) {
            isc_throw(NullRRset, "The rrset provided is NULL");
        }
        if (rrset->getRdataCount() == 0) {
            isc_throw(AddError, "The rrset provided is empty: " <<
                      rrset->getName() << "/" << rrset->getType());
        }
        // Check for singleton RRs. It should probably handled at a different
        // layer in future.
        if ((rrset->getType() == RRType::CNAME() ||
            rrset->getType() == RRType::DNAME()) &&
            rrset->getRdataCount() > 1)
        {
            // XXX: this is not only for CNAME or DNAME. We should generalize
            // this code for all other "singleton RR types" (such as SOA) in a
            // separate task.
            LOG_ERROR(logger,
                      DATASRC_MEMORY_MEM_SINGLETON).arg(rrset->getName()).
                arg(rrset->getType());
            isc_throw(AddError, "multiple RRs of singleton type for "
                      << rrset->getName());
        }
        // NSEC3/NSEC3PARAM is not a "singleton" per protocol, but this
        // implementation requests it be so at the moment.
        if ((rrset->getType() == RRType::NSEC3() ||
             rrset->getType() == RRType::NSEC3PARAM()) &&
            rrset->getRdataCount() > 1) {
            isc_throw(AddError, "Multiple NSEC3/NSEC3PARAM RDATA is given for "
                      << rrset->getName() << " which isn't supported");
        }

        // For RRSIGs, check consistency of the type covered.
        // We know the RRset isn't empty, so the following check is safe.
        if (rrset->getType() == RRType::RRSIG()) {
            RdataIteratorPtr rit = rrset->getRdataIterator();
            const RRType covered = dynamic_cast<const generic::RRSIG&>(
                rit->getCurrent()).typeCovered();
            for (rit->next(); !rit->isLast(); rit->next()) {
                if (dynamic_cast<const generic::RRSIG&>(
                        rit->getCurrent()).typeCovered() != covered) {
                    isc_throw(AddError, "RRSIG contains mixed covered types: "
                              << rrset->toText());
                }
            }
        }

        const NameComparisonResult compare =
            zone_name.compare(rrset->getName());
        if (compare.getRelation() != NameComparisonResult::SUPERDOMAIN &&
            compare.getRelation() != NameComparisonResult::EQUAL)
        {
            LOG_ERROR(logger,
                      DATASRC_MEMORY_MEM_OUT_OF_ZONE).arg(rrset->getName()).
                arg(zone_name);
            isc_throw(OutOfZone, "The name " << rrset->getName() <<
                " is not contained in zone " << zone_name);
        }

        // Some RR types do not really work well with a wildcard.
        // Even though the protocol specifically doesn't completely ban such
        // usage, we refuse to load a zone containing such RR in order to
        // keep the lookup logic simpler and more predictable.
        // See RFC4592 and (for DNAME) RFC6672 for more technical background.
        // Note also that BIND 9 refuses NS at a wildcard, so in that sense
        // we simply provide compatible behavior.
        if (rrset->getName().isWildcard()) {
            if (rrset->getType() == RRType::NS()) {
                LOG_ERROR(logger, DATASRC_MEMORY_MEM_WILDCARD_NS).
                    arg(rrset->getName());
                isc_throw(AddError, "Invalid NS owner name (wildcard): " <<
                          rrset->getName());
            }
            if (rrset->getType() == RRType::DNAME()) {
                LOG_ERROR(logger, DATASRC_MEMORY_MEM_WILDCARD_DNAME).
                    arg(rrset->getName());
                isc_throw(AddError, "Invalid DNAME owner name (wildcard): " <<
                          rrset->getName());
            }
        }

        // Owner names of NSEC3 have special format as defined in RFC5155,
        // and cannot be a wildcard name or must be one label longer than
        // the zone origin.  While the RFC doesn't prohibit other forms of
        // names, no sane zone would have such names for NSEC3.
        // BIND 9 also refuses NSEC3 at wildcard.
        if (rrset->getType() == RRType::NSEC3() &&
            (rrset->getName().isWildcard() ||
             rrset->getName().getLabelCount() !=
             zone_name.getLabelCount() + 1)) {
            LOG_ERROR(logger, DATASRC_MEMORY_BAD_NSEC3_NAME).
                arg(rrset->getName());
            isc_throw(AddError, "Invalid NSEC3 owner name: " <<
                      rrset->getName());
        }
    }

    void addNSEC3(const ConstRRsetPtr rrset,
                  const ConstRRsetPtr rrsig,
                  ZoneData& zone_data)
    {
        // We know rrset has exactly one RDATA
        const generic::NSEC3& nsec3_rdata =
            dynamic_cast<const generic::NSEC3&>(
                rrset->getRdataIterator()->getCurrent());

        NSEC3Data* nsec3_data = zone_data.getNSEC3Data();
        if (nsec3_data == NULL) {
            nsec3_data = NSEC3Data::create(mem_sgmt_, nsec3_rdata);
            zone_data.setNSEC3Data(nsec3_data);
            zone_data.setSigned(true);
        } else {
            size_t salt_len = nsec3_data->getSaltLen();
            const uint8_t* salt_data = nsec3_data->getSaltData();
            const vector<uint8_t>& salt_data_2 = nsec3_rdata.getSalt();

            if ((nsec3_rdata.getHashalg() != nsec3_data->hashalg) ||
                (nsec3_rdata.getIterations() != nsec3_data->iterations) ||
                (salt_data_2.size() != salt_len)) {
                isc_throw(AddError,
                          "NSEC3 with inconsistent parameters: " <<
                          rrset->toText());
            }
            if ((salt_len > 0) &&
                (std::memcmp(&salt_data_2[0], salt_data, salt_len) != 0)) {
                isc_throw(AddError,
                          "NSEC3 with inconsistent parameters: " <<
                          rrset->toText());
            }
        }

        ZoneNode* node;
        nsec3_data->insertName(mem_sgmt_, rrset->getName(), &node);

        RdataEncoder encoder;
        RdataSet* set = RdataSet::create(mem_sgmt_, encoder, rrset, rrsig);
        RdataSet* old_set = node->setData(set);
        if (old_set != NULL) {
            RdataSet::destroy(mem_sgmt_, rrclass_, old_set);
        }
    }

    void addRdataSet(const Name& zone_name, ZoneData& zone_data,
                     const ConstRRsetPtr rrset, const ConstRRsetPtr rrsig)
    {
        if (rrset->getType() == RRType::NSEC3()) {
            addNSEC3(rrset, rrsig, zone_data);
        } else {
            ZoneNode* node;
            zone_data.insertName(mem_sgmt_, rrset->getName(), &node);

            RdataSet* rdataset_head = node->getData();

            // Checks related to the surrounding data.
            // Note: when the check fails and the exception is thrown,
            // it may break strong exception guarantee.  At the moment
            // we prefer code simplicity and don't bother to introduce
            // complicated recovery code.
            contextCheck(zone_name, *rrset, rdataset_head);

            if (RdataSet::find(rdataset_head, rrset->getType()) != NULL) {
                isc_throw(AddError,
                          "RRset of the type already exists: "
                          << rrset->getName() << " (type: "
                          << rrset->getType() << ")");
            }

            RdataEncoder encoder;
            RdataSet* rdataset = RdataSet::create(mem_sgmt_, encoder, rrset,
                                                  rrsig);
            rdataset->next = rdataset_head;
            node->setData(rdataset);

            // Ok, we just put it in

            // If this RRset creates a zone cut at this node, mark the
            // node indicating the need for callback in find().
            if (rrset->getType() == RRType::NS() &&
                rrset->getName() != zone_name) {
                node->setFlag(ZoneNode::FLAG_CALLBACK);
                // If it is DNAME, we have a callback as well here
            } else if (rrset->getType() == RRType::DNAME()) {
                node->setFlag(ZoneNode::FLAG_CALLBACK);
            }

            // If we've added NSEC3PARAM at zone origin, set up NSEC3
            // specific data or check consistency with already set up
            // parameters.
            if (rrset->getType() == RRType::NSEC3PARAM() &&
                rrset->getName() == zone_name) {
                // We know rrset has exactly one RDATA
                const generic::NSEC3PARAM& param =
                    dynamic_cast<const generic::NSEC3PARAM&>
                      (rrset->getRdataIterator()->getCurrent());

                NSEC3Data* nsec3_data = zone_data.getNSEC3Data();
                if (nsec3_data == NULL) {
                    nsec3_data = NSEC3Data::create(mem_sgmt_, param);
                    zone_data.setNSEC3Data(nsec3_data);
                    zone_data.setSigned(true);
                } else {
                    size_t salt_len = nsec3_data->getSaltLen();
                    const uint8_t* salt_data = nsec3_data->getSaltData();
                    const vector<uint8_t>& salt_data_2 = param.getSalt();

                    if ((param.getHashalg() != nsec3_data->hashalg) ||
                        (param.getIterations() != nsec3_data->iterations) ||
                        (salt_data_2.size() != salt_len)) {
                        isc_throw(AddError,
                                  "NSEC3PARAM with inconsistent parameters: "
                                  << rrset->toText());
                    }

                    if ((salt_len > 0) &&
                        (std::memcmp(&salt_data_2[0],
                                     salt_data, salt_len) != 0)) {
                        isc_throw(AddError,
                                  "NSEC3PARAM with inconsistent parameters: "
                                  << rrset->toText());
                    }
                }
            } else if (rrset->getType() == RRType::NSEC()) {
                // If it is NSEC signed zone, we mark the zone as signed
                // (conceptually "signed" is a broader notion but our current
                // zone finder implementation regards "signed" as "NSEC
                // signed")
                zone_data.setSigned(true);
            }
        }
    }

    // Implementation of InMemoryClient::add()
    void add(const ConstRRsetPtr& rrset, const ConstRRsetPtr& sig_rrset,
             const Name& zone_name, ZoneData& zone_data)
    {
        // Sanitize input.  This will cause an exception to be thrown
        // if the input RRset is empty.
        addValidation(zone_name, rrset);
        if (sig_rrset) {
            addValidation(zone_name, sig_rrset);
        }

        // OK, can add the RRset.
        LOG_DEBUG(logger, DBG_TRACE_DATA, DATASRC_MEMORY_MEM_ADD_RRSET).
            arg(rrset->getName()).arg(rrset->getType()).arg(zone_name);

        // Add wildcards possibly contained in the owner name to the domain
        // tree.  This can only happen for the normal (non-NSEC3) tree.
        // Note: this can throw an exception, breaking strong exception
        // guarantee.  (see also the note for the call to contextCheck()
        // above).
        if (rrset->getType() != RRType::NSEC3()) {
            addWildcards(zone_name, zone_data, rrset->getName());
        }

        addRdataSet(zone_name, zone_data, rrset, sig_rrset);
    }
};

// A helper internal class for load().  make it non-copyable to avoid
// accidental copy.
//
// The current internal implementation expects that both a normal
// (non RRSIG) RRset and (when signed) its RRSIG are added at once.
// Also in the current implementation, the input sequence of RRsets
// are grouped with their owner name (so once a new owner name is encountered,
// no subsequent RRset has the previous owner name), but the ordering
// in the same group is not fixed.  So we hold all RRsets of the same
// owner name in node_rrsets_ and node_rrsigsets_, and add the matching
// pairs of RRsets to the zone when we see a new owner name.
//
// The caller is responsible for adding the RRsets of the last group
// in the input sequence by explicitly calling flushNodeRRsets() at the
// end.  It's cleaner and more robust if we let the destructor of this class
// do it, but since we cannot guarantee the adding operation is exception free,
// we don't choose that option to maintain the common expectation for
// destructors.
class InMemoryClient::Loader : boost::noncopyable {
    typedef std::map<RRType, ConstRRsetPtr> NodeRRsets;
    typedef NodeRRsets::value_type NodeRRsetsVal;
public:
    Loader(InMemoryClientImpl* client_impl, const Name& zone_name,
           ZoneData& zone_data) :
        client_impl_(client_impl), zone_name_(zone_name), zone_data_(zone_data)
    {}
    void addFromLoad(const ConstRRsetPtr& rrset) {
        // If we see a new name, flush the temporary holders, adding the
        // pairs of RRsets and RRSIGs of the previous name to the zone.
        if ((!node_rrsets_.empty() || !node_rrsigsets_.empty()) &&
            getCurrentName() != rrset->getName()) {
            flushNodeRRsets();
        }

        // Store this RRset until it can be added to the zone.  The current
        // implementation requires RRs of the same RRset should be added at
        // once, so we check the "duplicate" here.
        const bool is_rrsig = rrset->getType() == RRType::RRSIG();
        NodeRRsets& node_rrsets = is_rrsig ? node_rrsigsets_ : node_rrsets_;
        const RRType& rrtype = is_rrsig ?
            getCoveredType(rrset) : rrset->getType();
        if (!node_rrsets.insert(NodeRRsetsVal(rrtype, rrset)).second) {
            isc_throw(AddError,
                      "Duplicate add of the same type of"
                      << (is_rrsig ? " RRSIG" : "") << " RRset: "
                      << rrset->getName() << "/" << rrtype);
        }
    }
    void flushNodeRRsets() {
        BOOST_FOREACH(NodeRRsetsVal val, node_rrsets_) {
            // Identify the corresponding RRSIG for the RRset, if any.
            // If found add both the RRset and its RRSIG at once.
            ConstRRsetPtr sig_rrset;
            NodeRRsets::iterator sig_it =
                node_rrsigsets_.find(val.first);
            if (sig_it != node_rrsigsets_.end()) {
                sig_rrset = sig_it->second;
                node_rrsigsets_.erase(sig_it);
            }
            client_impl_->add(val.second, sig_rrset, zone_name_, zone_data_);
        }

        // Right now, we don't accept RRSIG without covered RRsets (this
        // should eventually allowed, but to do so we'll need to update the
        // finder).
        if (!node_rrsigsets_.empty()) {
            isc_throw(AddError, "RRSIG is added without covered RRset for "
                      << getCurrentName());
        }

        node_rrsets_.clear();
        node_rrsigsets_.clear();
    }
private:
    // A helper to identify the covered type of an RRSIG.
    static RRType getCoveredType(const ConstRRsetPtr& sig_rrset) {
        RdataIteratorPtr it = sig_rrset->getRdataIterator();
        // Empty RRSIG shouldn't be passed either via a master file or another
        // data source iterator, but it could still happen if the iterator
        // has a bug.  We catch and reject such cases.
        if (it->isLast()) {
            isc_throw(isc::Unexpected,
                      "Empty RRset is passed in-memory loader, name: "
                      << sig_rrset->getName());
        }
        return (dynamic_cast<const generic::RRSIG&>(it->getCurrent()).
                typeCovered());
    }
    const Name& getCurrentName() const {
        if (!node_rrsets_.empty()) {
            return (node_rrsets_.begin()->second->getName());
        }
        assert(!node_rrsigsets_.empty());
        return (node_rrsigsets_.begin()->second->getName());
    }

private:
    InMemoryClientImpl* client_impl_;
    const Name& zone_name_;
    ZoneData& zone_data_;
    NodeRRsets node_rrsets_;
    NodeRRsets node_rrsigsets_;
};

result::Result
InMemoryClient::InMemoryClientImpl::load(
    const Name& zone_name,
    const string& filename,
    boost::function<void(LoadCallback)> rrset_installer)
{
    SegmentObjectHolder<ZoneData, RRClass> holder(
        mem_sgmt_, ZoneData::create(mem_sgmt_, zone_name), rrclass_);

    Loader loader(this, zone_name, *holder.get());
    rrset_installer(boost::bind(&Loader::addFromLoad, &loader, _1));
    // Add any last RRsets that were left
    loader.flushNodeRRsets();

    const ZoneNode* origin_node = holder.get()->getOriginNode();
    const RdataSet* set = origin_node->getData();
    // If the zone is NSEC3-signed, check if it has NSEC3PARAM
    if (holder.get()->isNSEC3Signed()) {
        if (RdataSet::find(set, RRType::NSEC3PARAM()) == NULL) {
            LOG_WARN(logger, DATASRC_MEMORY_MEM_NO_NSEC3PARAM).
                arg(zone_name).arg(rrclass_);
        }
    }

    // When an empty zone file is loaded, the origin doesn't even have
    // an SOA RR. This condition should be avoided, and hence load()
    // should throw when an empty zone is loaded.
    if (RdataSet::find(set, RRType::SOA()) == NULL) {
        isc_throw(EmptyZone,
                  "Won't create an empty zone for: " << zone_name);
    }

    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_MEMORY_MEM_ADD_ZONE).
        arg(zone_name).arg(rrclass_);

    // Set the filename in file_name_tree_ now, so that getFileName()
    // can use it (during zone reloading).
    FileNameNode* node(NULL);
    switch (file_name_tree_->insert(mem_sgmt_, zone_name, &node)) {
    case FileNameTree::SUCCESS:
    case FileNameTree::ALREADYEXISTS:
        // These are OK
        break;
    default:
        // Can Not Happen
        assert(false);
    }
    // node must point to a valid node now
    assert(node != NULL);

    std::string* tstr = node->setData(new std::string(filename));
    delete tstr;

    ZoneTable::AddResult result(zone_table_->addZone(mem_sgmt_, rrclass_,
                                                     zone_name));
    if (result.code == result::SUCCESS) {
        // Only increment the zone count if the zone doesn't already
        // exist.
        ++zone_count_;
    }

    ZoneTable::FindResult fr(zone_table_->setZoneData(zone_name,
                                                      holder.release()));
    assert(fr.code == result::SUCCESS);
    if (fr.zone_data != NULL) {
        ZoneData::destroy(mem_sgmt_, fr.zone_data, rrclass_);
    }

    return (result.code);
}

namespace {
// A wrapper for dns::masterLoad used by load() below.  Essentially it
// converts the two callback types.  Note the mostly redundant wrapper of
// boost::bind.  It converts function<void(ConstRRsetPtr)> to
// function<void(RRsetPtr)> (masterLoad() expects the latter).  SunStudio
// doesn't seem to do this conversion if we just pass 'callback'.
void
masterLoadWrapper(const char* const filename, const Name& origin,
                  const RRClass& zone_class, LoadCallback callback)
{
    masterLoad(filename, origin, zone_class, boost::bind(callback, _1));
}

// The installer called from Impl::load() for the iterator version of load().
void
generateRRsetFromIterator(ZoneIterator* iterator, LoadCallback callback) {
    ConstRRsetPtr rrset;
    while ((rrset = iterator->getNextRRset()) != NULL) {
        callback(rrset);
    }
}
}

InMemoryClient::InMemoryClient(util::MemorySegment& mem_sgmt,
                               RRClass rrclass) :
    impl_(new InMemoryClientImpl(mem_sgmt, rrclass))
{}

InMemoryClient::~InMemoryClient() {
    delete impl_;
}

RRClass
InMemoryClient::getClass() const {
    return (impl_->rrclass_);
}

unsigned int
InMemoryClient::getZoneCount() const {
    return (impl_->zone_count_);
}

isc::datasrc::DataSourceClient::FindResult
InMemoryClient::findZone(const isc::dns::Name& zone_name) const {
    LOG_DEBUG(logger, DBG_TRACE_DATA,
              DATASRC_MEMORY_MEM_FIND_ZONE).arg(zone_name);

    ZoneTable::FindResult result(impl_->zone_table_->findZone(zone_name));

    ZoneFinderPtr finder;
    if (result.code != result::NOTFOUND) {
        finder.reset(new InMemoryZoneFinder(*result.zone_data, getClass()));
    }

    return (DataSourceClient::FindResult(result.code, finder));
}

const ZoneData*
InMemoryClient::findZoneData(const isc::dns::Name& zone_name) {
    ZoneTable::FindResult result(impl_->zone_table_->findZone(zone_name));
    return (result.zone_data);
}

result::Result
InMemoryClient::load(const isc::dns::Name& zone_name,
                     const std::string& filename) {
    LOG_DEBUG(logger, DBG_TRACE_BASIC, DATASRC_MEMORY_MEM_LOAD).arg(zone_name).
        arg(filename);

    return (impl_->load(zone_name, filename,
                        boost::bind(masterLoadWrapper, filename.c_str(),
                                    zone_name, getClass(), _1)));
}

result::Result
InMemoryClient::load(const isc::dns::Name& zone_name,
                     ZoneIterator& iterator) {
    return (impl_->load(zone_name, string(),
                        boost::bind(generateRRsetFromIterator,
                                    &iterator, _1)));
}

const std::string
InMemoryClient::getFileName(const isc::dns::Name& zone_name) const {
    FileNameNode* node(NULL);
    FileNameTree::Result result = impl_->file_name_tree_->find(zone_name,
                                                               &node);
    if (result == FileNameTree::EXACTMATCH) {
        return (*node->getData());
    } else {
        return (std::string());
    }
}

result::Result
InMemoryClient::add(const isc::dns::Name& zone_name,
                    const ConstRRsetPtr& rrset)
{
    const ZoneTable::FindResult result =
        impl_->zone_table_->findZone(zone_name);
    if (result.code != result::SUCCESS) {
        isc_throw(DataSourceError, "No such zone: " + zone_name.toText());
    }

    const ConstRRsetPtr sig_rrset =
        rrset ? rrset->getRRsig() : ConstRRsetPtr();
    impl_->add(rrset, sig_rrset, zone_name, *result.zone_data);

    // add() doesn't allow duplicate add, so we always return SUCCESS.
    return (result::SUCCESS);
}

namespace {

class MemoryIterator : public ZoneIterator {
private:
    ZoneChain chain_;
    const RdataSet* set_node_;
    const RRClass rrclass_;
    const ZoneTree& tree_;
    const ZoneNode* node_;
    // Only used when separate_rrs_ is true
    ConstRRsetPtr rrset_;
    RdataIteratorPtr rdata_iterator_;
    bool separate_rrs_;
    bool ready_;
public:
    MemoryIterator(const RRClass rrclass,
                   const ZoneTree& tree, const Name& origin,
                   bool separate_rrs) :
        rrclass_(rrclass),
        tree_(tree),
        separate_rrs_(separate_rrs),
        ready_(true)
    {
        // Find the first node (origin) and preserve the node chain for future
        // searches
        ZoneTree::Result result(tree_.find(origin, &node_, chain_));
        // It can't happen that the origin is not in there
        if (result != ZoneTree::EXACTMATCH) {
            isc_throw(Unexpected,
                      "In-memory zone corrupted, missing origin node");
        }
        // Initialize the iterator if there's somewhere to point to
        if (node_ != NULL && node_->getData() != NULL) {
            set_node_ = node_->getData();
            if (separate_rrs_ && set_node_ != NULL) {
                rrset_.reset(new TreeNodeRRset(rrclass_,
                                               node_, set_node_, true));
                rdata_iterator_ = rrset_->getRdataIterator();
            }
        }
    }

    virtual ConstRRsetPtr getNextRRset() {
        if (!ready_) {
            isc_throw(Unexpected, "Iterating past the zone end");
        }
        /*
         * This cycle finds the first nonempty node with yet unused
         * RdataSset.  If it is NULL, we run out of nodes. If it is
         * empty, it doesn't contain any RdataSets. If we are at the
         * end, just get to next one.
         */
        while (node_ != NULL &&
               (node_->getData() == NULL || set_node_ == NULL)) {
            node_ = tree_.nextNode(chain_);
            // If there's a node, initialize the iterator and check next time
            // if the map is empty or not
            if (node_ != NULL && node_->getData() != NULL) {
                set_node_ = node_->getData();
                // New RRset, so get a new rdata iterator
                if (separate_rrs_ && set_node_ != NULL) {
                    rrset_.reset(new TreeNodeRRset(rrclass_,
                                                   node_, set_node_, true));
                    rdata_iterator_ = rrset_->getRdataIterator();
                }
            }
        }
        if (node_ == NULL) {
            // That's all, folks
            ready_ = false;
            return (ConstRRsetPtr());
        }

        if (separate_rrs_) {
            // For separate rrs, reconstruct a new RRset with just the
            // 'current' rdata
            RRsetPtr result(new RRset(rrset_->getName(),
                                      rrset_->getClass(),
                                      rrset_->getType(),
                                      rrset_->getTTL()));
            result->addRdata(rdata_iterator_->getCurrent());
            rdata_iterator_->next();
            if (rdata_iterator_->isLast()) {
                // all used up, next.
                set_node_ = set_node_->getNext();
                // New RRset, so get a new rdata iterator, but only if this
                // was not the final RRset in the chain
                if (set_node_ != NULL) {
                    rrset_.reset(new TreeNodeRRset(rrclass_,
                                                   node_, set_node_, true));
                    rdata_iterator_ = rrset_->getRdataIterator();
                }
            }
            return (result);
        } else {
            ConstRRsetPtr result(new TreeNodeRRset(rrclass_,
                                                   node_, set_node_, true));

            // This one is used, move it to the next time for next call
            set_node_ = set_node_->getNext();

            return (result);
        }
    }

    virtual ConstRRsetPtr getSOA() const {
        isc_throw(NotImplemented, "Not implemented");
    }
};

} // End of anonymous namespace

ZoneIteratorPtr
InMemoryClient::getIterator(const Name& name, bool separate_rrs) const {
    ZoneTable::FindResult result(impl_->zone_table_->findZone(name));
    if (result.code != result::SUCCESS) {
        isc_throw(DataSourceError, "No such zone: " + name.toText());
    }

    return (ZoneIteratorPtr(new MemoryIterator(
                                getClass(),
                                result.zone_data->getZoneTree(), name,
                                separate_rrs)));
}

ZoneUpdaterPtr
InMemoryClient::getUpdater(const isc::dns::Name&, bool, bool) const {
    isc_throw(isc::NotImplemented, "Update attempt on in memory data source");
}

pair<ZoneJournalReader::Result, ZoneJournalReaderPtr>
InMemoryClient::getJournalReader(const isc::dns::Name&, uint32_t,
                                 uint32_t) const
{
    isc_throw(isc::NotImplemented, "Journaling isn't supported for "
              "in memory data source");
}

} // end of namespace memory
} // end of namespace datasrc
} // end of namespace isc
