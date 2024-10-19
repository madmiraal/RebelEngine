// SPDX-FileCopyrightText: 2023 Rebel Engine contributors
// SPDX-FileCopyrightText: 2014-2022 Godot Engine contributors
// SPDX-FileCopyrightText: 2007-2014 Juan Linietsky, Ariel Manzur
//
// SPDX-License-Identifier: MIT

#ifndef REBELSCRIPT_COMPILER_H
#define REBELSCRIPT_COMPILER_H

#include "core/set.h"
#include "rebelscript.h"
#include "rebelscript_parser.h"

class RebelScriptCompiler {
    const RebelScriptParser* parser;
    Set<RebelScript*> parsed_classes;
    Set<RebelScript*> parsing_classes;
    RebelScript* main_script;

    struct CodeGen {
        RebelScript* script;
        const RebelScriptParser::ClassNode* class_node;
        const RebelScriptParser::FunctionNode* function_node;
        bool debug_stack;

        List<Map<StringName, int>> stack_id_stack;
        Map<StringName, int> stack_identifiers;

        List<RebelScriptFunction::StackDebug> stack_debug;
        List<Map<StringName, int>> block_identifier_stack;
        Map<StringName, int> block_identifiers;

        void add_stack_identifier(const StringName& p_id, int p_stackpos) {
            stack_identifiers[p_id] = p_stackpos;
            if (debug_stack) {
                block_identifiers[p_id] = p_stackpos;
                RebelScriptFunction::StackDebug sd;
                sd.added      = true;
                sd.line       = current_line;
                sd.identifier = p_id;
                sd.pos        = p_stackpos;
                stack_debug.push_back(sd);
            }
        }

        void push_stack_identifiers() {
            stack_id_stack.push_back(stack_identifiers);
            if (debug_stack) {
                block_identifier_stack.push_back(block_identifiers);
                block_identifiers.clear();
            }
        }

        void pop_stack_identifiers() {
            stack_identifiers = stack_id_stack.back()->get();
            stack_id_stack.pop_back();

            if (debug_stack) {
                for (Map<StringName, int>::Element* E =
                         block_identifiers.front();
                     E;
                     E = E->next()) {
                    RebelScriptFunction::StackDebug sd;
                    sd.added      = false;
                    sd.identifier = E->key();
                    sd.line       = current_line;
                    sd.pos        = E->get();
                    stack_debug.push_back(sd);
                }
                block_identifiers = block_identifier_stack.back()->get();
                block_identifier_stack.pop_back();
            }
        }

        HashMap<Variant, int, VariantHasher, VariantComparator> constant_map;
        Map<StringName, int> name_map;
#ifdef TOOLS_ENABLED
        Vector<StringName> named_globals;
#endif

        int get_name_map_pos(const StringName& p_identifier) {
            int ret;
            if (!name_map.has(p_identifier)) {
                ret                    = name_map.size();
                name_map[p_identifier] = ret;
            } else {
                ret = name_map[p_identifier];
            }
            return ret;
        }

        int get_constant_pos(const Variant& p_constant) {
            if (constant_map.has(p_constant)) {
                return constant_map[p_constant];
            }
            int pos                  = constant_map.size();
            constant_map[p_constant] = pos;
            return pos;
        }

        Vector<int> opcodes;

        void alloc_stack(int p_level) {
            if (p_level >= stack_max) {
                stack_max = p_level + 1;
            }
        }

        void alloc_call(int p_params) {
            if (p_params >= call_max) {
                call_max = p_params;
            }
        }

        int current_line;
        int stack_max;
        int call_max;
    };

    bool _is_class_member_property(CodeGen& codegen, const StringName& p_name);
    bool _is_class_member_property(
        RebelScript* owner,
        const StringName& p_name
    );

    void _set_error(
        const String& p_error,
        const RebelScriptParser::Node* p_node
    );

    bool _create_unary_operator(
        CodeGen& codegen,
        const RebelScriptParser::OperatorNode* on,
        Variant::Operator op,
        int p_stack_level
    );
    bool _create_binary_operator(
        CodeGen& codegen,
        const RebelScriptParser::OperatorNode* on,
        Variant::Operator op,
        int p_stack_level,
        bool p_initializer = false,
        int p_index_addr   = 0
    );

    RebelScriptDataType _gdtype_from_datatype(
        const RebelScriptParser::DataType& p_datatype,
        RebelScript* p_owner = nullptr
    ) const;

    int _parse_assign_right_expression(
        CodeGen& codegen,
        const RebelScriptParser::OperatorNode* p_expression,
        int p_stack_level,
        int p_index_addr = 0
    );
    int _parse_expression(
        CodeGen& codegen,
        const RebelScriptParser::Node* p_expression,
        int p_stack_level,
        bool p_root        = false,
        bool p_initializer = false,
        int p_index_addr   = 0
    );
    Error _parse_block(
        CodeGen& codegen,
        const RebelScriptParser::BlockNode* p_block,
        int p_stack_level   = 0,
        int p_break_addr    = -1,
        int p_continue_addr = -1
    );
    Error _parse_function(
        RebelScript* p_script,
        const RebelScriptParser::ClassNode* p_class,
        const RebelScriptParser::FunctionNode* p_func,
        bool p_for_ready = false
    );
    Error _parse_class_level(
        RebelScript* p_script,
        const RebelScriptParser::ClassNode* p_class,
        bool p_keep_state
    );
    Error _parse_class_blocks(
        RebelScript* p_script,
        const RebelScriptParser::ClassNode* p_class,
        bool p_keep_state
    );
    void _make_scripts(
        RebelScript* p_script,
        const RebelScriptParser::ClassNode* p_class,
        bool p_keep_state
    );
    int err_line;
    int err_column;
    StringName source;
    String error;

public:
    Error compile(
        const RebelScriptParser* p_parser,
        RebelScript* p_script,
        bool p_keep_state = false
    );

    String get_error() const;
    int get_error_line() const;
    int get_error_column() const;

    RebelScriptCompiler();
};

#endif // REBELSCRIPT_COMPILER_H
