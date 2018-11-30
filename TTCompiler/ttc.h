/*
 * Tilemap Town scripting compiler
 *
 * Copyright (C) 2018 NovaSquirrel
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

// Data structure for a symbol table entry
struct symbol_data {
	const char *lexeme;
	int token_category;
	struct symbol_data *next;
};

// Data structure for a token
struct lexeme_token {
	int token_category;         // which token category
	int token_value;            // which token in the token category
	struct symbol_data *symbol; // symbol table entry
	struct lexeme_token *next;
};

// Node for the syntax tree
struct syntax_node {
  struct lexeme_token *token;
  struct syntax_node *child, *next;
};

enum token_category {
	t_identifier,    // an identifier
	t_integer,       // any integer
	t_real,          // any real number
	t_string,        // a string literal
	t_addsub,        // +, -
	t_muldiv,        // *, /, %
	t_logical,       // < > >= <= == <>
	t_shift,         // << >>
	t_bitmath,       // & | ^
	t_unary,         // ! ~
	t_lparen,        // (
	t_rparen,        // )
	t_lcurly,        // {
	t_rcurly,        // }
	t_lsquare,       // [
	t_rsquare,       // ]
	t_assignment,    // =
	t_comma,         // ,
	t_colon,         // :

	t_if,
	t_else,
	t_elif,
	t_until,
	t_while,
	t_for,
	t_in,
	t_step,

	t_to,
	t_continue,
	t_break,
	t_none,          // the "none" value

	t_def,
	t_var,
	t_true,
	t_false,
	t_return,

	t_newline,
	t_indent_in,
	t_indent_out,

	t_max_tokens,

	t_first_keyword = t_if,
	t_last_keyword = t_return,
};

struct lexeme_token *lexical_analyzer(FILE *File);
void syntactical_analyzer(struct lexeme_token *list);
void convert_indents(struct lexeme_token *list);
void error(const char *format, ...);
const char *token_print(struct lexeme_token *token);

extern const char *token_strings[t_max_tokens][20];
extern struct symbol_data *symbol_table;
extern struct lexeme_token *token_list;
extern struct lexeme_token *token_current;
extern struct syntax_node *tree_head;
extern struct syntax_node *tree_current;

