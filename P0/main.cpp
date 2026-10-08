// Command shell. Commands are case-insensitive; anything that is not a
// command is passed to the system shell, as in the course framework.
//
// Adding a command = write a cmdXxx() function and add one row to commands[].

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "circuit.h"
#include "levelizer.h"

namespace {

using Args = std::vector<std::string>;

Circuit ckt;
bool loaded = false;  // a circuit has been read successfully
bool done = false;

void cmdRead(const Args& a);
void cmdPc(const Args& a);
void cmdLev(const Args& a);
void cmdHelp(const Args& a);
void cmdQuit(const Args& a);

struct Command {
    const char* name;
    const char* usage;   // shown by HELP
    bool needsCircuit;   // READ must succeed first
    std::size_t nargs;   // required arguments
    void (*run)(const Args&);
};

const Command commands[] = {
    {"READ", "READ <file.ckt>    read a circuit and build its data structure", false, 1, cmdRead},
    {"PC",   "PC                 print the circuit",                            true,  0, cmdPc},
    {"LEV",  "LEV <output-file>  levelize the circuit and write the report",    true,  1, cmdLev},
    {"HELP", "HELP               print this help",                              false, 0, cmdHelp},
    {"QUIT", "QUIT               exit",                                         false, 0, cmdQuit},
};

void cmdRead(const Args& a) {
    std::string err;
    if (!ckt.read(a[0], err)) {
        std::cout << err << "\n";
        return;
    }
    loaded = true;
    std::cout << "==> OK";
}

void cmdPc(const Args&) { ckt.print(); }

void cmdLev(const Args& a) {
    int left = levelize(ckt);
    if (left > 0)
        std::cout << "Warning: " << left << " line(s) not levelized (combinational loop?)\n";
    if (!writeLevels(ckt, a[0])) {
        std::cout << "Cannot open output file " << a[0] << "\n";
        return;
    }
    std::cout << "==> OK";
}

void cmdHelp(const Args&) {
    for (const Command& c : commands) std::cout << c.usage << "\n";
}

void cmdQuit(const Args&) { done = true; }

std::string toUpper(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

}  // namespace

int main() {
    std::string line;
    while (!done) {
        std::cout << "\nCommand>" << std::flush;
        if (!std::getline(std::cin, line)) break;

        std::istringstream ss(line);
        std::string word;
        if (!(ss >> word)) continue;
        const std::string name = toUpper(word);
        Args args;
        while (ss >> word) args.push_back(word);

        const Command* cmd = nullptr;
        for (const Command& c : commands)
            if (name == c.name) cmd = &c;

        if (!cmd) {
            std::cout << std::flush;
            int rc = std::system(line.c_str());
            (void)rc;
        } else if (cmd->needsCircuit && !loaded) {
            std::cout << "Execution out of sequence!\n";
        } else if (args.size() < cmd->nargs) {
            std::cout << "Usage: " << cmd->usage << "\n";
        } else {
            cmd->run(args);
        }
    }
    return 0;
}
