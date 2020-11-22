//
// Created by Tyler on 11/17/20.
//

#include "dledger/record.hpp"
#include "dledger/ledger.hpp"
#include "dynamic-function-runner.h"
#include <iostream>
#include <unordered_set>
#include <ndn-cxx/util/scheduler.hpp>
#include <boost/asio/io_service.hpp>
#include <ndn-cxx/util/io.hpp>
#include <random>

std::random_device rd;  //Will be used to obtain a seed for the random number engine
std::mt19937 random_gen(rd()); //Standard mersenne_twister_engine seeded with rd()

using namespace dledger;

std::list<std::string> startingPeerPath({
                                                "./test-certs/test-a.cert",
                                                "./test-certs/test-b.cert",
                                                "./test-certs/test-c.cert",
                                                "./test-certs/test-d.cert"
                                        });

void periodicAddRecord(shared_ptr<Ledger> ledger, Scheduler& scheduler) {
    std::uniform_int_distribution<> distrib(1, 1000000);
    Record record(RecordType::GENERIC_RECORD, std::to_string(distrib(random_gen)));
    record.addRecordItem(makeStringBlock(255, std::to_string(distrib(random_gen))));
    record.addRecordItem(makeStringBlock(255, std::to_string(distrib(random_gen))));
    record.addRecordItem(makeStringBlock(255, std::to_string(distrib(random_gen))));
    ReturnCode result = ledger->createRecord(record);
    if (!result.success()) {
        std::cout << "- Adding record error : " << result.what() << std::endl;
    }

    // schedule for the next record generation
    scheduler.schedule(time::seconds(10), [ledger, &scheduler] { periodicAddRecord(ledger, scheduler); });
}

Block processRecord(shared_ptr<Ledger> ledger, Name recordName, ndn::Block& executionBlock, const DynamicFunctionRunner& runner) {
    printf("Processing: %s\n", recordName.toUri().c_str());
    Record r = *ledger->getRecord(recordName.toUri());
    int inputs[3];
    int i = 0;
    for (const auto& item : r.getRecordItems()) {
        inputs[i] = (int) std::stoi(readString(item));
        i ++;
    }

    //process!
    std::vector<uint8_t> buf(12);
    memcpy(buf.data(), inputs, 12);
    auto ans = runner.runWasmModule(executionBlock, buf);
    int ans_int = *ans.data();
    return makeStringBlock(255, std::to_string(ans_int));
}

void periodicProcessRecord(shared_ptr<Ledger> ledger, Scheduler& scheduler,
        std::unordered_set<Name>& waitingRecords,
        ndn::Block& executionBlock, const DynamicFunctionRunner& runner) {
    std::unordered_map<Name, Block> filteredRecords;

    //pick records to process
    std::set<int> toProcessNum;
    if (waitingRecords.size() <= 5) {
        for (int i = 0; i < waitingRecords.size(); i++)
            toProcessNum.emplace(i);
    } else {
        std::uniform_int_distribution<> distrib(0, waitingRecords.size() - 1);
        while (toProcessNum.size() < 5) {
            toProcessNum.emplace(distrib(random_gen));
        }
    }

    //execute
    int i = 0;
    if (executionBlock.isValid()) {
        for (const auto &item: waitingRecords) {
            if (toProcessNum.count(i) == 1) {
                filteredRecords.emplace(item, processRecord(ledger, item, executionBlock, runner));
            }
            i++;
        }
    }

    //build output block
    std::uniform_int_distribution<> distrib(0, 1000000);
    Record record(RecordType::GENERIC_RECORD, "output_" + std::to_string(distrib(random_gen)));
    for (const auto& filteredItem : filteredRecords) {
        Block block(131);
        block.push_back(filteredItem.first.wireEncode());
        block.push_back(filteredItem.second);
    }
    if (executionBlock.isValid() && toProcessNum.size() != 0) {
        ReturnCode result = ledger->createRecord(record);
        if (!result.success()) {
            std::cout << "- Adding record error : " << result.what() << std::endl;
            scheduler.schedule(time::seconds(1), [&]() { ledger->createRecord(record); });
        }
    }

    // schedule for the next record generation
    scheduler.schedule(time::milliseconds(15000 + distrib(random_gen) % 10000),
            [ledger, &scheduler, &waitingRecords, &executionBlock, runner] {
        periodicProcessRecord(ledger, scheduler, waitingRecords, executionBlock, runner); });
}

void addWasmRecord(shared_ptr<Ledger> ledger) {
    Record record(RecordType::GENERIC_RECORD, "dfi_filter1");
    std::ifstream file("dfi-app/dfi-filter1.wasm", std::ios::binary | std::ios::ate);
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    file.read(buffer.data(), size);
    BOOST_ASSERT(file.good());
    record.addRecordItem(makeBinaryBlock(253, buffer.data(), buffer.size()));
    ReturnCode result = ledger->createRecord(record);
    if (!result.success()) {
        std::cout << "- Adding record error : " << result.what() << std::endl;
    }
}

shared_ptr<Ledger> setupLedger(const std::string& idName, std::shared_ptr<Config> config,
        security::KeyChain &keychain, Face& face, boost::asio::io_service& ioService) {
    shared_ptr<Ledger> ledger = std::move(Ledger::initLedger(*config, keychain, face));
    std::unordered_set<Name> waitingRecords;
    ndn::Block executionBlock;
    DynamicFunctionRunner runner;

    ledger->setOnRecordAppConfirmed([&](const Record &record){
        if (record.getUniqueIdentifier() == "dfi_filter1") { // code block
            executionBlock = *record.getRecordItems().begin();
        } else if (record.getUniqueIdentifier().substr(0, 6) == "output") { // output block
            for (const auto& item : record.getRecordItems()) {
                item.parse();
                auto doneItem = Name(*item.elements_begin());
                waitingRecords.erase(doneItem);
            }
        } else {
            waitingRecords.emplace(record.getRecordName());
        }
    });

    if (idName == "test-a") {
        addWasmRecord(ledger);
    }

    Scheduler scheduler(ioService);
    periodicProcessRecord(ledger, scheduler, waitingRecords, executionBlock, runner);
    scheduler.schedule(time::seconds(2), [ledger, &scheduler]{periodicAddRecord(ledger, scheduler);});

    face.processEvents();
    return ledger;
}

int
main(int argc, char** argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s id_name\n", argv[0]);
        return 1;
    }
    std::srand(std::time(nullptr));
    std::string idName = argv[1];
    boost::asio::io_service ioService;
    Face face(ioService);
    security::KeyChain keychain;
    std::shared_ptr<Config> config = nullptr;
    try {
        config = Config::CustomizedConfig("/dledger-multicast", "/dledger/" + idName,
                                          std::string("./dledger-anchor.cert"), std::string("/tmp/dledger-db/" + idName),
                                          startingPeerPath);
        mkdir("/tmp/dledger-db/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    catch(const std::exception& e) {
        std::cout << e.what() << std::endl;
        return 1;
    }

    auto ledger = setupLedger(idName, config, keychain, face, ioService);
    return 0;
}