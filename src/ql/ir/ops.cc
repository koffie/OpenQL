/** \file
 * Defines basic access operations on the IR.
 */

#include "ql/ir/ops.h"

namespace ql {
namespace ir {

/**
 * Returns the data type with the given name, or returns an empty link if the
 * type does not exist.
 */
DataTypeLink find_type(const Ref &ir, const utils::Str &name) {
    auto begin = ir->platform->data_types.get_vec().begin();
    auto end = ir->platform->data_types.get_vec().end();
    auto pos = std::lower_bound(
        begin, end,
        utils::make<QubitType>(name),
        compare_by_name<DataType>
    );
    if (pos == end || (*pos)->name != name) {
        return {};
    } else {
        return *pos;
    }
}

/**
 * Returns the data type of/returned by an expression.
 */
DataTypeLink get_type_of(const ExpressionRef &expr) {
    if (auto lit = expr->as_literal()) {
        return lit->data_type;
    } else if (auto ref = expr->as_reference()) {
        return ref->data_type;
    } else if (auto fnc = expr->as_function_call()) {
        return fnc->function_type->return_type;
    } else {
        QL_ICE("unknown expression node type encountered");
    }
}

/**
 * Returns the maximum value that an integer of the given type may have.
 */
utils::Int get_max_int_for(const IntType &ityp) {
    auto bits = ityp.bits;
    if (ityp.is_signed) bits--;
    return (utils::Int)((1ull << bits) - 1);
}

/**
 * Returns the minimum value that an integer of the given type may have.
 */
utils::Int get_min_int_for(const IntType &ityp) {
    if (!ityp.is_signed) return 0;
    return (utils::Int)(-(1ull << (ityp.bits - 1)));
}

/**
 * Adds a physical object to the platform.
 */
ObjectLink add_physical_object(const Ref &ir, const utils::One<PhysicalObject> &obj) {

    // Check its name.
    if (!std::regex_match(obj->name, IDENTIFIER_RE)) {
        QL_USER_ERROR(
            "invalid name for new register: \"" <<
            obj->name << "\" is not a valid identifier"
        );
    }

    // Insert it in the right position to maintain list order by name, while
    // doing a name uniqueness test at the same time.
    auto begin = ir->platform->objects.get_vec().begin();
    auto end = ir->platform->objects.get_vec().end();
    auto pos = std::lower_bound(begin, end, obj, compare_by_name<PhysicalObject>);
    if (pos != end && (*pos)->name == obj->name) {
        QL_USER_ERROR(
            "invalid name for new register: \"" <<
            obj->name << "\" is already in use"
        );
    }
    ir->platform->objects.get_vec().insert(pos, obj);

    return obj;
}

/**
 * Returns the physical object with the given name, or returns an empty link if
 * the object does not exist.
 */
ObjectLink find_physical_object(const Ref &ir, const utils::Str &name) {
    auto begin = ir->platform->objects.get_vec().begin();
    auto end = ir->platform->objects.get_vec().end();
    auto pos = std::lower_bound(
        begin, end,
        utils::make<PhysicalObject>(name),
        compare_by_name<PhysicalObject>
    );
    if (pos == end || (*pos)->name != name) {
        return {};
    } else {
        return *pos;
    }
}

/**
 * Adds an instruction type to the platform, or return the matching instruction
 * type specialization without changing anything in the IR if one already
 * existed. The boolean in the return value indicates what happened: if true, a
 * new instruction type was added. The incoming instruction_type object should
 * be fully generalized; template operands can be attached with the optional
 * additional argument (in which case the instruction specialization tree will
 * be generated appropriately).
 */
static utils::Pair<InstructionTypeLink, utils::Bool> add_or_find_instruction_type(
    const Ref &ir,
    const utils::One<InstructionType> &instruction_type,
    const utils::Any<Expression> &template_operands = {}
) {
    QL_ASSERT(instruction_type->specializations.empty());
    QL_ASSERT(instruction_type->template_operands.empty());
    QL_ASSERT(instruction_type->generalization.empty());

    // Check its name.
    if (!std::regex_match(instruction_type->name, IDENTIFIER_RE)) {
        QL_USER_ERROR(
            "invalid name for new instruction type: \"" <<
            instruction_type->name << "\" is not a valid identifier"
        );
    }

    // Search for an existing matching instruction.
    auto begin = ir->platform->instructions.get_vec().begin();
    auto end = ir->platform->instructions.get_vec().end();
    auto pos = std::lower_bound(begin, end, instruction_type, compare_by_name<InstructionType>);
    auto already_exists = false;
    for (; pos != end && (*pos)->name == instruction_type->name; ++pos) {
        if ((*pos)->operand_types.size() != instruction_type->operand_types.size()) {
            continue;
        }
        auto match = true;
        for (utils::UInt i = 0; i < (*pos)->operand_types.size(); i++) {
            if ((*pos)->operand_types[i]->data_type != instruction_type->operand_types[i]->data_type) {
                match = false;
                break;
            }
        }
        if (match) {
            already_exists = true;
            break;
        }
    }

    // If the generalized instruction doesn't already exist, add it.
    auto added_anything = false;
    if (!already_exists) {
        auto clone = instruction_type.clone();
        clone->copy_annotations(*instruction_type);

        // The decompositions can't be cloned, because the links to
        // parameters and objects won't be updated properly (at least at the
        // time of writing clone() isn't smart enough for that). But we only
        // want them in the final, most specialized node anyway. So we add
        // the original from instruction_type at the end.
        clone->decompositions.reset();

        pos = ir->platform->instructions.get_vec().insert(pos, clone);
        added_anything = true;
    } else {

        // If it did already exist, copy the operand access modes from the
        // existing instruction. The assumption here is that the first time the
        // instruction is added is the "best" instruction in terms of
        // descriptiveness, so we need to copy anything that must be the same
        // across specializations to the incoming instruction type in case it's
        // added.
        for (utils::UInt i = 0; i < (*pos)->operand_types.size(); i++) {
            instruction_type->operand_types[i]->mode = (*pos)->operand_types[i]->mode;
        }

    }

    // Now create/add/look for specializations as appropriate.
    auto ityp = *pos;
    for (utils::UInt i = 0; i < template_operands.size(); i++) {
        auto op = template_operands[i];

        // See if the specialization already exists, and if so, recurse into
        // it.
        auto already_exists = false;
        for (const auto &spec : ityp->specializations) {
            if (spec->template_operands.back().equals(op)) {
                already_exists = true;
                ityp = spec;
                break;
            }
        }
        if (already_exists) {
            continue;
        }

        // The specialization doesn't exist yet, so we need to create it.
        auto spec = instruction_type.clone();
        spec->copy_annotations(*instruction_type);
        spec->decompositions.reset();

        // Move from operand types into template operands.
        for (utils::UInt j = 0; j <= i; j++) {
            QL_ASSERT(spec->operand_types[0]->data_type == get_type_of(template_operands[j]));
            spec->operand_types.remove(0);
            auto op_clone = template_operands[j].clone();
            op_clone->copy_annotations(*template_operands[j]);
            spec->template_operands.add(op_clone);
        }

        // Link the specialization up.
        ityp->specializations.add(spec);
        spec->generalization = ityp;
        added_anything = true;

        // Advance to next.
        ityp = spec;

    }

    // If we added an instruction type, make sure to add the decomposition rules
    // to the specialization.
    if (added_anything) {
        ityp->decompositions = instruction_type->decompositions;
    }

    return {ityp, added_anything};
}

/**
 * Adds an instruction type to the platform. The instruction_type object should
 * be fully generalized; template operands can be attached with the optional
 * additional argument (in which case the instruction specialization tree will
 * be generated appropriately).
 */
InstructionTypeLink add_instruction_type(
    const Ref &ir,
    const utils::One<InstructionType> &instruction_type,
    const utils::Any<Expression> &template_operands
) {

    // Defer to add_or_find_instruction_type().
    auto result = add_or_find_instruction_type(ir, instruction_type, template_operands);

    // If we didn't add anything because a matching specialization of a matching
    // instruction already existed, either throw an error or return the existing
    // instruction
    if (!result.second) {
        QL_USER_ERROR(
            "duplicate instruction type: " << describe(instruction_type)
        );
    }

    return result.first;
}

/**
 * Finds an instruction type based on its name and operand types. If
 * generate_overload_if_needed is set, and no instruction with the given name
 * and operand type set exists, then an overload is generated for the first
 * instruction type for which only the name matches, and that overload is
 * returned. If no matching instruction type is found or was created, an empty
 * link is returned.
 */
InstructionTypeLink find_instruction_type(
    const Ref &ir,
    const utils::Str &name,
    const utils::Vec<DataTypeLink> &types,
    utils::Bool generate_overload_if_needed
) {

    // Search for a matching instruction.
    auto begin = ir->platform->instructions.get_vec().begin();
    auto end = ir->platform->instructions.get_vec().end();
    auto first = std::lower_bound(
        begin, end,
        utils::make<InstructionType>(name),
        compare_by_name<InstructionType>
    );
    auto pos = first;
    for (; pos != end && (*pos)->name == name; ++pos) {
        if ((*pos)->operand_types.size() != types.size()) {
            continue;
        }
        auto match = true;
        for (utils::UInt i = 0; i < (*pos)->operand_types.size(); i++) {
            if ((*pos)->operand_types[i]->data_type != types[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            return *pos;
        }
    }

    // pos equalling first implies that (*pos)->name != name, i.e. there is no
    // instruction by this name.
    if (pos == first) {
        return {};
    }

    // If we shouldn't generate an overload if only the name matches, stop now.
    if (!generate_overload_if_needed) {
        return {};
    }

    // Generate an overload for this instruction with the given set of
    // parameters, conservatively assuming write access mode. This is based on
    // the first instruction we encountered with this name.
    auto ityp = first->clone();
    ityp->copy_annotations(**first);
    ityp->operand_types.reset();
    for (const auto &typ : types) {
        ityp->operand_types.emplace(prim::AccessMode::WRITE, typ);
    }

    // Insert the instruction just after all the other instructions with this
    // name, i.e. at pos, to maintain sort order.
    ir->platform->instructions.get_vec().insert(pos, ityp);

    return ityp;
}

/**
 * Builds a new instruction node based on the given name and operand list. Its
 * behavior depends on name.
 *
 *  - If "set", a set instruction is created. Exactly two operands must be
 *    specified, of which the first is the LHS and the second is the RHS. The
 *    LHS must be a reference, and have a classical data type. The RHS must have
 *    exactly the same data type as the LHS.
 *  - If "wait", a wait instruction is created. The first operand must be a
 *    non-negative integer literal, representing the duration. The remainder of
 *    the operands are what's waited on, and must be references. If there is
 *    only one operand, the instruction is a full barrier (i.e. it effectively
 *    waits on all objects).
 *  - If "barrier", a zero-duration wait instruction is created. The operands
 *    are what's waited on, and must be references. If there are no operands,
 *    the instruction is a full barrier (i.e. it effectively waits on all
 *    objects).
 *  - Any other name is treated as a custom instruction, resolved via
 *    find_instruction_type(). The most specialized instruction type is used.
 *
 * If no condition is specified, the instruction will be unconditional (a
 * literal true node is generated for it). For wait instructions, the specified
 * condition *must* be null, as wait instructions are always unconditional.
 *
 * Note that goto and dummy instructions cannot be created via this interface.
 *
 * The generate_overload_if_needed and return_empty_on_failure flags are hacks
 * for the conversion process from the old to new IR. See
 * find_instruction_type() for the former. The latter flag disables the
 * exception that would otherwise be thrown if no matching instruction type is
 * found, instead returning {}.
 */
InstructionRef make_instruction(
    const Ref &ir,
    const utils::Str &name,
    const utils::Any<Expression> &operands,
    const ExpressionRef &condition,
    utils::Bool generate_overload_if_needed,
    utils::Bool return_empty_on_failure
) {
    InstructionRef insn;
    if (name == "set") {

        // Build a set instruction.
        if (operands.size() != 2) {
            QL_USER_ERROR(
                "set instructions must have exactly two operands"
            );
        }
        if (!operands[0]->as_reference()) {
            QL_USER_ERROR(
                "the left-hand side of a set instructions must be a reference"
            );
        }
        auto type = get_type_of(operands[0]);
        if (!type->as_classical_type()) {
            QL_USER_ERROR(
                "set instructions only support classical data types"
            );
        }
        if (type != get_type_of(operands[1])) {
            QL_USER_ERROR(
                "the left-hand side and right-hand side of a set "
                "instruction must have the same type"
            );
        }
        insn = utils::make<SetInstruction>(operands[0], operands[1]);

    } else if (name == "wait") {

        // Build a wait instruction.
        auto wait_insn = utils::make<WaitInstruction>();
        if (operands.empty()) {
            QL_USER_ERROR(
                "wait instructions must have at least one "
                "operand (the duration)"
            );
        }
        if (auto ilit = operands[0]->as_int_literal()) {
            if (ilit->value < 0) {
                QL_USER_ERROR(
                    "the duration of a wait instruction cannot be negative"
                );
            }
            wait_insn->duration = (utils::UInt)ilit->value;
        } else {
            QL_USER_ERROR(
                "the duration of a wait instruction must be an integer literal"
            );
        }
        for (utils::UInt i = 1; i < operands.size(); i++) {
            auto ref = operands[i].as<Reference>();
            if (ref.empty()) {
                QL_USER_ERROR(
                    "the operands of a wait instruction after the first "
                    "must be references"
                );
            }
            wait_insn->objects.add(ref);
        }
        insn = wait_insn;

    } else if (name == "barrier") {

        // Build a barrier instruction.
        auto barrier_insn = utils::make<WaitInstruction>();
        for (const auto &operand : operands) {
            auto ref = operand.as<Reference>();
            if (ref.empty()) {
                QL_USER_ERROR(
                    "the operands of a wait instruction after the first "
                    "must be references"
                );
            }
            barrier_insn->objects.add(ref);
        }
        insn = barrier_insn;

    } else {

        // Build a custom instruction.
        auto custom_insn = utils::make<CustomInstruction>();
        custom_insn->operands = operands;

        // Find the type for the custom instruction.
        utils::Vec<DataTypeLink> types;
        for (const auto &operand : operands) {
            types.push_back(get_type_of(operand));
        }
        custom_insn->instruction_type = find_instruction_type(
            ir,
            name,
            types,
            generate_overload_if_needed
        );
        if (custom_insn->instruction_type.empty()) {
            if (return_empty_on_failure) {
                return {};
            }
            utils::StrStrm ss;
            ss << "unknown instruction: " << name;
            auto first = true;
            for (const auto &type : types) {
                if (first) {
                    first = false;
                } else {
                    ss << ",";
                }
                ss << " " << type->name;
            }
            QL_USER_ERROR(ss.str());
        }

        // Specialize the instruction type and operands as much as possible.
        utils::Bool specialization_found;
        do {
            specialization_found = false;
            for (const auto &spec : custom_insn->instruction_type->specializations) {
                if (spec->template_operands.back().equals(custom_insn->operands.front())) {
                    custom_insn->operands.remove(0);
                    custom_insn->instruction_type = spec;
                    specialization_found = true;
                    break;
                }
            }
        } while (specialization_found);

        insn = custom_insn;
    }

    // Set the condition, if applicable.
    if (auto cond_insn = insn->as_conditional_instruction()) {
        if (condition.empty()) {
            cond_insn->condition = make_bit_lit(ir, true);
        } else {
            cond_insn->condition = condition;
        }
    } else if (!condition.empty()) {
        QL_USER_ERROR(
            "condition specified for instruction that "
            "cannot be made conditional"
        );
    }

    // Return the constructed condition.
    return insn;
}

/**
 * Shorthand for making a set instruction.
 */
InstructionRef make_set_instruction(
    const Ref &ir,
    const ExpressionRef &lhs,
    const ExpressionRef &rhs,
    const ExpressionRef &condition
) {
    return make_instruction(ir, "set", {lhs, rhs}, condition);
}

/**
 * Adds a decomposition rule. An instruction is generated for the decomposition
 * rule based on instruction_type and template_operands if one didn't already
 * exist. If one did already exist, only the decompositions field of
 * instruction_type is used to extend the decomposition rule list of the
 * existing instruction type.
 */
InstructionTypeLink add_decomposition_rule(
    const Ref &ir,
    const utils::One<InstructionType> &instruction_type,
    const utils::Any<Expression> &template_operands
) {

    // Defer to add_or_find_instruction_type().
    auto result = add_or_find_instruction_type(ir, instruction_type, template_operands);

    // If we didn't add anything because a matching specialization of a matching
    // instruction already existed, just add the incoming decomposition rules to
    // it.
    if (!result.second) {
        result.first->decompositions.extend(instruction_type->decompositions);
    }

    return result.first;
}

/**
 * Adds a function type to the platform.
 */
FunctionTypeLink add_function_type(
    const Ref &ir,
    const utils::One<FunctionType> &function_type
) {

    // Check its name.
    if (
        !std::regex_match(function_type->name, IDENTIFIER_RE) &&
        !utils::starts_with(function_type->name, "operator")
    ) {
        QL_USER_ERROR(
            "invalid name for new function type: \"" <<
            function_type->name << "\" is not a valid identifier or operator"
        );
    }

    // Search for an existing matching function.
    auto begin = ir->platform->functions.get_vec().begin();
    auto end = ir->platform->functions.get_vec().end();
    auto pos = std::lower_bound(begin, end, function_type, compare_by_name<FunctionType>);
    for (; pos != end && (*pos)->name == function_type->name; ++pos) {
        if ((*pos)->operand_types.size() != function_type->operand_types.size()) {
            continue;
        }
        auto match = true;
        for (utils::UInt i = 0; i < (*pos)->operand_types.size(); i++) {
            if ((*pos)->operand_types[i]->data_type != function_type->operand_types[i]->data_type) {
                match = false;
                break;
            }
        }
        if (match) {
            QL_USER_ERROR(
                "duplicate function type: " << describe(function_type)
            );
        }
    }

    // Add the function type in the right place.
    ir->platform->functions.get_vec().insert(pos, function_type);

    return function_type;
}

/**
 * Finds a function type based on its name and operand types. If no matching
 * function type is found, an empty link is returned.
 */
FunctionTypeLink find_function_type(
    const Ref &ir,
    const utils::Str &name,
    const utils::Vec<DataTypeLink> &types
) {
    auto begin = ir->platform->functions.get_vec().begin();
    auto end = ir->platform->functions.get_vec().end();
    auto pos = std::lower_bound(
        begin,
        end,
        utils::make<FunctionType>(name),
        compare_by_name<FunctionType>
    );
    for (; pos != end && (*pos)->name == name; ++pos) {
        if ((*pos)->operand_types.size() != types.size()) {
            continue;
        }
        auto match = true;
        for (utils::UInt i = 0; i < (*pos)->operand_types.size(); i++) {
            if ((*pos)->operand_types[i]->data_type != types[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            return *pos;
        }
    }
    return {};
}

/**
 * Builds a new function call node based on the given name and operand list.
 */
utils::One<FunctionCall> make_function_call(
    const Ref &ir,
    const utils::Str &name,
    const utils::Any<Expression> &operands
) {

    // Build a function call node.
    auto function_call = utils::make<FunctionCall>();
    function_call->operands = operands;

    // Find the type for the custom function.
    utils::Vec<DataTypeLink> types;
    for (const auto &operand : operands) {
        types.push_back(get_type_of(operand));
    }
    function_call->function_type = find_function_type(ir, name, types);
    if (function_call->function_type.empty()) {
        utils::StrStrm ss;
        ss << "unknown function: " << name << "(";
        auto first = true;
        for (const auto &type : types) {
            if (first) {
                first = false;
            } else {
                ss << " ,";
            }
            ss << type->name;
        }
        ss << ")";
        QL_USER_ERROR(ss.str());
    }

    return function_call;
}

/**
 * Returns the number of qubits in the main qubit register.
 */
utils::UInt get_num_qubits(const Ref &ir) {
    QL_ASSERT(ir->platform->qubits->shape.size() == 1);
    return ir->platform->qubits->shape[0];
}

/**
 * Returns whether the given expression can be assigned or is a qubit (i.e.,
 * whether it can appear on the left-hand side of an assignment, or can be used
 * as an operand in classical write or qubit access mode).
 */
utils::Bool is_assignable_or_qubit(const ExpressionRef &expr) {
    if (expr->as_literal()) {
        return false;
    } else if (expr->as_reference()) {
        return true;
    } else if (expr->as_function_call()) {
        return false;
    } else {
        QL_ICE("unknown expression node type encountered");
    }
}

/**
 * Makes an integer literal using the given or default integer type.
 */
utils::One<IntLiteral> make_int_lit(
    const Ref &ir,
    utils::Int i,
    const DataTypeLink &type
) {
    auto typ = type;
    if (typ.empty()) {
        typ = ir->platform->default_int_type;
    }
    auto int_type = typ.as<IntType>();
    if (int_type.empty()) {
        QL_USER_ERROR(
            "type " + typ->name + " is not integer-like"
        );
    }
    if (i > get_max_int_for(*int_type) || i < get_min_int_for(*int_type)) {
        QL_USER_ERROR(
            "integer literal value out of range for default integer type"
        );
    }
    return utils::make<IntLiteral>(i, typ);
}

/**
 * Makes an integer literal using the given or default integer type.
 */
utils::One<IntLiteral> make_uint_lit(
    const Ref &ir,
    utils::UInt i,
    const DataTypeLink &type
) {
    auto typ = type;
    if (typ.empty()) {
        typ = ir->platform->default_int_type;
    }
    auto int_type = typ.as<IntType>();
    if (int_type.empty()) {
        QL_USER_ERROR(
            "type " << typ->name << " is not integer-like"
        );
    }
    if (i > (utils::UInt)get_max_int_for(*int_type)) {
        QL_USER_ERROR(
            "integer literal value out of range for default integer type"
        );
    }
    return utils::make<IntLiteral>((utils::UInt)i, typ);
}

/**
 * Makes an bit literal using the given or default bit type.
 */
utils::One<BitLiteral> make_bit_lit(
    const Ref &ir,
    utils::Bool b,
    const DataTypeLink &type
) {
    auto typ = type;
    if (typ.empty()) {
        typ = ir->platform->default_bit_type;
    }
    auto bit_type = typ.as<BitType>();
    if (bit_type.empty()) {
        QL_USER_ERROR(
            "type " + typ->name + " is not bit-like"
        );
    }
    return utils::make<BitLiteral>(b, typ);
}

/**
 * Makes a qubit reference to the main qubit register.
 */
utils::One<Reference> make_qubit_ref(const Ref &ir, utils::UInt idx) {
    return make_reference(ir, ir->platform->qubits, {idx});
}

/**
 * Makes a reference to the implicit measurement bit associated with a qubit in
 * the main qubit register.
 */
utils::One<Reference> make_bit_ref(const Ref &ir, utils::UInt idx) {
    if (ir->platform->implicit_bit_type.empty()) {
        QL_USER_ERROR(
            "platform does not support implicit measurement bits for qubits"
        );
    }
    auto ref = make_qubit_ref(ir, idx);
    ref->data_type = ir->platform->implicit_bit_type;
    return ref;
}

/**
 * Makes a reference to the specified object using literal indices.
 */
utils::One<Reference> make_reference(
    const Ref &ir,
    const ObjectLink &obj,
    utils::Vec<utils::UInt> indices
) {
    if (indices.size() > obj->shape.size()) {
        QL_USER_ERROR(
            "too many indices specified to make reference to '" + obj->name + "'"
        );
    } else if (indices.size() < obj->shape.size()) {
        QL_USER_ERROR(
            "not enough indices specified to make reference to '" + obj->name + "' "
            "(only individual elements can be referenced at this time)"
        );
    }
    auto ref = utils::make<Reference>(obj, obj->data_type);
    for (utils::UInt i = 0; i < indices.size(); i++) {
        if (indices[i] >= obj->shape[i]) {
            QL_USER_ERROR(
                "index out of range making reference to '" + obj->name + "'"
            );
        }
        ref->indices.add(make_uint_lit(ir, indices[i]));
    }
    return ref;
}

/**
 * Makes a temporary object with the given type.
 */
ObjectLink make_temporary(const Ref &ir, const DataTypeLink &data_type) {
    auto obj = utils::make<TemporaryObject>("", data_type);
    ir->program->objects.add(obj);
    return obj;
}

/**
 * Returns the duration of an instruction in quantum cycles. Note that this will
 * be zero for non-quantum instructions.
 */
utils::UInt get_duration_of_instruction(const InstructionRef &insn) {
    if (auto custom = insn->as_custom_instruction()) {
        return custom->instruction_type->duration;
    } else if (auto wait = insn->as_wait_instruction()) {
        return wait->duration;
    } else {
        return 0;
    }
}

/**
 * Returns the duration of a block in quantum cycles. If the block contains
 * structured control-flow sub-blocks, these are counted as zero cycles.
 */
utils::UInt get_duration_of_block(const BlockBaseRef &block) {

    // It is always necessary to iterate over the entire block, because the
    // first instruction might have a duration longer than the entire rest of
    // the block.
    utils::UInt duration = 0;
    for (const auto &stmt : block->statements) {
        auto insn = stmt.as<Instruction>();
        if (!insn.empty()) {
            duration = utils::max(
                duration,
                insn->cycle + get_duration_of_instruction(insn)
            );
        }
    }

    return duration;
}

/**
 * Returns whether an instruction is a quantum gate, by returning the number of
 * qubits in its operand list.
 */
utils::UInt is_quantum_gate(const InstructionRef &insn) {
    utils::UInt num_qubits = 0;
    if (auto custom = insn->as_custom_instruction()) {
        for (auto &otyp : custom->instruction_type->operand_types) {
            if (otyp->data_type->as_qubit_type()) {
                num_qubits++;
            }
        }
    }
    return num_qubits;
}

/**
 * Metadata for operators as they appear in cQASM (or just logically in
 * general). Used to avoid excessive parentheses when printing expressions.
 * The first element in the key pair is the function name, the second is the
 * number of operands.
 */
const utils::Map<utils::Pair<utils::Str, utils::UInt>, OperatorInfo> OPERATOR_INFO = {
    {{"operator?:",  3}, { 1, OperatorAssociativity::RIGHT, "",  " ? ",   " : "}},
    {{"operator||",  2}, { 2, OperatorAssociativity::LEFT,  "",  " || ",  ""}},
    {{"operator^^",  2}, { 3, OperatorAssociativity::LEFT,  "",  " ^^ ",  ""}},
    {{"operator&&",  2}, { 4, OperatorAssociativity::LEFT,  "",  " && ",  ""}},
    {{"operator|",   2}, { 5, OperatorAssociativity::LEFT,  "",  " | ",   ""}},
    {{"operator^",   2}, { 6, OperatorAssociativity::LEFT,  "",  " ^ ",   ""}},
    {{"operator&",   2}, { 7, OperatorAssociativity::LEFT,  "",  " & ",   ""}},
    {{"operator==",  2}, { 8, OperatorAssociativity::LEFT,  "",  " == ",  ""}},
    {{"operator!=",  2}, { 8, OperatorAssociativity::LEFT,  "",  " != ",  ""}},
    {{"operator<",   2}, { 9, OperatorAssociativity::LEFT,  "",  " < ",   ""}},
    {{"operator>",   2}, { 9, OperatorAssociativity::LEFT,  "",  " > ",   ""}},
    {{"operator<=",  2}, { 9, OperatorAssociativity::LEFT,  "",  " <= ",  ""}},
    {{"operator>=",  2}, { 9, OperatorAssociativity::LEFT,  "",  " >= ",  ""}},
    {{"operator<<",  2}, {10, OperatorAssociativity::LEFT,  "",  " << ",  ""}},
    {{"operator<<<", 2}, {10, OperatorAssociativity::LEFT,  "",  " <<< ", ""}},
    {{"operator>>",  2}, {10, OperatorAssociativity::LEFT,  "",  " >> ",  ""}},
    {{"operator>>>", 2}, {10, OperatorAssociativity::LEFT,  "",  " >>> ", ""}},
    {{"operator+",   2}, {11, OperatorAssociativity::LEFT,  "",  " + ",   ""}},
    {{"operator-",   2}, {11, OperatorAssociativity::LEFT,  "",  " - ",   ""}},
    {{"operator*",   2}, {12, OperatorAssociativity::LEFT,  "",  " * ",   ""}},
    {{"operator/",   2}, {12, OperatorAssociativity::LEFT,  "",  " / ",   ""}},
    {{"operator//",  2}, {12, OperatorAssociativity::LEFT,  "",  " // ",  ""}},
    {{"operator%",   2}, {12, OperatorAssociativity::LEFT,  "",  " % ",   ""}},
    {{"operator**",  2}, {13, OperatorAssociativity::RIGHT, "",  " ** ",  ""}},
    {{"operator-",   1}, {14, OperatorAssociativity::RIGHT, "-", "",      ""}},
    {{"operator+",   1}, {14, OperatorAssociativity::RIGHT, "+", "",      ""}},
    {{"operator~",   1}, {14, OperatorAssociativity::RIGHT, "~", "",      ""}},
    {{"operator!",   1}, {14, OperatorAssociativity::RIGHT, "!", "",      ""}},
};

/**
 * Describes visited nodes into the given stream. The description aims to be a
 * one-liner that's comprehensible to a user; for example, a function type node
 * returns its prototype. This makes it a lot more useful for error messages
 * than the dumper that's automatically generated by tree-gen. Note however that
 * no description is defined for things that are inherently multiline, like
 * blocks.
 *
 * FIXME: visitors are defined to have mutable access to the tree, even though
 *  that doesn't make sense in this context. So we act like everything is
 *  const, and do an ugly constness cast in the describe() function. Much better
 *  would be to just extend tree-gen to also generate a ConstVisitor variant,
 *  but this is more work than it's worth right now.
 */
class DescribingVisitor : public Visitor<void> {
protected:

    /**
     * Stream to write the node description to.
     */
    std::ostream &ss;

    /**
     * Precedence level of the current surrounding expression. All visit
     * functions should leave this variable the way they found it (exceptions
     * aside, things are assumed to irreparably break on exception anyway), but
     * they may modify it mid-function before recursively calling other visitor
     * functions. Only the visit_function_call() node does this and uses this,
     * though. The logic is that parentheses must be printed if the current
     * precedence level is greater than the precedence of the operator to be
     * printed.
     */
    utils::UInt precedence = 0;

public:

    /**
     * Constructs the visitor.
     */
    explicit DescribingVisitor(std::ostream &ss) : ss(ss) {};

    void visit_node(Node &node) override {
        ss << "<UNKNOWN>";
    }

    void visit_root(Root &root) override {
        if (root.program.empty()) {
            ss << "empty root";
        } else {
            ss << "root for ";
            root.program.visit(*this);
        }
    }

    void visit_platform(Platform &platform) override {
        if (platform.name.empty()) {
            ss << "anonymous platform";
        } else {
            ss << "platform " << platform.name;
        }
    }

    void visit_data_type(DataType &data_type) override {
        ss << data_type.name;
    }

protected:
    utils::Bool print_instruction_type_prefix(InstructionType &instruction_type) {
        ss << instruction_type.name;
        if (instruction_type.cqasm_name != instruction_type.name) {
            ss << "/" << instruction_type.cqasm_name;
        }
        auto first = true;
        if (!instruction_type.template_operands.empty()) {
            auto generalization = instruction_type.generalization;
            if (!generalization->generalization.empty()) {
                generalization = generalization->generalization;
            }
            for (utils::UInt i = 0; i < instruction_type.template_operands.size(); i++) {
                if (!first) ss << ",";
                first = false;
                ss << " ";
                generalization->operand_types[i].visit(*this);
                ss << "=";
                instruction_type.template_operands[i].visit(*this);
            }
        }
        return first;
    }

public:
    void visit_instruction_type(InstructionType &instruction_type) override {
        auto first = print_instruction_type_prefix(instruction_type);
        for (auto &opt : instruction_type.operand_types) {
            if (!first) ss << ",";
            first = false;
            ss << " ";
            opt.visit(*this);
        }
    }

    void visit_function_type(FunctionType &function_type) override {
        ss << function_type.name << "(";
        auto first = true;
        for (auto &opt : function_type.operand_types) {
            if (!first) ss << ", ";
            first = false;
            opt.visit(*this);
        }
        ss << ") -> ";
        function_type.return_type.visit(*this);
    }

    void visit_object(Object &object) override {
        if (object.name.empty()) {
            ss << "<anonymous>";
        } else {
            ss << object.name;
        }
        ss << ": ";
        object.data_type.visit(*this);
        if (!object.shape.empty()) {
            ss << "[";
            auto first = true;
            for (auto size : object.shape) {
                if (!first) ss << ", ";
                first = false;
                ss << size;
            }
            ss << "]";
        }
    }

    void visit_operand_type(OperandType &operand_type) override {
        switch (operand_type.mode) {
            case prim::AccessMode::WRITE: {
                if (!operand_type.data_type->as_qubit_type()) {
                    ss << "write ";
                }
                break;
            }
            case prim::AccessMode::READ:      ss << "read ";      break;
            case prim::AccessMode::LITERAL:   ss << "literal ";   break;
            case prim::AccessMode::COMMUTE_X: ss << "X-commute "; break;
            case prim::AccessMode::COMMUTE_Y: ss << "Y-commute "; break;
            case prim::AccessMode::COMMUTE_Z: ss << "Z-commute "; break;
            case prim::AccessMode::MEASURE:   ss << "measure ";   break;
            default: break;
        }
        operand_type.data_type.visit(*this);
    }

    void visit_program(Program &program) override {
        if (program.name.empty()) {
            ss << "anonymous program";
        } else {
            ss << "program " << program.name;
        }
    }

    void visit_block(Block &block) override {
        if (block.name.empty()) {
            ss << "anonymous block";
        } else {
            ss << "block " << block.name;
        }
    }

    void visit_conditional_instruction(ConditionalInstruction &conditional_instruction) override {
        if (
            !conditional_instruction.condition->as_bit_literal() ||
            !conditional_instruction.condition->as_bit_literal()->value
        ) {
            ss << "cond (";
            conditional_instruction.condition.visit(*this);
            ss << ") ";
        }
    }

    void visit_custom_instruction(CustomInstruction &custom_instruction) override {
        visit_conditional_instruction(custom_instruction);
        auto first = print_instruction_type_prefix(*custom_instruction.instruction_type);
        for (utils::UInt i = 0; i < custom_instruction.operands.size(); i++) {
            if (!first) ss << ",";
            first = false;
            ss << " ";
            custom_instruction.instruction_type->operand_types[i].visit(*this);
            ss << "=";
            custom_instruction.operands[i].visit(*this);
        }
    }

    void visit_set_instruction(SetInstruction &set_instruction) override {
        visit_conditional_instruction(set_instruction);
        set_instruction.lhs.visit(*this);
        ss << " = ";
        set_instruction.rhs.visit(*this);
    }

    void visit_goto_instruction(GotoInstruction &goto_instruction) override {
        visit_conditional_instruction(goto_instruction);
        ss << "goto ";
        goto_instruction.target.visit(*this);
    }

    void visit_wait_instruction(WaitInstruction &wait_instruction) override {
        ss << "wait";
        if (wait_instruction.duration) {
            ss << " " << wait_instruction.duration;
            if (wait_instruction.duration == 1) {
                ss << " cycle";
            } else {
                ss << " cycles";
            }
            if (!wait_instruction.objects.empty()) {
                ss << " after ";
            }
        } else if (!wait_instruction.objects.empty()) {
            ss << " on ";
        }
        auto first = true;
        for (auto &ref : wait_instruction.objects) {
            if (!first) ss << ",";
            first = false;
            ss << " ";
            ref.visit(*this);
        }
    }

    void visit_source_instruction(SourceInstruction &source_instruction) override {
        ss << "SOURCE";
    }

    void visit_sink_instruction(SinkInstruction &sink_instruction) override {
        ss << "SINK";
    }

    void visit_if_else(IfElse &if_else) override {
        ss << "if (";
        if_else.branches[0]->condition.visit(*this);
        ss << ") ...";
    }

    void visit_loop(Loop &loop) override {
        ss << "loop ...";
    }

    void visit_break_statement(BreakStatement &break_statement) override {
        ss << "break";
    }

    void visit_continue_statement(ContinueStatement &continue_statementn) override {
        ss << "continue";
    }

    void visit_bit_literal(BitLiteral &bit_literal) override {
        if (bit_literal.value) {
            ss << "true";
        } else {
            ss << "false";
        }
    }

    void visit_int_literal(IntLiteral &int_literal) override {
        ss << int_literal.value;
    }

    void visit_real_literal(RealLiteral &real_literal) override {
        ss << real_literal.value;
    }

    void visit_complex_literal(ComplexLiteral &complex_literal) override {
        ss << complex_literal.value;
    }

    void visit_real_matrix_literal(RealMatrixLiteral &real_matrix_literal) override {
        ss << real_matrix_literal.value;
    }

    void visit_complex_matrix_literal(ComplexMatrixLiteral &complex_matrix_literal) override {
        ss << complex_matrix_literal.value;
    }

    void visit_string_literal(StringLiteral &string_literal) override {
        auto esc = string_literal.value;
        esc = utils::replace_all(esc, "\\", "\\\\");
        esc = utils::replace_all(esc, "\"", "\\\"");
        ss << "\"" << esc << "\"";
    }

    void visit_json_literal(JsonLiteral &json_literal) override {
        ss << json_literal.value;
    }

    void visit_reference(Reference &reference) override {
        if (reference.data_type != reference.target->data_type) {
            ss << "(";
            reference.data_type.visit(*this);
            ss << ")";
        }
        if (reference.target->name.empty()) {
            ss << "<anonymous>";
        } else {
            ss << reference.target->name;
        }
        if (!reference.indices.empty()) {
            ss << "[";
            auto first = true;
            for (auto &index : reference.indices) {
                if (!first) ss << ", ";
                first = false;
                index.visit(*this);
            }
            ss << "]";
        }
    }

    void visit_function_call(FunctionCall &function_call) override {
        auto prev_precedence = precedence;
        auto op_inf = OPERATOR_INFO.find({
            function_call.function_type->name,
            function_call.operands.size()
        });
        if (op_inf == OPERATOR_INFO.end()) {

            // Reset precedence for the function operands.
            precedence = 0;
            ss << function_call.function_type->name << "(";
            auto first = true;
            for (auto &operand : function_call.operands) {
                if (!first) ss << ", ";
                first = false;
                operand.visit(*this);
            }
            ss << ")";

        } else {
            if (precedence > op_inf->second.precedence) {
                ss << "(";
            }

            ss << op_inf->second.prefix;
            if (function_call.operands.size() == 1) {

                // Print the only operand with this precedence level.
                // Associativity doesn't matter for unary operators because we
                // don't have postfix operators.
                precedence = op_inf->second.precedence;
                function_call.operands.front().visit(*this);

            } else if (function_call.operands.size() > 1) {

                // Print the first operand with this precedence level if
                // left-associative, or with one level higher precedence if
                // right-associative to force parentheses for equal precedence
                // in that case.
                precedence = op_inf->second.precedence;
                if (op_inf->second.associativity == OperatorAssociativity::RIGHT) {
                    precedence++;
                }
                function_call.operands.front().visit(*this);
                ss << op_inf->second.infix;

                // If this is a ternary operator, print the middle operand.
                // Always place parentheses around it in case it's another
                // operator with the same precedence; I don't think this is
                // actually necessary, but more readable in my opinion.
                if (function_call.operands.size() > 2) {
                    QL_ASSERT(function_call.operands.size() <= 3);
                    precedence = op_inf->second.precedence + 1;
                    function_call.operands[1].visit(*this);
                    ss << op_inf->second.infix2;
                }

                // Print the second operand with this precedence level if
                // right-associative, or with one level higher precedence if
                // left-associative to force parentheses for equal precedence
                // in that case.
                precedence = op_inf->second.precedence;
                if (op_inf->second.associativity == OperatorAssociativity::LEFT) {
                    precedence++;
                }
                function_call.operands.back().visit(*this);

            } else {
                QL_ASSERT(false);
            }

            precedence = prev_precedence;
            if (precedence > op_inf->second.precedence) {
                ss << ")";
            }
        }
        precedence = prev_precedence;
    }

};

/**
 * Gives a one-line description of a node.
 */
void describe(const Node &node, std::ostream &ss) {
    DescribingVisitor visitor(ss);

    // See fix-me note at top of DescribingVisitor. tl;dr visitors are defined
    // to always be mutable by tree-gen, but there's no reason for that here.
    const_cast<Node&>(node).visit(visitor);
}

/**
 * Gives a one-line description of a node.
 */
void describe(const utils::One<Node> &node, std::ostream &ss) {
    describe(*node, ss);
}

/**
 * Gives a one-line description of a node.
 */
utils::Str describe(const Node &node) {
    utils::StrStrm ss;
    describe(node, ss);
    return ss.str();
}

/**
 * Gives a one-line description of a node.
 */
utils::Str describe(const utils::One<Node> &node) {
    utils::StrStrm ss;
    describe(node, ss);
    return ss.str();
}

/**
 * Clones this wrapper (and its underlying reference object).
 */
ObjectAccesses::ReferenceWrapper ObjectAccesses::ReferenceWrapper::clone() const {
    return {*reference.clone().as<Reference>()};
}

/**
 * Dereference operator (shorthand).
 */
const Reference &ObjectAccesses::ReferenceWrapper::operator*() const {
    return reference;
}

/**
 * Dereference operator (shorthand).
 */
Reference &ObjectAccesses::ReferenceWrapper::operator*() {
    return reference;
}

/**
 * Dereference operator (shorthand).
 */
const Reference *ObjectAccesses::ReferenceWrapper::operator->() const {
    return &reference;
}

/**
 * Dereference operator (shorthand).
 */
Reference *ObjectAccesses::ReferenceWrapper::operator->() {
    return &reference;
}

/**
 * Less-than operator to allow this to be used as a key to a map.
 */
utils::Bool ObjectAccesses::ReferenceWrapper::operator<(const ReferenceWrapper &rhs) const {
    if (reference.target > rhs->target) return false;
    if (reference.target < rhs->target) return true;
    if (reference.data_type > rhs->data_type) return false;
    if (reference.data_type < rhs->data_type) return true;
    utils::UInt i = 0;
    while (true) {
        if (i >= rhs->indices.size()) return false;
        if (i >= reference.indices.size()) return true;
        if (reference.indices[i] > rhs->indices[i]) return false;
        if (reference.indices[i] < rhs->indices[i]) return true;
        i++;
    }
}

/**
 * Value-based equality operator.
 */
utils::Bool ObjectAccesses::ReferenceWrapper::operator==(const ReferenceWrapper &rhs) const {
    return reference.equals(*rhs);
}

/**
 * Returns the contained dependency list.
 */
const ObjectAccesses::Accesses &ObjectAccesses::get() const {
    return accesses;
}

/**
 * Adds a single object access. Literal access mode is upgraded to read
 * mode, as it makes no sense to access an object in literal mode (this
 * should never happen for consistent IRs though, unless this is explicitly
 * called this way). Measure access mode is upgraded to a write access to
 * both the qubit and the implicit bit associated with it. If there was
 * already an access for the object, the access mode is combined: if they
 * match the mode is maintained, otherwise the mode is changed to write.
 */
void ObjectAccesses::add_access(
    const Ref &ir,
    prim::AccessMode mode,
    const ReferenceWrapper &reference
) {
    if (mode == prim::AccessMode::LITERAL) {
        mode = prim::AccessMode::READ;
    } else if (mode == prim::AccessMode::MEASURE) {
        ReferenceWrapper copy = reference.clone();
        copy->data_type = ir->platform->implicit_bit_type;
        add_access(ir, prim::AccessMode::WRITE, copy);
        mode = prim::AccessMode::WRITE;
    }
    auto it = accesses.find(reference);
    if (it == accesses.end()) {
        accesses.insert(it, {reference, mode});
    } else if (it->second != mode) {
        it->second = prim::AccessMode::WRITE;
    }
}

/**
 * Adds dependencies on whatever is used by a complete expression.
 */
void ObjectAccesses::add_expression(
    const Ref &ir,
    prim::AccessMode mode,
    const ExpressionRef &expr
) {
    if (auto ref = expr->as_reference()) {
        add_access(ir, mode, {*ref});
    } else if (auto call = expr->as_function_call()) {
        add_operands(ir, call->function_type->operand_types, call->operands);
    }
}

/**
 * Adds dependencies on the operands of a function or instruction.
 */
void ObjectAccesses::add_operands(
    const Ref &ir,
    const utils::Any<OperandType> &prototype,
    const utils::Any<Expression> &operands
) {
    utils::UInt num_qubits = 0;
    for (auto &otyp : prototype) {
        if (otyp->data_type->as_qubit_type()) {
            num_qubits++;
        }
    }
    auto disable_qubit_commutation = (
        (num_qubits == 1 && disable_single_qubit_commutation) ||
        (num_qubits > 1 && disable_multi_qubit_commutation)
    );
    for (utils::UInt i = 0; i < prototype.size(); i++) {
        auto mode = prototype[i]->mode;
        if (disable_qubit_commutation) {
            switch (mode) {
                case prim::AccessMode::COMMUTE_X:
                case prim::AccessMode::COMMUTE_Y:
                case prim::AccessMode::COMMUTE_Z:
                    mode = prim::AccessMode::WRITE;
                default:
                    break;
            }
        }
        add_expression(ir, mode, operands[i]);
    }
}

/**
 * Adds dependencies for a complete statement.
 */
void ObjectAccesses::add_statement(const Ref &ir, const StatementRef &stmt) {
    auto barrier = false;
    if (auto cond = stmt->as_conditional_instruction()) {
        add_expression(ir, prim::AccessMode::READ, cond->condition);
        if (auto custom = stmt->as_custom_instruction()) {
            add_operands(ir, custom->instruction_type->operand_types, custom->operands);
            if (!custom->instruction_type->template_operands.empty()) {
                auto gen = custom->instruction_type;
                while (!gen->generalization.empty()) gen = gen->generalization;
                for (utils::UInt i = 0; i < custom->instruction_type->template_operands.size(); i++) {
                    add_expression(
                        ir,
                        gen->operand_types[i]->mode,
                        custom->instruction_type->template_operands[i]
                    );
                }
            }
        } else if (auto set = stmt->as_set_instruction()) {
            add_expression(ir, prim::AccessMode::WRITE, set->lhs);
            add_expression(ir, prim::AccessMode::READ, set->rhs);
        } else if (stmt->as_goto_instruction()) {
            barrier = true;
        } else {
            QL_ASSERT(false);
        }
    } else if (auto wait = stmt->as_wait_instruction()) {
        if (wait->objects.empty()) {
            barrier = true;
        } else {
            for (const auto &ref : wait->objects) {
                add_expression(ir, prim::AccessMode::WRITE, ref);
            }
        }
    } else if (stmt->as_dummy_instruction()) {
        barrier = true;
    } else if (auto if_else = stmt->as_if_else()) {
        for (const auto &branch : if_else->branches) {
            add_expression(ir, prim::AccessMode::READ, branch->condition);
            add_block(ir, branch->body);
        }
        if (!if_else->otherwise.empty()) {
            add_block(ir, if_else->otherwise);
        }
    } else if (auto loop = stmt->as_loop()) {
        add_block(ir, loop->body);
        if (auto stat = stmt->as_static_loop()) {
            add_expression(ir, prim::AccessMode::WRITE, stat->lhs);
        } else if (auto dyn = stmt->as_dynamic_loop()) {
            add_expression(ir, prim::AccessMode::READ, dyn->condition);
            if (auto forl = stmt->as_for_loop()) {
                add_statement(ir, forl->initialize);
                add_statement(ir, forl->update);
            } else if (stmt->as_repeat_until_loop()) {
                // no further dependencies
            } else {
                QL_ASSERT(false);
            }
        } else {
            QL_ASSERT(false);
        }
    } else if (stmt->as_loop_control_statement()) {
        barrier = true;
    } else {
        QL_ASSERT(false);
    }

    // Generate data dependencies for barrier-like instructions. Instructions
    // can shift around between barriers (as read accesses commute), but they
    // cannot cross a barrier, and barriers themselves cannot commute.
    add_access(ir, barrier ? prim::AccessMode::WRITE : prim::AccessMode::READ, {});

}

/**
 * Adds dependencies for a whole (sub)block of statements.
 */
void ObjectAccesses::add_block(const Ref &ir, const SubBlockRef &block) {
    for (const auto &stmt : block->statements) {
        add_statement(ir, stmt);
    }
}

/**
 * Clears the dependency list, allowing the object to be reused.
 */
void ObjectAccesses::reset() {
    accesses.clear();
}

/**
 * Constructs a remapper.
 */
ReferenceRemapper::ReferenceRemapper(Map &&map) : map(std::move(map)) {
}

/**
 * Constructs a remapper.
 */
ReferenceRemapper::ReferenceRemapper(const Map &map) : map(map) {
}

/**
 * The visit function that actually implements the remapping.
 */
void ReferenceRemapper::visit_reference(Reference &node) {
    auto it = map.find(node.target);
    if (it != map.end()) {
        node.target = it->second;
    }
}

} // namespace ir
} // namespace ql
