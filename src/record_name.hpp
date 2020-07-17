//
// Created by Tyler on 6/3/20.
//

#ifndef DLEDGER_RECORD_NAME_HPP
#define DLEDGER_RECORD_NAME_HPP

#include <ndn-cxx/name.hpp>
#include <dledger/config.hpp>
#include <dledger/record.hpp>

using namespace ndn;
namespace dledger {

class RecordName: public Name {
public:
    // record Name: /<application-common-prefix>/<producer-name>/<record-type>/<record-name>/<timestamp>
    RecordName(const Name& name);
    RecordName(const Name& peerPrefix, RecordType type, const std::string &identifier, time::system_clock::TimePoint time=time::system_clock::now());
    std::string getApplicationCommonPrefix() const;
    std::string getProducerID() const;
    RecordType getRecordType() const;
    std::string getRecordUniqueIdentifier() const;
    time::system_clock::TimePoint getGenerationTimestamp() const;
    bool hasImplicitDigest() const;
    std::string getImplicitDigest() const;

    //generate names
    static RecordName generateRecordName(const Config& config, const Record& record);
    };

}


#endif //DLEDGER_RECORD_NAME_HPP
