#ifndef DLEDGER_INCLUDE_RETURN_CODE_H_
#define DLEDGER_INCLUDE_RETURN_CODE_H_

#include <stdint.h>
#include <stdio.h>
#include <string>
#include <utility>

namespace dledger {

enum ErrorCode {
    EC_OK = 0,
    EC_NoTailingRecord = 1,
    EC_NotEnoughTailingRecord = 2,
    EC_SigningError = 3,
    EC_TimingError = 4
  };

class ReturnCode {
public:
  ReturnCode(std::string value) noexcept : m_errorCode(EC_OK), m_status(std::move(value)) {}
  ReturnCode(ErrorCode code, std::string reason) noexcept : m_errorCode(code), m_status(std::move(reason)) {}

  // init a success return code
  static ReturnCode noError(std::string message = "") { return ReturnCode(std::move(message)); }

  // init an error caused by no tailing record
  static ReturnCode noTailingRecord() { return ReturnCode(EC_NoTailingRecord, "No Tailing Record"); }
  static ReturnCode notEnoughTailingRecord() { return ReturnCode(EC_NotEnoughTailingRecord, "Not Enough Tailing Records"); }

  static ReturnCode signingError(const std::string& reason) { return ReturnCode(EC_SigningError, reason); }
  static ReturnCode timingError(const std::string& reason) { return ReturnCode(EC_TimingError, reason); }


  bool success() { return m_errorCode == EC_OK; }
  std::string what() { return m_status; }
  uint16_t code() {return m_errorCode; }

private:
  uint16_t m_errorCode;
  std::string m_status;
};

} // namespace dledger

#endif // define DLEDGER_INCLUDE_RETURN_CODE_H_