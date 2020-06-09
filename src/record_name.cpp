//
// Created by Tyler on 6/3/20.
//

#include "record_name.hpp"

using namespace ndn;
namespace dledger {
    // record Name: /<application-common-prefix>/<producer-name>/<record-type>/<record-name>/<timestamp>
    RecordName::RecordName(const Name& name): Name(name) {
        if (this->size() < 5 || (hasImplicitDigest() && this->size() < 6))
            BOOST_THROW_EXCEPTION(std::runtime_error("record name too short"));
        getGenerationTimestamp();
        if (getRecordType() == RecordType::BaseRecord)
            BOOST_THROW_EXCEPTION(std::runtime_error("record name invalid type"));
    }

    std::string RecordName::getApplicationCommonPrefix() const {
        int numSuffix = hasImplicitDigest() ? 5 : 4;
        return this->getSubName(0, size() - numSuffix).toUri();
    }

    std::string RecordName::getProducerID() const {
        int location = hasImplicitDigest() ? -5 : -4;
        return readString(get(location));
    }

    RecordType RecordName::getRecordType() const {
        int location = hasImplicitDigest() ? -4 : -3;
        return stringToRecordType(readString(get(location)));
    }

    std::string RecordName::getRecordUniqueIdentifier() const {
        int location = hasImplicitDigest() ? -3 : -2;
        return readString(get(location));
    }

    time::system_clock::TimePoint RecordName::getGenerationTimestamp() const {
        int location = hasImplicitDigest() ? -2 : -1;
        return get(location).toTimestamp();
    }

    bool RecordName::hasImplicitDigest() const {
        return this->get(-1).isImplicitSha256Digest();
    }

    std::string RecordName::getImplicitDigest() const {
        if (hasImplicitDigest())
            return readString(this->get(-1));
        else
            return "";
    }

    RecordName RecordName::generateGenesisRecordName(Config config, int i) {
        Name recordName(config.peerPrefix.getSubName(0, config.peerPrefix.size() - 1));
        recordName.append("genesis").append(recordTypeToString(GenesisRecord)).append(std::to_string(i));
        recordName.appendTimestamp(time::system_clock::time_point());
        return recordName;
    }

    RecordName RecordName::generateGenericRecordName(Config config, Record record) {
        Name recordName(config.peerPrefix);
        recordName.append(recordTypeToString(record.getType()))
        .append(record.getUniqueIdentifier())
        .appendTimestamp();
        return recordName;
    }

}



