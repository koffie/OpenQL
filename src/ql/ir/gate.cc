/** \file
 * Quantum gate abstraction implementation.
 */

#include "ql/ir/gate.h"

#include <cctype>
#include "ql/utils/num.h"
#include "ql/utils/str.h"

namespace ql {
namespace ir {

using namespace utils;

std::ostream &operator<<(std::ostream &os, GateType gate_type) {
    switch (gate_type) {
        case GateType::IDENTITY: os << "IDENTITY"; break;
        case GateType::HADAMARD: os << "HADAMARD"; break;
        case GateType::PAULI_X: os << "PAULI_X"; break;
        case GateType::PAULI_Y: os << "PAULI_Y"; break;
        case GateType::PAULI_Z: os << "PAULI_Z"; break;
        case GateType::PHASE: os << "PHASE"; break;
        case GateType::PHASE_DAG: os << "PHASE_DAG"; break;
        case GateType::T: os << "T"; break;
        case GateType::T_DAG: os << "T_DAG"; break;
        case GateType::RX90: os << "RX90"; break;
        case GateType::MRX90: os << "RXM90"; break;
        case GateType::RX180: os << "RX180"; break;
        case GateType::RY90: os << "RY90"; break;
        case GateType::MRY90: os << "RYM90"; break;
        case GateType::RY180: os << "RY180"; break;
        case GateType::RX: os << "RX"; break;
        case GateType::RY: os << "RY"; break;
        case GateType::RZ: os << "RZ"; break;
        case GateType::PREP_Z: os << "PREP_Z"; break;
        case GateType::CNOT: os << "CNOT"; break;
        case GateType::CPHASE: os << "CPHASE"; break;
        case GateType::TOFFOLI: os << "TOFFOLI"; break;
        case GateType::CUSTOM: os << "CUSTOM"; break;
        case GateType::COMPOSITE: os << "COMPOSITE"; break;
        case GateType::MEASURE: os << "MEASURE"; break;
        case GateType::DISPLAY: os << "DISPLAY"; break;
        case GateType::DISPLAY_BINARY: os << "DISPLAY_BINARY"; break;
        case GateType::NOP: os << "NOP"; break;
        case GateType::DUMMY: os << "DUMMY"; break;
        case GateType::SWAP: os << "SWAP"; break;
        case GateType::WAIT: os << "WAIT"; break;
        case GateType::CLASSICAL: os << "CLASSICAL"; break;
        default: os << "UNKNOWN"; break;
    }
    return os;
}

std::ostream &operator<<(std::ostream &os, ConditionType condition_type) {
    switch (condition_type) {
        case ConditionType::ALWAYS: os << "ALWAYS"; break;
        case ConditionType::NEVER: os << "NEVER"; break;
        case ConditionType::UNARY: os << "UNARY"; break;
        case ConditionType::NOT: os << "NOT"; break;
        case ConditionType::AND: os << "AND"; break;
        case ConditionType::NAND: os << "NAND"; break;
        case ConditionType::OR: os << "OR"; break;
        case ConditionType::NOR: os << "NOR"; break;
        case ConditionType::XOR: os << "XOR"; break;
        case ConditionType::NXOR: os << "NXOR"; break;
        default: os << "UNKNOWN"; break;
    }
    return os;
}

Bool Gate::is_conditional() const {
    return condition != ConditionType::ALWAYS;
}

Instruction Gate::cond_qasm() const {
    QL_ASSERT(Gate::is_valid_cond(condition, cond_operands));
    switch (condition) {
        case ConditionType::ALWAYS:
            return "";
        case ConditionType::NEVER:
            return Instruction("cond(0) ");
        case ConditionType::UNARY:
            return Instruction("cond(b[" + to_string(cond_operands[0]) + "]) ");
        case ConditionType::NOT:
            return Instruction("cond(!b[" + to_string(cond_operands[0]) + "]) ");
        case ConditionType::AND:
            return Instruction("cond(b[" + to_string(cond_operands[0]) + "]&&b[" + to_string(cond_operands[1]) + ") ");
        case ConditionType::NAND:
            return Instruction("cond(!(b[" + to_string(cond_operands[0]) + "]&&b[" + to_string(cond_operands[1]) + ")) ");
        case ConditionType::OR:
            return Instruction("cond(b[" + to_string(cond_operands[0]) + "]||b[" + to_string(cond_operands[1]) + ") ");
        case ConditionType::NOR:
            return Instruction("cond(!(b[" + to_string(cond_operands[0]) + "]||b[" + to_string(cond_operands[1]) + ")) ");
        case ConditionType::XOR:
            return Instruction("cond(b[" + to_string(cond_operands[0]) + "]^^b[" + to_string(cond_operands[1]) + ") ");
        case ConditionType::NXOR:
            return Instruction("cond(!(b[" + to_string(cond_operands[0]) + "]^^b[" + to_string(cond_operands[1]) + ")) ");
    }
    return "";
}

Bool Gate::is_valid_cond(ConditionType condition, const Vec<UInt> &cond_operands) {
    switch (condition) {
    case ConditionType::ALWAYS:
    case ConditionType::NEVER:
        return (cond_operands.size()==0);
    case ConditionType::UNARY:
    case ConditionType::NOT:
        return (cond_operands.size()==1);
    case ConditionType::AND:
    case ConditionType::NAND:
    case ConditionType::OR:
    case ConditionType::NOR:
    case ConditionType::XOR:
    case ConditionType::NXOR:
        return (cond_operands.size()==2);
    }
    return false;
}

namespace gate_types {

Identity::Identity(UInt q) {
    name = "i";
    duration = 40;
    operands.push_back(q);
}

Instruction Identity::qasm() const {
    return Instruction(cond_qasm() + "i q[" + to_string(operands[0]) + "]");
}

GateType Identity::type() const {
    return GateType::IDENTITY;
}

Hadamard::Hadamard(UInt q) {
    name = "h";
    duration = 40;
    operands.push_back(q);
}

Instruction Hadamard::qasm() const {
    return Instruction(cond_qasm() + "h q[" + to_string(operands[0]) + "]");
}

GateType Hadamard::type() const {
    return GateType::HADAMARD;
}

Phase::Phase(UInt q) {
    name = "s";
    duration = 40;
    operands.push_back(q);
}

Instruction Phase::qasm() const {
    return Instruction(cond_qasm() + "s q[" + to_string(operands[0]) + "]");
}

GateType Phase::type() const {
    return GateType::PHASE;
}

PhaseDag::PhaseDag(UInt q) {
    name = "sdag";
    duration = 40;
    operands.push_back(q);
}

Instruction PhaseDag::qasm() const {
    return Instruction(cond_qasm() + "sdag q[" + to_string(operands[0]) + "]");
}

GateType PhaseDag::type() const {
    return GateType::PHASE_DAG;
}

RX::RX(UInt q, double theta) {
    name = "rx";
    duration = 40;
    angle = theta;
    operands.push_back(q);
}

Instruction RX::qasm() const {
    return Instruction(cond_qasm() + "rx q[" + to_string(operands[0]) + "], " + to_string(angle));
}

GateType RX::type() const {
    return GateType::RX;
}

RY::RY(UInt q, double theta) {
    name = "ry";
    duration = 40;
    angle = theta;
    operands.push_back(q);
}

Instruction RY::qasm() const {
    return Instruction(cond_qasm() + "ry q[" + to_string(operands[0]) + "], " + to_string(angle));
}

GateType RY::type() const {
    return GateType::RY;
}

RZ::RZ(UInt q, double theta) {
    name = "rz";
    duration = 40;
    angle = theta;
    operands.push_back(q);
}

Instruction RZ::qasm() const {
    return Instruction(cond_qasm() + "rz q[" + to_string(operands[0]) + "], " + to_string(angle));
}

GateType RZ::type() const {
    return GateType::RZ;
}

T::T(UInt q) {
    name = "t";
    duration = 40;
    operands.push_back(q);
}

Instruction T::qasm() const {
    return Instruction(cond_qasm() + "t q[" + to_string(operands[0]) + "]");
}

GateType T::type() const {
    return GateType::T;
}

TDag::TDag(UInt q) {
    name = "tdag";
    duration = 40;
    operands.push_back(q);
}

Instruction TDag::qasm() const {
    return Instruction(cond_qasm() + "tdag q[" + to_string(operands[0]) + "]");
}

GateType TDag::type() const {
    return GateType::T_DAG;
}

PauliX::PauliX(UInt q) {
    name = "x";
    duration = 40;
    operands.push_back(q);
}

Instruction PauliX::qasm() const {
    return Instruction(cond_qasm() + "x q[" + to_string(operands[0]) + "]");
}

GateType PauliX::type() const {
    return GateType::PAULI_X;
}

PauliY::PauliY(UInt q) {
    name = "y";
    duration = 40;
    operands.push_back(q);
}

Instruction PauliY::qasm() const {
    return Instruction(cond_qasm() + "y q[" + to_string(operands[0]) + "]");
}

GateType PauliY::type() const {
    return GateType::PAULI_Y;
}

PauliZ::PauliZ(UInt q) {
    name = "z";
    duration = 40;
    operands.push_back(q);
}

Instruction PauliZ::qasm() const {
    return Instruction(cond_qasm() + "z q[" + to_string(operands[0]) + "]");
}

GateType PauliZ::type() const {
    return GateType::PAULI_Z;
}

RX90::RX90(UInt q) {
    name = "x90";
    duration = 40;
    operands.push_back(q);
}

Instruction RX90::qasm() const {
    return Instruction(cond_qasm() + "x90 q[" + to_string(operands[0]) + "]");
}

GateType RX90::type() const {
    return GateType::RX90;
}

MRX90::MRX90(UInt q) {
    name = "mx90";
    duration = 40;
    operands.push_back(q);
}

Instruction MRX90::qasm() const {
    return Instruction(cond_qasm() + "mx90 q[" + to_string(operands[0]) + "]");
}

GateType MRX90::type() const {
    return GateType::MRX90;
}

RX180::RX180(UInt q) {
    name = "x180";
    duration = 40;
    operands.push_back(q);
}

Instruction RX180::qasm() const {
    return Instruction(cond_qasm() + "x180 q[" + to_string(operands[0]) + "]");
}

GateType RX180::type() const {
    return GateType::RX180;
}

RY90::RY90(UInt q) {
    name = "y90";
    duration = 40;
    operands.push_back(q);
}

Instruction RY90::qasm() const {
    return Instruction(cond_qasm() + "y90 q[" + to_string(operands[0]) + "]");
}

GateType RY90::type() const {
    return GateType::RY90;
}

MRY90::MRY90(UInt q) {
    name = "my90";
    duration = 40;
    operands.push_back(q);
}

Instruction MRY90::qasm() const {
    return Instruction(cond_qasm() + "my90 q[" + to_string(operands[0]) + "]");
}

GateType MRY90::type() const {
    return GateType::MRY90;
}

RY180::RY180(UInt q) {
    name = "y180";
    duration = 40;
    operands.push_back(q);
}

Instruction RY180::qasm() const {
    return Instruction(cond_qasm() + "y180 q[" + to_string(operands[0]) + "]");
}

GateType RY180::type() const {
    return GateType::RY180;
}

Measure::Measure(UInt q) {
    name = "measure";
    duration = 40;
    operands.push_back(q);
}

Measure::Measure(UInt q, UInt c) {
    name = "measure";
    duration = 40;
    operands.push_back(q);
    creg_operands.push_back(c);
}

Instruction Measure::qasm() const {
    StrStrm ss;
    ss << "measure ";
    ss << "q[" << operands[0] << "]";
    if (!creg_operands.empty()) {
        ss << ", r[" << creg_operands[0] << "]";
    }

    return Instruction(ss.str());
}

GateType Measure::type() const {
    return GateType::MEASURE;
}

PrepZ::PrepZ(UInt q) {
    name = "prep_z";
    duration = 40;
    operands.push_back(q);
}

Instruction PrepZ::qasm() const {
    return Instruction(cond_qasm() + "prep_z q[" + to_string(operands[0]) + "]");
}

GateType PrepZ::type() const {
    return GateType::PREP_Z;
}

CNot::CNot(UInt q1, UInt q2) {
    name = "cnot";
    duration = 80;
    operands.push_back(q1);
    operands.push_back(q2);
}

Instruction CNot::qasm() const {
    return Instruction(cond_qasm() + "cnot q[" + to_string(operands[0]) + "]"
                       + ",q[" + to_string(operands[1]) + "]");
}

GateType CNot::type() const {
    return GateType::CNOT;
}

CPhase::CPhase(UInt q1, UInt q2) {
    name = "cz";
    duration = 80;
    operands.push_back(q1);
    operands.push_back(q2);
}

Instruction CPhase::qasm() const {
    return Instruction(cond_qasm() + "cz q[" + to_string(operands[0]) + "]"
                       + ",q[" + to_string(operands[1]) + "]");
}

GateType CPhase::type() const {
    return GateType::CPHASE;
}

Toffoli::Toffoli(UInt q1, UInt q2, UInt q3) {
    name = "toffoli";
    duration = 160;
    operands.push_back(q1);
    operands.push_back(q2);
    operands.push_back(q3);
}

Instruction Toffoli::qasm() const {
    return Instruction(cond_qasm() + "toffoli q[" + to_string(operands[0]) + "]"
                       + ",q[" + to_string(operands[1]) + "]"
                       + ",q[" + to_string(operands[2]) + "]");
}

GateType Toffoli::type() const {
    return GateType::TOFFOLI;
}

Nop::Nop() {
    name = "wait";
    duration = 20;
}

Instruction Nop::qasm() const {
    return Instruction("nop");
}

GateType Nop::type() const {
    return GateType::NOP;
}

Swap::Swap(UInt q1, UInt q2) {
    name = "swap";
    duration = 80;
    operands.push_back(q1);
    operands.push_back(q2);
}

Instruction Swap::qasm() const {
    return Instruction(cond_qasm() + "swap q[" + to_string(operands[0]) + "]"
                       + ",q[" + to_string(operands[1]) + "]");
}

GateType Swap::type() const {
    return GateType::SWAP;
}

/****************************************************************************\
| Special gates
\****************************************************************************/

Wait::Wait(Vec<UInt> qubits, UInt d, UInt dc) {
    name = "wait";
    duration = d;
    duration_in_cycles = dc;
    for (auto &q : qubits) {
        operands.push_back(q);
    }
}

Instruction Wait::qasm() const {
    return Instruction("wait " + to_string(duration_in_cycles));
}

GateType Wait::type() const {
    return GateType::WAIT;
}

Source::Source() {
    name = "SOURCE";
    duration = 1;
}

Instruction Source::qasm() const {
    return Instruction("SOURCE");
}

GateType Source::type() const {
    return GateType::DUMMY;
}

Sink::Sink() {
    name = "Sink";
    duration = 1;
}

Instruction Sink::qasm() const {
    return Instruction("SINK");
}

GateType Sink::type() const {
    return GateType::DUMMY;
}

Display::Display() {
    name = "display";
    duration = 0;
}

Instruction Display::qasm() const {
    return Instruction("display");
}

GateType Display::type() const {
    return GateType::DISPLAY;
}

Custom::Custom(const Str &name) {
    this->name = name;  // just remember name, e.g. "x", "x %0" or "x q0", expansion is done by add_custom_gate_if_available().
    // FIXME: no syntax check is performed
}

Custom::Custom(const Custom &g) {
    // FIXME JvS: This copy constructor does NOT copy everything, and apparently
    // the scheduler relies on it not doing so!
    QL_DOUT("Custom gate copy constructor for " << g.name);
    name = g.name;
    // operands = g.operands; FIXME
    creg_operands = g.creg_operands;
    // int_operand = g.int_operand; FIXME
    duration = g.duration;
    // angle = g.angle; FIXME
    // cycle = g.cycle; FIXME
}

/**
 * match qubit id
 */
Bool Custom::is_qubit_id(const Str &str) {
    if (str[0] != 'q') {
        return false;
    }
    UInt l = str.length();
    for (UInt i = 1; i < l; ++i) {
        if (!std::isdigit(str[i])) {
            return false;
        }
    }
    return true;
}

/**
 * return qubit id
 */
UInt Custom::qubit_id(const Str &qubit) {
    Str id = qubit.substr(1);
    return parse_uint(id.c_str());
}

/**
 * load instruction from json map
 */
void Custom::load(nlohmann::json &instr) {
    QL_DOUT("loading instruction '" << name << "'...");
    Str l_attr = "(none)";
    try {
        l_attr = "qubits";
        QL_DOUT("qubits: " << instr["qubits"]);
        UInt parameters = instr["qubits"].size();
        for (UInt i = 0; i < parameters; ++i) {
            Str qid = instr["qubits"][i];
            if (!is_qubit_id(qid)) {
                QL_EOUT("invalid qubit id in attribute 'qubits' !");
                throw Exception(
                    "[x] error : ql::custom_gate() : error while loading instruction '" +
                    name + "' : attribute 'qubits' : invalid qubit id !",
                    false);
            }
            operands.push_back(qubit_id(qid));
        }
        l_attr = "duration";
        duration = instr["duration"];
        QL_DOUT("duration: " << instr["duration"]);
        l_attr = "matrix";
        // FIXME: make matrix optional, default to NaN
        auto mat = instr["matrix"];
        QL_DOUT("matrix: " << instr["matrix"]);

    } catch (Json::exception &e) {
        QL_EOUT("while loading instruction '" << name << "' (attr: " << l_attr
                                              << ") : " << e.what());
        throw Exception(
            "[x] error : ql::custom_gate() : error while loading instruction '" +
            name + "' : attribute '" + l_attr + "' : \n\t" + e.what(), false);
    }

}

void Custom::print_info() const {
    QL_PRINTLN("[-] custom gate : ");
    QL_PRINTLN("    |- name     : " << name);
    QL_PRINTLN("    |- qubits   : " << to_string(operands));
    QL_PRINTLN("    |- duration : " << duration);
}

Instruction Custom::qasm() const {
    StrStrm ss;
    UInt p = name.find(' ');
    Str gate_name = name.substr(0, p);
    ss << cond_qasm();
    if (operands.empty()) {
        ss << gate_name;
    } else if (operands.size() == 1) {
        ss << gate_name << " q[" << operands[0] << "]";
    } else {
        ss << gate_name << " q[" << operands[0] << "]";
        for (UInt i = 1; i < operands.size(); i++) {
            ss << ",q[" << operands[i] << "]";
        }
    }

    // deal with custom gates with argument, such as angle
    if (gate_name == "rx" || gate_name == "ry" || gate_name == "rz") {	// FIXME: implicitly defining semantics here
        ss << ", " << angle;
    }

    if (creg_operands.size() == 1) {
        ss << ", r[" << creg_operands[0] << "]";
    } else if (creg_operands.size() > 1) {
        ss << ", r[" << creg_operands[0] << "]";
        for (size_t i = 1; i < creg_operands.size(); i++) {
            ss << ", r[" << creg_operands[i] << "]";
        }
    }

    if (breg_operands.size() == 1) {
        ss << ", b[" << breg_operands[0] << "]";
    } else if (breg_operands.size() > 1) {
        ss << ", b[" << breg_operands[0] << "]";
        for (size_t i = 1; i < breg_operands.size(); i++) {
            ss << ", b[" << breg_operands[i] << "]";
        }
    }

    return Instruction(ss.str());
}

GateType Custom::type() const {
    return GateType::CUSTOM;
}

Composite::Composite(const Str &name) : Custom(name) {
    duration = 0;
}

Composite::Composite(const Str &name, const GateRefs &seq) : Custom(name) {
    duration = 0;
    for (const auto &g : seq) {
        gs.add(g);
        duration += g->duration;    // FIXME: not true if gates operate in parallel
        operands.insert(operands.end(), g->operands.begin(), g->operands.end());
    }
}

Instruction Composite::qasm() const {
    StrStrm instr;
    for (const auto &g : gs) {
        instr << g->qasm() << std::endl;
    }
    return Instruction(instr.str());
}

GateType Composite::type() const {
    return GateType::COMPOSITE;
}

} // namespace gates
} // namespace ir
} // namespace ql
