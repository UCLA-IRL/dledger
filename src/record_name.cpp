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
        if (getRecordType() == RecordType::BASE_RECORD)
            BOOST_THROW_EXCEPTION(std::runtime_error("record name invalid type"));
    }

    RecordName::RecordName(const Name& peerPrefix, RecordType type, const std::string &identifier,
            time::system_clock::TimePoint time):
            Name(peerPrefix) {
        this->append(recordTypeToString(type));
        append(identifier);
        appendTimestamp(time);
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

    RecordName RecordName::generateRecordName(const Config& config, const Record& record) {
        if (record.getType() == GENESIS_RECORD) {
            Name genesisPrefix(config.peerPrefix.getSubName(0, config.peerPrefix.size() - 1));
            genesisPrefix.append("genesis");
            RecordName recordName(genesisPrefix, record.getType(), record.getUniqueIdentifier(), time::system_clock::time_point());
            return recordName;
        }
        RecordName recordName(config.peerPrefix, record.getType(), record.getUniqueIdentifier());
        return recordName;
    }

}



