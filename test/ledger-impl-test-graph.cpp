#include "dledger/record.hpp"
#include "dledger/ledger.hpp"
#include <iostream>
#include <fstream>
#include <ndn-cxx/security/signature-sha256-with-rsa.hpp>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/asio/io_service.hpp>

#include <ndn-cxx/util/io.hpp>

using namespace dledger;

std::list<std::string> startingPeerPath({
                                                "./test-1a.cert",
                                                "./test-1b.cert",
                                                "./test-1c.cert",
                                                "./test-1d.cert",
                                                "./test-1e.cert"
                                        });

void periodicAddRecord(shared_ptr<Ledger> ledger, Scheduler& scheduler) {
    Record record(RecordType::GENERIC_RECORD, std::to_string(std::rand()));
    record.addRecordItem(makeStringBlock(255, std::to_string(std::rand())));
    record.addRecordItem(makeStringBlock(255, std::to_string(std::rand())));
    record.addRecordItem(makeStringBlock(255, std::to_string(std::rand())));
    ReturnCode result = ledger->createRecord(record);
    if (!result.success()) {
        std::cout << "- Adding record error : " << result.what() << std::endl;
    }

    // schedule for the next record generation
    scheduler.schedule(time::seconds(5), [ledger, &scheduler] { periodicAddRecord(ledger, scheduler); });
}

std::string getNodeDigest(const Record &r) {
    auto hash = r.m_data->getFullName().get(-1).toUri();
    hash = hash.substr(hash.size() - 5);
    return "\"" + r.getProducerID() + '/' + hash + "\"";
}

std::string getNodeAttribute(const Record &r) {
    if (r.getType() == CERTIFICATE_RECORD) return "[fillcolor=blue, style=filled, fontcolor=white]";
    if (r.getType() == REVOCATION_RECORD) return "[fillcolor=red, style=filled, fontcolor=white]";
    if (r.getProducerID() == "test-2a") return "[fillcolor=yellow, style=filled]";
    if (r.getProducerID() == "test-1b") return "[fillcolor=green, style=filled]";
    if (r.getProducerID() == "test-2b") return "[fillcolor=grey, style=filled]";
    return "";
}

int
main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s id_name\n", argv[0]);
        return 1;
    }
    std::string idName = argv[1];
    boost::asio::io_service ioService;
    Face face(ioService);
    security::KeyChain keychain;
    std::shared_ptr<Config> config = nullptr;

    std::ofstream dot_log;
    dot_log.open("records.txt");

    try {
        config = Config::CustomizedConfig("/dledger-multicast", "/dledger/" + idName,
                                          std::string("./dledger-anchor.cert"),
                                          std::string("/tmp/dledger-db/" + idName), startingPeerPath);
        mkdir("/tmp/dledger-db/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    catch (const std::exception &e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    shared_ptr<Ledger> ledger = std::move(Ledger::initLedger(*config, keychain, face));
    ledger->setOnRecordAppConfirmed([&dot_log, &ledger](const Record &r) {
        dot_log << getNodeDigest(r) << " " << getNodeAttribute(r) << ";" << std::endl;
        for (const auto &ptr: r.getPointersFromHeader()) {
            auto ancestor = ledger->getRecord(ptr.toUri());
            if (ancestor.has_value())
                dot_log << getNodeDigest(r) << " -> " << getNodeDigest(*ancestor) << ";" << std::endl;
        }

        if (r.getType() == CERTIFICATE_RECORD) {
            CertificateRecord certRecord(r);
            for (const auto &ptr: certRecord.getPrevCertificates()) {
                auto ancestor = ledger->getRecord(ptr.toUri());
                if (ancestor.has_value())
                    dot_log << getNodeDigest(r) << " -> " << getNodeDigest(*ancestor) << "[color=blue, style=dashed];"
                            << std::endl;
            }
        }
    });

    Scheduler scheduler(ioService);
    scheduler.schedule(time::seconds(1), [ledger, &scheduler] { periodicAddRecord(ledger, scheduler); });

    face.processEvents(time::seconds(180));
    return 0;
}