#include "mos6502.hpp"
// Stardard Library Headers
#include <cstdint>
#include <utility>
// Project Headers
#include "bus.hpp"

// ------------------------ MOS6502 Pointer Class ------------------------------

MOS6502::MOS6502Ptr::MOS6502Ptr(const uint16_t& virtual_address): 
    address{std::in_place_type<uint16_t>, virtual_address} {}

MOS6502::MOS6502Ptr::MOS6502Ptr(const MOS6502::MOS6502Ptr::Register& target_register):
    address{std::in_place_type<MOS6502::MOS6502Ptr::Register>, target_register} {}

void MOS6502::MOS6502Ptr::operator=(const uint16_t& virtual_address) {
    address = virtual_address;
}

void MOS6502::MOS6502Ptr::operator=(const MOS6502::MOS6502Ptr::Register& target_register) {
    address = target_register;
}

uint8_t& MOS6502::MOS6502Ptr::dereference(MOS6502& cpu) {
    // Holds Virtual Memory Address
    if (std::holds_alternative<uint16_t>(address)) {
        return cpu.getReferenceToMemory(std::get<uint16_t>(address));
    }
    // Holds "Address" for Register
    switch (std::get<MOS6502::MOS6502Ptr::Register>(address)) {
        case MOS6502::MOS6502Ptr::Register::ACCUMULATOR:
            return cpu.accumulator_;
    }
}

// ----------------------------- MOS6502 Class ---------------------------------

const std::unordered_map<uint8_t, MOS6502::Instruction> MOS6502::instruction_lookup_table = {
    {0x69, {MOS6502::ImmediateAddressingMode, MOS6502::ADC, 2}}
};

MOS6502::MOS6502(): bus(nullptr), program_counter_(0xFFFC), stack_ptr_(0), accumulator_(0), 
                    x_reg_(0), y_reg_(0), processor_status_({.RAW_VALUE=0}),
                    total_cycle_ran_(0), instruction_(nullptr), instruction_opcode_(0x00), 
                    instruction_cycle_remaining_(0), operand_address_(0x00), 
                    relative_addressing_offset_(0) {}

void MOS6502::connectBUS(BUS* target_bus) {
    bus = target_bus;
}

void MOS6502::runCycle() {
    static bool fetched = false;
    static void (*opcode_fn)(uint16_t& program_counter) = nullptr;

    total_cycle_ran_++;

    // Fetch new instruction (Cost 1 cycle)
    if (!fetched) {
        instruction_opcode_ = readMemory(program_counter_);
        program_counter_++;

        instruction_ = &instruction_lookup_table.at(instruction_opcode_);
        
        instruction_->operationFn(*this);

        fetched = true;
        return;
    }

    // If opcode cycle
    if (instruction_cycle_remaining_ == 0) {
        opcode_fn(program_counter_);
        fetched = false;
    }
    instruction_cycle_remaining_--;
}

void MOS6502::outputCurrentState(std::ostream &out) const {
    for (uint8_t i = 0; i < 8; i++) {
        bool cur_bit = processor_status_.RAW_VALUE & (1 << i);
        out << cur_bit;
    }
    out << std::endl;
    // Output total cycles elapsed
    out << "Cycles Elapsed: " << total_cycle_ran_ << std::endl;
}

void MOS6502::ADC(MOS6502& cpu) {
    uint16_t result = static_cast<uint16_t>(cpu.accumulator_) + static_cast<uint16_t>(cpu.operand_address_.dereference(cpu)) + static_cast<uint16_t>(cpu.processor_status_.CARRY);

    cpu.processor_status_.ZERO = (result == 0);
    cpu.processor_status_.CARRY = (result > 0x00FF);

    // If the result sign was different from accumulator sign and accumulator sign is the same as the operand sign,
    //   then we have an overflow
    if (((cpu.accumulator_ ^ result) & 0x80) && !((cpu.accumulator_ ^ cpu.operand_address_.dereference(cpu)) & 0x80)) {
        cpu.processor_status_.OVERFLOW_FLAG = 1;
    }
    else {
        cpu.processor_status_.OVERFLOW_FLAG = 0;
    }

    cpu.processor_status_.NEGATIVE = ((result & 0x0080) > 0);
    // Update the accumulator
    cpu.accumulator_ = result & 0x00FF;
}

void MOS6502::AND(MOS6502& cpu) {
    cpu.accumulator_ &= cpu.operand_address_.dereference(cpu);
    cpu.processor_status_.ZERO = (cpu.accumulator_ == 0);
    cpu.processor_status_.NEGATIVE = ((cpu.accumulator_ & 0x80) > 0);
}

void MOS6502::ASL(MOS6502& cpu) {
    uint8_t result = cpu.operand_address_.dereference(cpu) << 0x01;

    cpu.processor_status_.CARRY = ((cpu.operand_address_.dereference(cpu) & 0x80) > 0);
    cpu.processor_status_.ZERO = (result == 0);
    cpu.processor_status_.NEGATIVE = ((result & 0x80) > 0);

    cpu.operand_address_.dereference(cpu) = result;
}

void MOS6502::BCC(MOS6502& cpu) {
    if (!cpu.processor_status_.CARRY) {
        uint8_t new_pc_address = cpu.program_counter_ + cpu.relative_addressing_offset_;
        // Branch success adds 1 cycle
        cpu.instruction_cycle_remaining_++;
        // If branched to a new page we need to add 1 more cycle
        if ((cpu.program_counter_ & 0xFF00) != (new_pc_address & 0xFF00)) {
            cpu.instruction_cycle_remaining_++;
        }
        cpu.program_counter_ = new_pc_address;
    }
}

void MOS6502::BCS(MOS6502& cpu) {
    if (cpu.processor_status_.CARRY) {
        uint8_t new_pc_address = cpu.program_counter_ + cpu.relative_addressing_offset_;
        // Branch success adds 1 cycle
        cpu.instruction_cycle_remaining_++;
        // If branched to a new page we need to add 1 more cycle
        if ((cpu.program_counter_ & 0xFF00) != (new_pc_address & 0xFF00)) {
            cpu.instruction_cycle_remaining_++;
        }
        cpu.program_counter_ = new_pc_address;
    }
}

void MOS6502::BEQ(MOS6502& cpu) {
    if (cpu.processor_status_.ZERO) {
        uint8_t new_pc_address = cpu.program_counter_ + cpu.relative_addressing_offset_;
        // Branch success adds 1 cycle
        cpu.instruction_cycle_remaining_++;
        // If branched to a new page we need to add 1 more cycle
        if ((cpu.program_counter_ & 0xFF00) != (new_pc_address & 0xFF00)) {
            cpu.instruction_cycle_remaining_++;
        }
        cpu.program_counter_ = new_pc_address;
    }
}

void MOS6502::ImplicitAddressingMode(MOS6502& cpu) {
    cpu.operand_address_ = MOS6502Ptr::Register::ACCUMULATOR;
}

void MOS6502::ImmediateAddressingMode(MOS6502& cpu) {
    cpu.operand_address_ = cpu.program_counter_;
    cpu.program_counter_++;
}

void MOS6502::ZeroPageAddressingMode(MOS6502& cpu) {
    cpu.operand_address_ = cpu.readMemory(cpu.program_counter_);
    cpu.program_counter_++;
}

void MOS6502::ZeroPageXAddressingMode(MOS6502& cpu) {
    cpu.operand_address_ = cpu.readMemory(cpu.program_counter_) + cpu.x_reg_;
    cpu.program_counter_++;
}

void MOS6502::ZeroPageYAddressingMode(MOS6502& cpu) {
    cpu.operand_address_ = cpu.readMemory(cpu.program_counter_) + cpu.y_reg_;
    cpu.program_counter_++;
}

void MOS6502::RelativeAddressingMode(MOS6502& cpu) {
    uint8_t relativeSkip = cpu.readMemory(cpu.program_counter_);
    cpu.program_counter_++;
    cpu.relative_addressing_offset_ = *reinterpret_cast<int8_t*>(&relativeSkip);
}

void MOS6502::AbsoluteAddressingMode(MOS6502& cpu) {
    uint8_t address_low_byte = cpu.readMemory(cpu.program_counter_);
    cpu.program_counter_++;
    uint8_t address_high_byte = cpu.readMemory(cpu.program_counter_);
    cpu.program_counter_++;

    cpu.operand_address_ = (static_cast<uint16_t>(address_high_byte) << 8) + static_cast<uint16_t>(address_low_byte);
}

void MOS6502::AbsoluteXAddressingMode(MOS6502& cpu) {
    uint8_t address_low_byte = cpu.readMemory(cpu.program_counter_);
    cpu.program_counter_++;
    uint8_t address_high_byte = cpu.readMemory(cpu.program_counter_);
    cpu.program_counter_++;

    cpu.operand_address_ = (static_cast<uint16_t>(address_high_byte) << 8) + static_cast<uint16_t>(address_low_byte) + static_cast<uint16_t>(cpu.x_reg_);
}

void MOS6502::AbsoluteYAddressingMode(MOS6502& cpu) {
    uint8_t address_low_byte = cpu.readMemory(cpu.program_counter_);
    cpu.program_counter_++;
    uint8_t address_high_byte = cpu.readMemory(cpu.program_counter_);
    cpu.program_counter_++;

    cpu.operand_address_ = (static_cast<uint16_t>(address_high_byte) << 8) + static_cast<uint16_t>(address_low_byte) + static_cast<uint16_t>(cpu.y_reg_);
}

void MOS6502::IndirectAddressingMode(MOS6502& cpu) {
    uint8_t address_low_byte = cpu.readMemory(cpu.program_counter_);
    cpu.program_counter_++;
    uint8_t address_high_byte = cpu.readMemory(cpu.program_counter_);
    cpu.program_counter_++;

    uint16_t target_address = (static_cast<uint16_t>(address_high_byte) << 8) + static_cast<uint16_t>(address_low_byte);
    uint8_t indirect_address_low_byte = cpu.readMemory(target_address);
    uint8_t indirect_address_high_byte = cpu.readMemory(target_address + 1);

    cpu.operand_address_ = (static_cast<uint16_t>(indirect_address_high_byte) << 8) + static_cast<uint16_t>(indirect_address_low_byte);
}
