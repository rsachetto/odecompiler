////
//// Created by sachetto on 06/10/17.
////
#include <criterion/criterion.h>
#include <sys/mman.h>
#include <criterion/internal/assert.h>
#include <signal.h>
#include "../src/file_utils/file_utils.h"
#include "../src/compiler/lexer.h"
#include "../src/compiler/parser.h"
#include "../src/code_converter.h"
#include "../src/stb/stb_ds.h"

program create_parse_program(char *input) {
    lexer *l = new_lexer(input, "test");
    parser *p = new_parser(l);
    program prog = parse_program(p, false, false, NULL);

    cr_assert(prog != NULL);

    return prog;

}

void test_identifier(ast *ident, char *expected) {
    if(ident->tag == ast_call_expression) {
        cr_assert_str_eq(expected, ident->call_expr.function_identifier->identifier.value);
    } else {
        cr_assert_str_eq(expected, ident->identifier.value);
    }
}

void test_literal_expression(ast *exp, void *expected) {

    switch(exp->tag) {
        case ast_identifier:
        {
            char *value = (char*) expected;
            test_identifier(exp, value);
            break;
        }
        case ast_number_literal: {
            int value = *((int*)expected);
            cr_assert_eq(exp->num_literal.value, value);
            break;
        }
    }
}

void test_infix_expression(ast *exp, void *l, char *op, void *r) {

    cr_assert_eq(exp->tag, ast_infix_expression);

    test_literal_expression(exp->infix_expr.left, l);
    cr_assert_str_eq(exp->infix_expr.op, op);
    test_literal_expression(exp->infix_expr.right, r);

}

Test(compiler, ToRORd) {

    size_t size_expected;
    size_t size_converted;

    const char *source_file = "ToRORd.ode";

    size_t file_size;
    const char *source = read_entire_file_with_mmap(source_file, &file_size);

    lexer *l = new_lexer(source, source_file);
    parser *p = new_parser(l);
    program program = parse_program(p, true, true, NULL);

    check_parser_errors(p, true);

    const char *out_file = "converted.c";

    FILE *outfile = fopen(out_file, "w");

    convert_to_c(program, outfile, EULER_ADPT_SOLVER);
    free_lexer(l);
    free_parser(p);
    free_program(program);
    fclose(outfile);

    char *expected = read_entire_file_with_mmap("ToRORd.c", &size_expected);
    char *converted = read_entire_file_with_mmap(out_file, &size_converted);

    cr_expect_eq(size_expected, size_converted);

    munmap(expected, size_expected);
    munmap(converted, size_converted);

}

Test(parser, return_statement) {

    char * input = "return 5 \
                    return 10 \
                    return 993322";


    program prog = create_parse_program(input);

    int len = arrlen(prog);

    cr_expect_eq(len, 3);

    for(int i = 0; i < len; i++) {
        ast *a = prog[i];
        cr_assert_eq(a->tag, ast_return_stmt);

        char *literal = strndup(a->token.literal, a->token.literal_len);

        cr_assert_str_eq(literal, "return");

    }

}

Test(parser, assignment_statement) {
    char *input = "foobar = 1";

    program prog = create_parse_program(input);

    int len = arrlen(prog);

    cr_assert_eq(len, 1);

    ast *a = prog[0];

    cr_assert_eq(a->tag, ast_assignment_stmt);

    char *literal = a->assignment_stmt.name->identifier.value;
    cr_assert_str_eq(literal, "foobar");

    cr_assert_eq(a->assignment_stmt.value->num_literal.value, 1);
}

Test(parser, literal_expression) {
    char *input = "5";

    program prog = create_parse_program(input);

    int len = arrlen(prog);

    cr_assert_eq(len, 1);

    ast *a = prog[0];

    cr_assert_eq(a->tag, ast_expression_stmt);
    cr_assert_eq(a->expr_stmt->tag, ast_number_literal);

    cr_assert_eq(a->expr_stmt->num_literal.value, 5);
}

Test(parser, prefix_expressions) {


    struct {
        char *input;
        char *operator;

        union {
            int int_val;
            bool bool_val;
        };

    } prefix_tests[] = {{"!5", "!", {5}}, {"-15", "-", {15}}, {"!true", "!", {true}}, {"!false", "!", {false}}};


    for(int i = 0; i < 4; i++) {
        program prog = create_parse_program(prefix_tests[i].input);

        int len = arrlen(prog);

        ast *a = prog[0];

        cr_assert_eq(len, 1);
        cr_assert_eq(a->tag, ast_expression_stmt);
        cr_assert_eq(a->expr_stmt->tag, ast_prefix_expression);

        cr_assert_str_eq(a->expr_stmt->prefix_expr.op , prefix_tests[i].operator);
        if(i < 2)
            cr_assert_eq(a->expr_stmt->prefix_expr.right->num_literal.value , prefix_tests[i].int_val);
        else
            cr_assert_eq(a->expr_stmt->prefix_expr.right->bool_literal.value , prefix_tests[i].int_val);

        free_program(prog);
    }

}

Test(parser, if_expression) {
    char *input = "x = 1\ny = 2\nif (x < y) { x }";

    program prog = create_parse_program(input);

    int len = arrlen(prog);
    cr_assert_eq(len, 3);

    ast *a = prog[2];
    cr_assert_eq(a->tag, ast_expression_stmt);
    cr_assert_eq(a->expr_stmt->tag, ast_if_expr);

    test_infix_expression(a->expr_stmt->if_expr.condition, "x", "<", "y");

    cr_assert_eq(arrlen(a->expr_stmt->if_expr.consequence), 1);

    ast *c = a->expr_stmt->if_expr.consequence[0];
    cr_assert_eq(c->tag, ast_expression_stmt);

    test_identifier(c->expr_stmt, "x");

    cr_assert_null(a->expr_stmt->if_expr.alternative);

}

Test(parser, if_else_expression) {
    char *input = "x = 1\ny = 2\nif (x < y) { x } else {y}";

    program prog = create_parse_program(input);

    int len = arrlen(prog);
    cr_assert_eq(len, 3);

    ast *a = prog[2];
    cr_assert_eq(a->tag, ast_expression_stmt);
    cr_assert_eq(a->expr_stmt->tag, ast_if_expr);

    test_infix_expression(a->expr_stmt->if_expr.condition, "x", "<", "y");

    cr_assert_eq(arrlen(a->expr_stmt->if_expr.consequence), 1);

    ast *c = a->expr_stmt->if_expr.consequence[0];
    cr_assert_eq(c->tag, ast_expression_stmt);
    test_identifier(c->expr_stmt, "x");

    ast *alt = a->expr_stmt->if_expr.alternative[0];
    cr_assert_eq(alt->tag, ast_expression_stmt);
    test_identifier(alt->expr_stmt, "y");

}

Test(parser, function_literal) {
    char *input = "fn func(x, y) {x + y}";

    program prog = create_parse_program(input);

    int len = arrlen(prog);

    cr_expect_eq(len, 1);

    ast *a = prog[0];

    cr_expect_eq(a->tag, ast_function_statement);
    cr_expect_eq(arrlen(a->function_stmt.parameters), 2);

    test_literal_expression(a->function_stmt.parameters[0], "x");
    test_literal_expression(a->function_stmt.parameters[1], "y");

    cr_expect_eq(arrlen(a->function_stmt.body), 1);
    cr_expect_eq(a->function_stmt.body[0]->tag, ast_expression_stmt);

    test_infix_expression(a->function_stmt.body[0]->expr_stmt, "x", "+", "y");

}

//TODO: TestFunctionParameterParsing
Test(parser, function_parameter) {
}

Test(parser, call_expression) {

    char *input = "add(1, 2 * 3, 4 + 5)";

    program prog = create_parse_program(input);
    int len = arrlen(prog);
    cr_expect_eq(len, 1);

    ast *a = prog[0];
    cr_assert_eq(a->tag, ast_expression_stmt);
    cr_assert_eq(a->expr_stmt->tag, ast_call_expression);

    test_identifier(a->expr_stmt, "add");
    cr_expect_eq(arrlen(a->expr_stmt->call_expr.arguments), 3);

    int l = 1;
    int r;
    test_literal_expression(a->expr_stmt->call_expr.arguments[0], (void*)&l);

    l = 2;
    r = 3;
    test_infix_expression(a->expr_stmt->call_expr.arguments[1], (void*)&l, "*", (void*)&r);

    l = 4;
    r = 5;
    test_infix_expression(a->expr_stmt->call_expr.arguments[2], (void*)&l, "+", (void*)&r);

}

