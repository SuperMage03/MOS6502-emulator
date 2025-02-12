#ifndef _JSON_TEST_HARNESS_HPP_
#define _JSON_TEST_HARNESS_HPP_
// Standard Library Includes
#include <string>
#include <cstdint>
// External Library Includes
#include <nlohmann/json.hpp>
// Project Includes
#include "mos6502.hpp"
// Using shorthand declarations
using json = nlohmann::json;

class JSONTestHarness {
public:
    enum Result {
        TEST_OK,
        TEST_FAILED,
        ALL_TESTS_PASSED,
    };

    /**
    * @brief  Constructor for JSONTestHarness
    * @param  cpu: Target CPU
    * @param  file_path: Path to JSON File
    * @return None
    */
    JSONTestHarness(MOS6502& cpu, const std::string& file_path);

    /**
    * @brief  Executes a single instruction
    * @param  None
    * @return Result of the test
    */
    Result singleInstructionStep();
    
private:
    uint32_t instructions_tested_;
    MOS6502& cpu_;
    json test_json_;
};

#endif
