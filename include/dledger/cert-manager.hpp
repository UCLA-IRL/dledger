//
// Created by Tyler on 8/8/20.
//

#ifndef DLEDGER_CERT_MANAGER_HPP
#define DLEDGER_CERT_MANAGER_HPP

#include <ndn-cxx/name.hpp>
#include <ndn-cxx/security/certificate.hpp>
#include <ndn-cxx/interest.hpp>
#include "record.hpp"

using namespace ndn;
namespace dledger {
    class CertificateManager {
    public:

        /**
         * Verify if signature of the data is valid as
         * a record in the system (can be from a revoked certificate).
         * This make sure an old record can be recovered after disconnection
         * @param data the data to be check
         * @return true if the signature is valid
         */
        virtual bool verifySignature(const Data &data) const = 0;

        /**
         * Verify if the certificate or revocation record
         * has correct format
         * @param record a certificate or revocation record
         * @return true if the signature is valid
         */
        virtual bool verifyRecordFormat(const Record &record) const = 0;

        /**
         * Verify if signature of the data is valid as
         * a record in the system (cannot be from a revoked certificate)
         * @param data the data to be check
         * @return true if the signature is valid
         */
        virtual bool endorseSignature(const Data &data) const = 0;

        /**
         * Verify if signature of the interest is valid as
         * a record in the system (cannot be from a revoked certificate)
         * @param interest the interest to be check
         * @return true if the signature is valid
         */
        virtual bool verifySignature(const Interest &interest) const = 0;

        /**
         * accept an certificate or revocation record.
         * @param record the record to be accepted
         */
        virtual void acceptRecord(const Record &record) = 0;

        /**
         * check if the identity have valid certificate to generate
         * @return true if the identity have valid certificate to generate
         */
        virtual bool authorizedToGenerate() const = 0;

    };
}

#endif //DLEDGER_CERT_MANAGER_HPP
