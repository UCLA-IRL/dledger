#ifndef DLEDGER_INCLUDE_RETURN_CODE_H_
#define DLEDGER_INCLUDE_RETURN_CODE_H_

#include <stdint.h>
#include <stdio.h>
#include <string>

namespace dledger {

class ReturnCode {
public:
  // return a success return code
  ReturnCode() noexcept : m_errorCode(EC_OK), m_status("") {}

  // init a success return code
  static ReturnCode noError() { return ReturnCode(); }

  // init an error caused by no tailing record
  static ReturnCode noTailingRecord() { return ReturnCode(EC_NoTailingRecord, ""); }
  static ReturnCode notEnoughTailingRecord() { return ReturnCode(EC_NotEnoughTailingRecord, ""); }

  static ReturnCode signingError(const std::string& reason) { return ReturnCode(EC_SigningError, reason); }

  bool success() { return m_errorCode == EC_OK; }

private:
  enum ErrorCode {
    EC_OK = 0,
    EC_NoTailingRecord = 1,
    EC_NotEnoughTailingRecord = 2,
    EC_SigningError = 3,
  };

  ReturnCode(ErrorCode code, const std::string& reason) noexcept : m_errorCode(code), m_status(reason) {}

  uint16_t m_errorCode;
  std::string m_status;
};

} // namespace dledger

#endif // define DLEDGER_INCLUDE_RETURN_CODE_H_