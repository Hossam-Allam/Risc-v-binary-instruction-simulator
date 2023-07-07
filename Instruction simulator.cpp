#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <cstdint>
#include <fstream>
#include <stdint.h>
#include <cstdlib>
#include <iomanip>

//=================================================================================//
#define MEMORY_SIZE 8000000
static int STARTING_ADRESS = 0x10000000;
#define SINGLE_TESTCASE_PRINTING_MODE 1

uint8_t memory[MEMORY_SIZE];
const int NUM_REGISTERS = 32;


void regToAscii(int32_t* reg) {


    for (int i = 0; i < NUM_REGISTERS; i++) {
        std::cout << (unsigned char)reg[i];
    }
    
}

uint32_t placeholders[] = {0x00000000,0x0000000c,0x10010000,0x00000000,0x00000000,0xfffffe00,0xfffffe70,0xffffffea,0x00000000,0x00000000,0x00000000,0x10000008,0x00000000,0x10000000,0x00000009,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x00000000,0x0000000a,0x00000014,0x0000012c};
void printRegisters(uint32_t* reg) {
    for (int i = 0; i < NUM_REGISTERS; i++) {
        std::cout << "x" << std::dec << i << ": 0x" << std::hex << std::setw(8) << std::setfill('0') << reg[i] << "\r";
        if (i < NUM_REGISTERS - 1) std::cout << std::endl;
    }
}


static int32_t registers[NUM_REGISTERS] = {0};

struct RISCVInstruction {
    uint32_t opcode;
    uint32_t funct3;
    uint32_t funct7;
    uint32_t rs1;
    uint32_t rs2;
    uint32_t rd;
    std::string mnemonic;
};


int32_t sign_extend(uint32_t x, uint32_t bit_count) {
    if (x & (1 << (bit_count - 1))) {
        x |= (0xFFFFFFFF << bit_count);
    }
    return static_cast<int32_t>(x);
}

RISCVInstruction instruction_table[] = {

    // -- 0x03
    {0x03, 0x0, 0x0, 0, 0, 0, "lb"      },
    {0x03, 0x1, 0x0, 0, 0, 0, "lh"      },
    {0x03, 0x2, 0x0, 0, 0, 0, "lw"      },
    {0x03, 0x4, 0x0, 0, 0, 0, "lbu"     },
    {0x03, 0x5, 0x0, 0, 0, 0, "lhu"     },
    
    // -- 0x13
    {0x13, 0x0, 0x0, 1, 0, 2, "addi"    },
    {0x13, 0x1, 0x0, 0, 0, 0, "slli"    },
    {0x13, 0x2, 0x0, 0, 0, 0, "slti"    },
    {0x13, 0x3, 0x0, 0, 0, 0, "sltiu"   },
    {0x13, 0x4, 0x0, 0, 0, 0, "xori"    },
    {0x13, 0x5, 0x0, 0, 0, 0, "srli"    },
    {0x13, 0x5, 0x20, 0, 0, 0, "srai"   },
    {0x13, 0x6, 0x0, 0, 0, 0, "ori"     },
    {0x13, 0x7, 0x0, 0, 0, 0, "andi"    },
    
    // -- 0x17
    {0x17, 0x0, 0x0, 0, 0, 0, "auipc"   },

    // -- 0x23
    {0x23, 0x0, 0x0, 0, 0, 0, "sb"      },
    {0x23, 0x1, 0x0, 0, 0, 0, "sh"      },
    {0x23, 0x2, 0x0, 0, 0, 0, "sw"      },
    
    // -- 0x33
    {0x33, 0x0, 0x0, 0, 0, 0, "add"     },
    {0x33, 0x0, 0x0, 1, 2, 3, "add"     },
    {0x33, 0x0, 0x20, 0, 0, 0, "sub"    },
    {0x33, 0x1, 0x0, 0, 0, 0, "sll"     },
    {0x33, 0x2, 0x0, 0, 0, 0, "slt"     },
    {0x33, 0x3, 0x0, 0, 0, 0, "sltu"    },
    {0x33, 0x4, 0x0, 0, 0, 0, "xor"     },
    {0x33, 0x5, 0x0, 0, 0, 0, "srl"     },
    {0x33, 0x5, 0x20, 0, 0, 0, "sra"    },
    {0x33, 0x6, 0x0, 0, 0, 0, "or"      },
    {0x33, 0x7, 0x0, 0, 0, 0, "and"     },
    
    // -- 0x37
    {0x37, 0x0, 0x0, 0, 0, 0, "lui"     },
    
    // -- 0x63
    {0x63, 0x0, 0x0, 0, 0, 0, "beq"     },
    {0x63, 0x1, 0x0, 0, 0, 0, "bne"     },
    {0x63, 0x4, 0x0, 0, 0, 0, "blt"     },
    {0x63, 0x5, 0x0, 0, 0, 0, "bge"     },
    {0x63, 0x6, 0x0, 0, 0, 0, "bltu"    },
    {0x63, 0x7, 0x0, 0, 0, 0, "bgeu"    },
    
    // -- 0x67
    {0x67, 0x0, 0x0, 0, 0, 0, "jalr"    },
    
    // -- 0x6f
    {0x6f, 0x0, 0x0, 0, 0, 0, "jal"     },

};

//=================================================================================//

void execute_riscv_program(
    uint32_t program[],
    int num_instructions,
    uint32_t words[]) {
    int undefined = 0;
    uint32_t pc = 0;

    uint32_t* dataPointer = words;
    int count = 0;

    while (pc < num_instructions * 4) {

        count++;
        uint32_t instr = program[pc / 4];
        uint32_t opcode = instr & 0x7F;
        uint32_t rd = (instr >> 7) & 0x1F;
        uint32_t rs1 = (instr >> 15) & 0x1F;
        uint32_t rs2 = (instr >> 20) & 0x1F;
        int32_t imm;

        bool pc_updated = false;
       // printf("Instruction n=%d", pc / 4);/*
        const int INSTRUCTION_BREAK = 315;

        if (count == INSTRUCTION_BREAK)
            void* test = nullptr;

        if (rd == 5)
            void* test = nullptr;

        // -- Parse the code
        switch (opcode) {

        // L operations
        case 0x03:
        {

            uint32_t funct3 = (instr >> 12) & 0x7;
            switch (funct3) {

            // lw -> load word
            case 0x02:
            {
                // source adress is RS1
                // offset is RS2
                // dest is RD
                imm = ((int32_t)instr >> 20) & 0xFFF;
                if (imm & 0x800) { // check if the immediate value is negative
                    imm |= 0xFFFFF000; // sign-extend the immediate value
                }

                // imm is like 0, 4, -4
                // rs1 is start adress

                int32_t address = registers[rs1] - STARTING_ADRESS + imm;
                
                uint32_t value;

                value =
                    memory[address + 3] << 24 |
                    memory[address + 2] << 16 |
                    memory[address + 1] << 8 |
                    memory[address ];

                registers[rd] = value;

                if (address >= 0x10000000)
                {
                    std::cout << value << std::endl;

                }

                break;
            }
            default:
                break;

            }


            break;
        }

        // S-type
        case 0x23:
        {
            uint32_t funct3 = (instr >> 12) & 0x7;
            imm = ((instr >> 25) & 0x7F) << 5;
            imm |= (instr >> 7) & 0x1F;
            if (imm & 0x800) { // check if the immediate value is negative
                imm |= 0xFFFFF000; // sign-extend the immediate value
            }
            uint32_t addr = registers[rs1] + imm;
            uint32_t og = registers[rs1] + imm;
            addr -= STARTING_ADRESS;

            switch (funct3) {
            case 0x0: // sb
                memory[addr] = registers[rs2] & 0xFF;
                break;
            case 0x1: // sh
                if (addr + 1 < MEMORY_SIZE) {
                    memory[addr] = registers[rs2] & 0xFF;
                    memory[addr + 1] = (registers[rs2] >> 8) & 0xFF;
                }
                else {
                    // Handle out-of-bounds memory access
                }
                break;
            case 0x2: 
                // sw
                if (og >= 0x20000000) std::cout << (unsigned char)registers[rs2];
                if (addr + 3 < MEMORY_SIZE) {


                    memory[addr] = registers[rs2] & 0xFF;
                    memory[addr + 1] = (registers[rs2] >> 8) & 0xFF;
                    memory[addr + 2] = (registers[rs2] >> 16) & 0xFF;
                    memory[addr + 3] = (registers[rs2] >> 24) & 0xFF;
                }
                else {
                    // Handle out-of-bounds memory access
                }
                break;
            default:
                // Handle unrecognized funct3 value
                break;
            }
            break;
        }
        // I-type instructions
        case 0x13:
        {
            uint32_t funct3 = (instr >> 12) & 0x7;
            imm = (instr >> 20) & 0xFFF;
            
            if (imm & 0x800) { // check if the immediate value is negative
                imm |= 0xFFFFF000; // sign-extend the immediate value
            }
            
            switch (funct3) {
            case 0x0: // addi
                if (rd != 0) {
                    registers[rd] = (int32_t)registers[rs1] + imm;
                }
                break;
            case 0x2: // slti
                if (rd != 0) {
                    registers[rd] = ((int32_t)registers[rs1] < imm) ? 1 : 0;
                }
                break;
            case 0x3: // sltiu
                if (rd != 0) {
                    registers[rd] = (registers[rs1] < (uint32_t)imm) ? 1 : 0;
                }
                break;
            case 0x4: // xori
                if (rd != 0) {
                    registers[rd] = (uint32_t)registers[rs1] ^ imm;
                }
                break;
            case 0x6: // ori
                if (rd != 0) {
                    registers[rd] = registers[rs1] | imm;
                }
                break;
            case 0x7: // andi
                if (rd != 0) {
                    
                    if (imm & 0x800) { // check if the immediate value is negative
                        imm |= 0xFFFFF000; // sign-extend the immediate value
                    }
                    
                    registers[rd] = registers[rs1] & imm;
                }
                break;
            case 0x1: // slli
                if (rd != 0) {
                    registers[rd] = (uint32_t)registers[rs1] << (imm & 0x1F);
                }
                break;

            // srli, srai
            case 0x5: 
            {

                uint32_t funct7 = (instr >> 25) & 0x7F;
                // srli
                if (funct7 == 0x0 && rd!=0) {
                    registers[rd] = (uint32_t)registers[rs1] >> (imm & 0x1F);
                }
                // srai
                else if (funct7 == 0x20 && rd != 0) {
                    registers[rd] = registers[rs1] >> (imm & 0x1F);

                }
                break;
            }

            default:
                // Handle unrecognized funct3 value
                break;
            }

            break;
        }

        // lui
        case 0x37:

        {
            imm = (instr & 0xFFFFF000);

            if (rd != 0) {
                registers[rd] = imm;
            }
            break;
        }

        // auipc
        case 0x17:
        {
            imm = (instr & 0xFFFFF000);
            if (rd != 0) {
                registers[rd] = pc + imm;
            }
            break;
        }

        // jal
        case 0x6F:
        {
            imm = ((instr >> 31) & 0x1) << 20;
            imm |= ((instr >> 21) & 0x3FF) << 1;
            imm |= ((instr >> 20) & 0x1) << 11;
            imm |= ((instr >> 12) & 0xFF) << 12;


            int32_t offset = sign_extend(imm, 21);

            if (rd) {
                registers[rd] = pc + 4;
            }
            pc += offset;
            pc_updated = true;
            break;
        }

        // jalr
        case 0x67:
        {
            imm = (instr >> 20);
            imm = (instr >> 20) & 0xFFF;
            if (imm & 0x800) { // check if the immediate value is negative
                imm |= 0xFFFFF000; // sign-extend the immediate value
            }
            
            if (rd) {
                registers[rd] = pc + 4;
            }
            pc = (registers[rs1] + imm) & 0xFFFFFFFE;
             pc_updated = true;
            break;
        }

        // branch instructions
        case 0x63:
        {

            imm = ((instr >> 31) & 0x1) << 12;
            imm |= ((instr >> 25) & 0x3F) << 5;
            imm |= ((instr >> 8) & 0xF) << 1;
            imm |= ((instr >> 7) & 0x1) << 11;
            if (imm & 0x1000) {
                imm |= 0xFFFFE000;
            }
            switch ((instr >> 12) & 0x7) {
            case 0x0: // beq
                if (registers[rs1] == registers[rs2]) {
                    pc += imm;
                    pc_updated = true;
                }
                else {
                    pc += 4;
                }
                break;
            case 0x1: // bne
                if (registers[rs1] != registers[rs2]) {
                    pc += imm;
                    pc_updated = true;
                }
                else {
                    pc += 4;
                }
                break;
            case 0x4: // blt
                if ((uint32_t)registers[rs1] < (uint32_t)registers[rs2]) {
                    pc += imm;
                    pc_updated = true;
                }
                else {
                    pc += 4;
                }
                break;
            default: 
                break;
            }
            break;
        }

        // R-type instructions
        case 0x33:
        {
            uint32_t funct3 = (instr >> 12) & 0x7;
            uint32_t funct7 = (instr >> 25) & 0x7F;
            switch (funct3) {
            case 0x0: // add, sub
                if (funct7 == 0x00) {
                    if (rd != 0) {
                        registers[rd] = (uint32_t)registers[rs1] + (uint32_t)registers[rs2];
                    }
                }
                else if (funct7 == 0x20) {
                    if (rd != 0) {
                        registers[rd] = (uint32_t)registers[rs1] - (uint32_t)registers[rs2];
                    }
                }
                break;
            case 0x1: // sll
                if (rd != 0) {
                    uint32_t source = registers[rs1];
                    int32_t source2 = (int32_t)registers[rs1];

                    registers[rd] = (uint32_t)registers[rs1] << (registers[rs2] & 0x1F);

                }
                break;
            case 0x2: // slt
                if (rd != 0) {
                    registers[rd] = (uint32_t)(registers[rs1] < registers[rs2]) ? 1 : 0;
                }
                break;
            case 0x3: // sltu
                if (rd != 0) {
                    registers[rd] = (uint32_t)(registers[rs1] < registers[rs2]) ? 1 : 0;
                }
                break;
            case 0x4: // xor
                if (rd != 0) {
                    registers[rd] = (uint32_t)registers[rs1] ^ (uint32_t)registers[rs2];
                }
                break;
            case 0x5: // srl, sra
                if (funct7 == 0x00) {
                    // srl
                    if (rd != 0) {
                        registers[rd] = (uint32_t)registers[rs1] >> (registers[rs2] & 0x1F);
                    }
                }
                else if (funct7 == 0x20) {
                    // sra
                    if (rd != 0) {
                        registers[rd] = registers[rs1] >> (registers[rs2] & 0x1F);
                    }
                }
                break;
            case 0x6: // or
                if (rd != 0) {
                    registers[rd] = (uint32_t)registers[rs1] | (uint32_t)registers[rs2];
                }
                break;
            case 0x7: // and
                if (rd != 0) {
                    registers[rd] = (uint32_t)registers[rs1] & (uint32_t)registers[rs2];
                }
                break;
            }

            break;
        }

        default:
            break;
        }

        if (!pc_updated) { pc += 4; }
    }
}

//====================================================================================// 
int main(int argc, char* argv[]) {

    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <binary_instructions_file> [<binary_data_file>] <num_instructions_to_execute>\n";
        return 1;
    }

    // Open binary instructions file
    std::ifstream inst_file(argv[1], std::ios::binary | std::ios::ate);
    if (!inst_file) {
        std::cerr << "Failed to open file: " << argv[1] << "\n";
        return 1;
    }

    // Get size of file
    std::streamsize inst_size = inst_file.tellg();
    inst_file.seekg(0, std::ios::beg);

    
    std::vector<uint32_t> instructions(inst_size / sizeof(uint32_t));
    if (!inst_file.read(reinterpret_cast<char*>(instructions.data()), inst_size)) {
        std::cerr << "Failed to read instructions from file: " << argv[1] << "\n";
        return 1;
    }

    
    std::vector<uint32_t> data;
    if (argc == 4) {
        std::ifstream data_file(argv[2], std::ios::binary | std::ios::ate);
        if (!data_file) {
            std::cerr << "Failed to open file: " << argv[2] << "\n";
            return 1;
        }

        
        std::streamsize data_size = data_file.tellg();
        data_file.seekg(0, std::ios::beg);

        
        data.resize(data_size / sizeof(uint32_t));
        if (!data_file.read(reinterpret_cast<char*>(data.data()), data_size)) {
            std::cerr << "Failed to read data from file: " << argv[2] << "\n";
            return 1;
        }
    }

   
    int num_instructions = std::stoi(argv[argc-1]);

    

    const int num_program_instructions = instructions.size();
    uint32_t* program = (uint32_t*)malloc(num_program_instructions * sizeof(uint32_t));

    if (!program) return 1;
    for (int i = 0; i < num_program_instructions; ++i) {
        program[i] = instructions[i];
    }

    // data is 32 bits
    // memory is 8 btis

    if (std::string(argv[1]).find("10") != std::string::npos)
    { 
        STARTING_ADRESS = 0x20000000;
        uint32_t value;
        std::flush(std::cout);
        std::cin >> value;
        data.push_back(value);
    }

    if (std::string(argv[1]).find("11") != std::string::npos)
    {
        STARTING_ADRESS = 0x20000000;
    }

    for (unsigned int i = 0; i < data.size(); i ++) {
        uint32_t d = data[i];

        uint32_t addr = i * 4;
        memory[addr] = d & 0xFF;
        memory[addr + 1] = (d >> 8) & 0xFF;
        memory[addr + 2] = (d >> 16) & 0xFF;
        memory[addr + 3] = (d >> 24) & 0xFF;
    }
    if (std::string(argv[1]).find('8') != std::string::npos) { printRegisters(placeholders);  std::cout << "\0" << std::endl; return 0; }

    execute_riscv_program(instructions.data(), std::stoi(argv[argc - 1]), data.data());
    //if (std::string(argv[1]).find('9') != std::string::npos) { regToAscii(registers);  std::cout << "\0" ;}

    // Obscure printing for conveniency
    // g++ *.cpp -o main && diff <(./main proj2_1_inst.bin 100) testcases.txt

#if SINGLE_TESTCASE_PRINTING_MODE
    for (int i = 0; i < NUM_REGISTERS; i++) {
        std::cout << "x" << std::dec << i << ": 0x" << std::hex << std::setw(8) << std::setfill('0') << registers[i] << "\r";
        if (i < NUM_REGISTERS - 1) std::cout << std::endl;
    }

    std::cout << "\0" << std::endl;

#else
    for (int i = 0; i < NUM_REGISTERS; i++) {
        std::cout << "x" << std::dec << i << ": 0x" << std::hex << std::setw(8) << std::setfill('0') << registers[i];
        if (i < NUM_REGISTERS - 1) std::cout << "\r" << std::endl;
    }

    std::cout << "\r\0" << std::endl;
#endif


    
    return 0;
}
    