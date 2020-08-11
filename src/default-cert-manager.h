//
// Created by Tyler on 8/8/20.
//

#ifndef DLEDGER_DEFAULT_CERT_MANAGER_H
#define DLEDGER_DEFAULT_CERT_MANAGER_H

#include <unordered_set>
#include "dledger/cert-manager.hpp"

using namespace ndn;
namespace dledger {
    class DefaultCertificateManager : public CertificateManager {
    public:

        DefaultCertificateManager(const Name &peerPrefix,
                                  shared_ptr<security::v2::Certificate> anchorCert,
                                  const std::list<security::v2::Certificate> &startingPeers);

        bool verifySignature(const Data &data) const override;

        bool verifyRecordFormat(const Record &record) const override;

        bool endorseSignature(const Data &data) const override;

        bool verifySignature(const Interest &interest) const override;

        void acceptRecord(const Record &record) override;

        bool authorizedToGenerate() const override;

    private:
        Name getCertificateNameIdentity(const Name &certificateName) const;

        Name m_peerPrefix;
        std::shared_ptr<security::v2::Certificate> m_anchorCert;
        std::map<Name, std::list<security::v2::Certificate>> m_peerCertificates; // first: name of the peer, second: certificate
        std::unordered_set<Name> m_revokedCertificates;
    };
};


#endif //DLEDGER_DEFAULT_CERT_MANAGER_H
