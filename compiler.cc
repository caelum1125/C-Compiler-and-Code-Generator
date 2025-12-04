/*
 * compiler.cc
 *
 *  Created on: Apr 28, 2025
 *      Author: caelumterrell
 */
#include "lexer.h"
#include "execute.h"
#include <map>
#include <string>
#include <vector>
#include <iostream>

using namespace std;

// Global Variables
LexicalAnalyzer lexer;
map<string, int> location_table;
int next_mem_location = 0;

// Helper Functions
void syntax_error() {
    cout << "SYNTAX ERROR" << endl;
    exit(1);
}

Token expect(TokenType expected_type) {
    Token t = lexer.GetToken();
    if (t.token_type != expected_type) {
        syntax_error();
    }
    return t;
}

bool is_token(TokenType type) {
    Token t = lexer.peek(1);
    return t.token_type == type;
}

int location(string var_name) {
    if (location_table.find(var_name) == location_table.end()) {
        syntax_error();
    }
    return location_table[var_name];
}

// Forward declarations
InstructionNode* parse_program();
InstructionNode* parse_var_section();
void parse_inputs();
InstructionNode* parse_body();
InstructionNode* parse_stmt_list();
InstructionNode* parse_stmt();
InstructionNode* parse_condition(ConditionalOperatorType &op_type, int &op1, int &op2);

// Top-level entry
InstructionNode* parse_Generate_Intermediate_Representation() {
    InstructionNode* program = parse_program();
    return program;
}

InstructionNode* parse_program() {
    parse_var_section();
    InstructionNode* body = parse_body();
    parse_inputs();
    return body;
}

InstructionNode* parse_var_section() {
    while (true) {
        Token id = expect(ID);
        location_table[id.lexeme] = next_mem_location++;
        if (is_token(COMMA)) {
            expect(COMMA);
        } else {
            break;
        }
    }
    expect(SEMICOLON);
    return nullptr;
}

InstructionNode* parse_body() {
    expect(LBRACE);
    InstructionNode* inst_list = parse_stmt_list();
    expect(RBRACE);
    return inst_list;
}

void parse_inputs() {
    Token num = expect(NUM);
    inputs.push_back(stoi(num.lexeme));
    while (is_token(NUM)) {
        num = expect(NUM);
        inputs.push_back(stoi(num.lexeme));
    }
}

InstructionNode* parse_stmt_list() {
    InstructionNode* inst1 = parse_stmt();
    if (is_token(ID) || is_token(WHILE) || is_token(IF) || is_token(SWITCH) || is_token(FOR) || is_token(INPUT) || is_token(OUTPUT)) {
        InstructionNode* inst2 = parse_stmt_list();
        InstructionNode* temp = inst1;
        while (temp->next != nullptr) {
            temp = temp->next;
        }
        temp->next = inst2;
    }
    return inst1;
}

InstructionNode* parse_stmt() {
    Token t = lexer.peek(1);
    if (t.token_type == ID) {
        Token lhs = expect(ID);
        expect(EQUAL);
        Token rhs = lexer.peek(1);

        InstructionNode* inst = new InstructionNode;
        inst->type = ASSIGN;
        inst->assign_inst.lhs_loc = location(lhs.lexeme);

        Token primary1 = lexer.GetToken();
        if (is_token(PLUS) || is_token(MINUS) || is_token(MULT) || is_token(DIV)) {
            Token op = lexer.GetToken();
            Token primary2 = lexer.GetToken();

            inst->assign_inst.op1_loc = (primary1.token_type == ID) ? location(primary1.lexeme) : next_mem_location;
            if (primary1.token_type == NUM) mem[next_mem_location++] = stoi(primary1.lexeme);

            inst->assign_inst.op2_loc = (primary2.token_type == ID) ? location(primary2.lexeme) : next_mem_location;
            if (primary2.token_type == NUM) mem[next_mem_location++] = stoi(primary2.lexeme);

            if (op.token_type == PLUS) inst->assign_inst.op = OPERATOR_PLUS;
            else if (op.token_type == MINUS) inst->assign_inst.op = OPERATOR_MINUS;
            else if (op.token_type == MULT) inst->assign_inst.op = OPERATOR_MULT;
            else if (op.token_type == DIV) inst->assign_inst.op = OPERATOR_DIV;
        } else {
            inst->assign_inst.op = OPERATOR_NONE;
            inst->assign_inst.op1_loc = (primary1.token_type == ID) ? location(primary1.lexeme) : next_mem_location;
            if (primary1.token_type == NUM) mem[next_mem_location++] = stoi(primary1.lexeme);
            inst->assign_inst.op2_loc = -1;
        }

        expect(SEMICOLON);
        inst->next = nullptr;
        return inst;
    } else if (t.token_type == INPUT) {
        expect(INPUT);
        Token id = expect(ID);
        expect(SEMICOLON);

        InstructionNode* inst = new InstructionNode;
        inst->type = IN;
        inst->input_inst.var_loc = location(id.lexeme);
        inst->next = nullptr;
        return inst;
    } else if (t.token_type == OUTPUT) {
        expect(OUTPUT);
        Token id = expect(ID);
        expect(SEMICOLON);

        InstructionNode* inst = new InstructionNode;
        inst->type = OUT;
        inst->output_inst.var_loc = location(id.lexeme);
        inst->next = nullptr;
        return inst;
    } else if (t.token_type == IF) {
        expect(IF);

        InstructionNode* if_node = new InstructionNode;
        if_node->type = CJMP;
        parse_condition(if_node->cjmp_inst.condition_op, if_node->cjmp_inst.op1_loc, if_node->cjmp_inst.op2_loc);

        InstructionNode* body = parse_body();

        InstructionNode* noop = new InstructionNode;
        noop->type = NOOP;
        noop->next = nullptr;

        InstructionNode* temp = body;
        while (temp->next != nullptr) temp = temp->next;
        temp->next = noop;

        if_node->next = body;
        if_node->cjmp_inst.target = noop;

        return if_node;
    } else if (t.token_type == WHILE) {
        expect(WHILE);

        InstructionNode* while_node = new InstructionNode;
        while_node->type = CJMP;
        parse_condition(while_node->cjmp_inst.condition_op, while_node->cjmp_inst.op1_loc, while_node->cjmp_inst.op2_loc);

        InstructionNode* body = parse_body();

        InstructionNode* jump = new InstructionNode;
        jump->type = JMP;
        jump->jmp_inst.target = while_node;
        jump->next = nullptr;

        InstructionNode* noop = new InstructionNode;
        noop->type = NOOP;
        noop->next = nullptr;

        InstructionNode* temp = body;
        while (temp->next != nullptr) temp = temp->next;
        temp->next = jump;

        while_node->next = body;
        jump->next = noop;
        while_node->cjmp_inst.target = noop;

        return while_node;
    } else if (t.token_type == FOR) {
        expect(FOR);
        expect(LPAREN);

        InstructionNode* init = parse_stmt();
        ConditionalOperatorType cond_op;
        int op1, op2;
        parse_condition(cond_op, op1, op2);
        expect(SEMICOLON);
        InstructionNode* update = parse_stmt();
        expect(RPAREN);

        InstructionNode* cond = new InstructionNode;
        cond->type = CJMP;
        cond->cjmp_inst.condition_op = cond_op;
        cond->cjmp_inst.op1_loc = op1;
        cond->cjmp_inst.op2_loc = op2;

        InstructionNode* body = parse_body();

        InstructionNode* jump = new InstructionNode;
        jump->type = JMP;
        jump->jmp_inst.target = cond;
        jump->next = nullptr;

        InstructionNode* noop = new InstructionNode;
        noop->type = NOOP;
        noop->next = nullptr;

        InstructionNode* temp = body;
        while (temp->next != nullptr) temp = temp->next;
        temp->next = update;

        update->next = jump;
        cond->next = body;
        jump->next = noop;
        cond->cjmp_inst.target = noop;

        init->next = cond;
        return init;
        } else if (t.token_type == SWITCH) {
        expect(SWITCH);
        Token var = expect(ID);
        expect(LBRACE);

        // Collect all cases without wiring yet
        struct CaseBlock { InstructionNode* cjmp; InstructionNode* body; };
        vector<CaseBlock> case_blocks;

        while (is_token(CASE)) {
            expect(CASE);
            Token num = expect(NUM);
            expect(COLON);

            // Create the conditional jump (test var != num)
            InstructionNode* cjmp = new InstructionNode;
            cjmp->type = CJMP;
            cjmp->cjmp_inst.condition_op = CONDITION_NOTEQUAL;
            cjmp->cjmp_inst.op1_loc = location(var.lexeme);
            mem[next_mem_location] = stoi(num.lexeme);
            cjmp->cjmp_inst.op2_loc = next_mem_location++;
            cjmp->next = nullptr;  // to be set in wiring loop

            // Parse the body of the case
            InstructionNode* body = nullptr;
            if (is_token(LBRACE)) body = parse_body();
            else                    body = parse_stmt();

            case_blocks.push_back({cjmp, body});
        }

        // Parse optional DEFAULT
        InstructionNode* default_case = nullptr;
        if (is_token(DEFAULT)) {
            expect(DEFAULT);
            expect(COLON);
            if (is_token(LBRACE)) default_case = parse_body();
            else                  default_case = parse_stmt();
        }

        expect(RBRACE);

        // Create a noop label for exit
        InstructionNode* noop = new InstructionNode;
        noop->type = NOOP;
        noop->next = nullptr;

        // Append a JMP->noop at the end of each case body
        for (auto& block : case_blocks) {
            InstructionNode* tail = block.body;
            while (tail->next) tail = tail->next;
            InstructionNode* jump = new InstructionNode;
            jump->type = JMP;
            jump->jmp_inst.target = noop;
            jump->next = nullptr;
            tail->next = jump;
        }

        // Append noop at end of default body, if present
        if (default_case) {
            InstructionNode* tail = default_case;
            while (tail->next) tail = tail->next;
            tail->next = noop;
        }

        // Wire CJMPs: on var!=num go to next test, on var==num enter body
        for (size_t i = 0; i < case_blocks.size(); ++i) {
            InstructionNode* cjmp = case_blocks[i].cjmp;
            InstructionNode* body = case_blocks[i].body;
            InstructionNode* skip = (i + 1 < case_blocks.size())
                ? case_blocks[i+1].cjmp
                : (default_case ? default_case : noop);
            cjmp->next = skip;
            cjmp->cjmp_inst.target = body;
        }

        // Entry point: first CJMP or default/noop if no cases
        return case_blocks.empty()
            ? (default_case ? default_case : noop)
            : case_blocks[0].cjmp;
    } else {
        syntax_error();
        return nullptr;
    }

}
InstructionNode* parse_condition(ConditionalOperatorType &op_type, int &op1, int &op2) {
    Token left = lexer.GetToken();
    Token relop = lexer.GetToken();
    Token right = lexer.GetToken();

    if (relop.token_type != GREATER && relop.token_type != LESS && relop.token_type != NOTEQUAL) {
        syntax_error();
    }

    if (left.token_type != ID && left.token_type != NUM) syntax_error();
    if (right.token_type != ID && right.token_type != NUM) syntax_error();

    op1 = (left.token_type == ID) ? location(left.lexeme) : next_mem_location;
    if (left.token_type == NUM) mem[next_mem_location++] = stoi(left.lexeme);

    op2 = (right.token_type == ID) ? location(right.lexeme) : next_mem_location;
    if (right.token_type == NUM) mem[next_mem_location++] = stoi(right.lexeme);

    if (relop.token_type == GREATER) op_type = CONDITION_GREATER;
    else if (relop.token_type == LESS) op_type = CONDITION_LESS;
    else if (relop.token_type == NOTEQUAL) op_type = CONDITION_NOTEQUAL;

    return nullptr;
}






