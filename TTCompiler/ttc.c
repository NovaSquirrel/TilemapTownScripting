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

// ----- FLOAT CHECKER -----
enum {
  FStart,      // starting state
  FSign,       // + or - at the state
  FInteger,    // the integer amount
  FPoint,      // the decimal point
  FFraction,   // the fractional amount
  FExpMark,    // the E before an exponent amount
  FExpSign,    // + or - before the exponent amount
  FExpInteger, // exponent amount
  FError,      // error
};

struct float_state {
  int plus_minus, is_digit, decimal_point, e, valid;
} float_state_machine[] = {
//  +/-       0-9          .       E     valid
  {FSign,    FInteger,    FError, FError,   0}, // Start
  {FError,   FInteger,    FError, FError,   0}, // Sign
  {FError,   FInteger,    FPoint, FExpMark, 2}, // Integer
  {FError,   FFraction,   FError, FError,   0}, // Point
  {FError,   FFraction,   FError, FExpMark, 1}, // Fraction
  {FExpSign, FExpInteger, FError, FError,   0}, // ExpMark
  {FError,   FExpInteger, FError, FError,   0}, // ExpSign
  {FError,   FExpInteger, FError, FError,   1}, // ExpInteger
  {FError,   FError,      FError, FError,   0}, // Dead
};

// Returns 1 if float, 2 if integer, or 0 if neither
int is_float(const char *string) {
  int state = FStart; // state

  while(*string) {
    char c = *(string++); // get a character
    if(c == '+' || c == '-')
      state = float_state_machine[state].plus_minus;
    else if(isdigit(c))
      state = float_state_machine[state].is_digit;
    else if(c == '.')
      state = float_state_machine[state].decimal_point;
    else if(c == 'E')
      state = float_state_machine[state].e;
    else
      state = FError;
  }
  return float_state_machine[state].valid;
}

// Data structure for symbol table entries
struct symbol_data {
	const char *lexeme;
	int token_category;
	struct symbol_data *next;
};

struct symbol_data *symbol_table = NULL;

// ----- TOKEN INFORMATION -----
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

// The token_strings list lays out the token category name (first element)
// as well as the strings for each of the tokens in the category (all other elements).
const char *token_strings[t_max_tokens][20] = {
	{"identifier", NULL},             {"integer", NULL},
	{"real", NULL},                   {"string", NULL},
	{"add/sub", "+", "-", NULL},
	{"mul/div", "*", "/", "%", NULL}, {"logical", "<", ">", "<=", ">=", "==", "<>", NULL},
	{"shift", "<<", ">>"},            {"bitmath", "&", "|", "^"},
	{"unary", "!", "~"},              
	{"lparen", "(", NULL},            {"rparen", ")", NULL},
	{"lcurly", "{", NULL},            {"rcurly", "}", NULL},
	{"lsquare", "[", NULL},           {"rsquare", "]", NULL},
	{"assignment", "=", NULL},        {"comma", ",", NULL}, {"colon", ":", NULL},

	// keywords
	{"if", "if", NULL},         {"else", "else", NULL},     {"elif", "elif", NULL},
	{"until", "until", NULL},   {"while", "while", NULL},
	{"for", "for", NULL},       {"in", "in", NULL},			{"step", "step", NULL},
	{"to", "to", NULL},         {"continue", "continue", NULL},
	{"break", "break", NULL},   {"none", "none", NULL},
	{"def", "def", NULL},       {"var", "var", NULL},
	{"true", "true", NULL},     {"false", "false", NULL},

	{"return", "return", NULL},

	{"\n", "\n", NULL},
	{"{{", "{{", NULL},
	{"]}", "}}", NULL},
};

// Data structure for a token
struct lexeme_token {
	int token_category;         // which token category
	int token_value;            // which token in the token category
	struct symbol_data *symbol; // symbol table entry
	struct lexeme_token *next;
};

// Makes a string representation of a token
const char *token_print(struct lexeme_token *token) {
	static char buffer[50];
	if(!token)
		return "?";

	// newlines use token_value differently
	if(token->token_category == t_newline)
		sprintf(buffer, "{\\n %d}", token->token_value);
	// if it's a token that uses a symbol, print the symbol
	else if(token->symbol)
		sprintf(buffer, "(%s, %s)", token->symbol->lexeme, token_strings[token->token_category][0]);
	// keyword
	else if(!strcmp(token_strings[token->token_category][token->token_value], token_strings[token->token_category][0]))
		sprintf(buffer, "(%s)", token_strings[token->token_category][0]);
	// token with multiple variants
	else
		sprintf(buffer, "(%s, %s)", token_strings[token->token_category][token->token_value], token_strings[token->token_category][0]);
	return buffer;
}

// ----- STATE VARIABLES -----

char lexeme[100] = ""; // buffer to accumulate the lexeme in
int lexeme_index = 0;	// index in the buffer
struct lexeme_token *token_list = NULL;
struct lexeme_token *token_current = NULL;
FILE *program_file;
int c; // current character

// Error, so exit the whole program
void error(const char *format, ...) {
	va_list argptr;
	va_start(argptr, format);
	printf("Error: ");
	vprintf(format, argptr);
	putchar('\n');
	va_end(argptr);
	exit(-1);
}

// ----- SYMBOL TABLE -----

// Allocate a new symbol for the symbol table
struct symbol_data *alloc_symbol(const char *lexeme, int token_category) {
	struct symbol_data *new_symbol;
	if(new_symbol == NULL)
		error("Couldn't allocate symbol");
	new_symbol = (struct symbol_data*)calloc(1, sizeof(struct symbol_data));
	new_symbol->lexeme = strdup(lexeme);
	new_symbol->token_category = token_category;
	new_symbol->next = NULL;
	return new_symbol;
}

// Find a symbol in the symbol table and optionally auto-create it if it's not found
struct symbol_data *find_symbol(const char *lexeme, int token_category, int auto_create) {
	struct symbol_data *symbol;
	struct symbol_data *new_symbol;

	// If the symbol table is empty, handle that
	if(!symbol_table) {
		if(auto_create) {
			new_symbol = alloc_symbol(lexeme, token_category);
			symbol_table = new_symbol;
			return new_symbol;
		} else
			return NULL;
	}

	// Find the symbol if it exists
	struct symbol_data *previous = NULL;
	for(symbol = symbol_table; symbol; symbol = symbol->next) {
		previous = symbol;
		if(!strcmp(symbol->lexeme, lexeme) && symbol->token_category == token_category)
			return symbol;
	}

	// Symbol does not exist, can I create it?
	if(!auto_create)
		return NULL;
	// Create it, add it to the table
	new_symbol = alloc_symbol(lexeme, token_category);
	previous->next = new_symbol;
	return new_symbol;
}

// ----- LEXICAL ANALYZER -----

// Reads the next character from the file
void get_char() {
	c = fgetc(program_file);
}

// Adds a character, and ends the string there
void add_char() {
	lexeme[lexeme_index++] = c;
	if(lexeme_index >= sizeof(lexeme)) {
		error("Lexeme is too long (%s)", lexeme);
	}
	lexeme[lexeme_index] = 0;
	c = 0;
}

// Clear the lexeme buffer
void clear_lexeme() {
	lexeme[0] = 0;
	lexeme_index = 0;
}

// Is the lexeme a keyword?
int is_keyword() {
	int i;
	for(i = t_first_keyword; i <= t_last_keyword; i++) {
		if(!strcmp(token_strings[i][0], lexeme)) {
			return i;
		}
	}
	return -1;
}

// Adds a token to the list of tokens
void add_token(int token_category, int symbol) {
	// Allocate and set the token
	struct lexeme_token *token = (struct lexeme_token*)calloc(1, sizeof(struct lexeme_token));
	if(!token) {
		error("Can't allocate token");
	}

	if(symbol) {
	// If it's a symbol, maintain the symbol table
		token->symbol = find_symbol(lexeme, token_category, 1);
	} else {
		// If it's not a symbol, loop to find the correct token category subtype to use
		for(int i=0; token_strings[token_category][i]; i++)
			if(!strcmp(token_strings[token_category][i], lexeme)) {
				token->token_value = i;
				break;
			}
	}
	token->token_category = token_category;
	token->next = NULL;

	// Add it to the list
	if(token_current) {
		token_current->next = token;
	}
	token_current = token;

	// Start the list if it's not already started
	if(!token_list) {
		token_list = token;
	}

	// Get ready for the next lexeme
	clear_lexeme();
}

// Adds a token consisting of one character
void add_char_token(int token_category) {
	add_char();
	add_token(token_category, 0);
}

// Run the lexical analyzer
struct lexeme_token *lexical_analyzer(FILE *File) {
	program_file = File;
	token_list = NULL;
	token_current = NULL;
	clear_lexeme();

	// Lexical analyzer main loop
	while(1) {
		// Read a character if we don't have one
		if(c == 0) {
			get_char();
		}
		if(c == EOF) {	 // Stop if the end was reached
			break;
		}

		// Recognize all the different types of tokens
		if(isspace(c)) { // Ignore space characters
			if(c == '\n') {
				// newlines add a newline token
				add_char_token(t_newline);

				// count the indent
				get_char();
				while(c == '\t' || c == ' ') {
					get_char();
					token_current->token_value++;
				}
			} else {
				// all other spaces are ignored
				c = 0;
			}
		} else if(c == '-' || c =='+') {
			add_char_token(t_addsub);
		} else if(c == '*' || c == '/' || c == '%') {
			add_char_token(t_muldiv);
		} else if(c == '!' || c == '~') {
			add_char_token(t_unary);
		} else if(c == '&' || c == '|' || c == '^') {
			add_char_token(t_bitmath);
		} else if(c == '(') {
			add_char_token(t_lparen);
		} else if(c == ')') {
			add_char_token(t_rparen);
		} else if(c == '{') {
			add_char_token(t_lcurly);
		} else if(c == '}') {
			add_char_token(t_rcurly);
		} else if(c == '[') {
			add_char_token(t_lsquare);
		} else if(c == ']') {
			add_char_token(t_rsquare);
		} else if(c == ',') {
			add_char_token(t_comma);
		} else if(c == ':') {
			add_char_token(t_colon);
		// Comparison operators
		} else if(c == '=') { // differentiate between = and ==
			add_char();
			get_char();
			if(c == '=') {
				add_char();
				add_token(t_logical, 0);
			} else {
				add_token(t_assignment, 0);
			}
		} else if(c == '>') {
			add_char();
			get_char();
			if(c == '>') {
				add_char();
				add_token(t_shift, 0);
			} else {
				if(c == '=') { // >=
					add_char();
				}
				add_token(t_logical, 0);
			}
		} else if(c == '<') {
			add_char();
			get_char();
			if(c == '<') {
				add_char();
				add_token(t_shift, 0);
			} else {
				if(c == '=' || c == '>') { // <= <>
					add_char();
				}
				add_token(t_logical, 0);
			}
		// "strings"
		} else if(c == '\"') {
			add_char();
			int old_char; // add_char clears c, so keep a copy here
			do {
				get_char();
				old_char = c;
				add_char();
			} while(old_char != '\"' && old_char != EOF);
			add_token(t_string, 1);
		// identifiers or keywords
		} else if(isalpha(c) || c == '_' || c == '@') {
			add_char();
			get_char();
			while(isalnum(c) || c == '_') {
				add_char();
				get_char();
			}
			// Look up what keyword it is, if it's a keyword at all
			int keyword_num = is_keyword();
			if(keyword_num != -1) {
				add_token(keyword_num, 0);
			} else {
				add_token(t_identifier, 1);
			}
		// Integers and floats
		} else if(isdigit(c)) {
			while(isdigit(c) || c == 'E' || c == '.') {
				add_char();
				get_char();
			}
			int is_a_float = is_float(lexeme);
			if(is_a_float == 0)
				error("Invalid number %s", lexeme);
			if(is_a_float == 1) // 1 = float
				add_token(t_real, 1);
			if(is_a_float == 2) // 2 = integer
				add_token(t_integer, 1);
		// If a comment is found, skip to the end of the line
		} else if(c == '#') {
			do {
				get_char();
			} while(c != '\n' && c != EOF);
		} else {
			error("Unexpected character %c", c);
		}

	}
	return token_list;
}

// ----- PARSE TREE FUNCTIONS -----

struct syntax_node {
  struct lexeme_token *token;
  struct syntax_node *child, *next;
};

struct syntax_node *tree_head = NULL;
struct syntax_node *tree_current = NULL;

// Make a new tree node
struct syntax_node *tree_new() {
  struct syntax_node *node = (struct syntax_node*)calloc(1, sizeof(struct syntax_node));
  node->token = token_current;
  return node;
}

// Directly set one node's child
void tree_add_child(struct syntax_node *parent, struct syntax_node *child) {
  if(!parent->child) {
    // Put it directly in the child pointer if possible
    parent->child = child;
  } else {
    // If there's already a child, find the next free spot for it
    struct syntax_node *temp = tree_current->child;
    while(temp->next)
      temp = temp->next;
    temp->next = child;
  }
}

// Add a child to the current node and make that the new current node
void tree_child() {
  struct syntax_node *node = tree_new();
  if(!tree_head) {
    // no head? set this as the new head
    tree_head = node;
  } else if(!tree_current) {
    // add another thing to global scope
    tree_current = tree_head;
    while(tree_current->next)
      tree_current = tree_current->next;
    tree_current->next = node;    
  } else if(!tree_current->child) {
    // there's no child yet, add one
    tree_current->child = node;
  } else {
    // there's already a child, add a sibling
    tree_current = tree_current->child;
    while(tree_current->next)
      tree_current = tree_current->next;
    tree_current->next = node;
  }
  tree_current = node;
}

// ----- CONVERT INDENTS TO BRACES -----
void convert_indents(struct lexeme_token *list) {
	int indent_level[20] = {0};
	int indent_index = 0;

	while(list) {
		if(list->token_category == t_newline) {
			// ignore (and remove) all but the last newline in a row
			while(list->next && list->next->token_category == t_newline) {
				struct lexeme_token *free_me = list->next;
				struct lexeme_token *after = free_me->next;
				
				memcpy(list, free_me, sizeof(struct lexeme_token));
				free(free_me);
				list->next = after;
			}

			// current indent amount
			int target = list->token_value;

			if(target > indent_level[indent_index]) {
				// indenting in
				if(indent_index == 19) // stack overflow
					error("Too many indents");
				indent_level[++indent_index] = target;

				// add a t_indent_in token
				struct lexeme_token *new_token = (struct lexeme_token*)calloc(1, sizeof(struct lexeme_token));
				new_token->token_category = t_indent_in;
				new_token->next = list->next;
				list->next = new_token;
				list = new_token;
			} else if(target < indent_level[indent_index]) {
				// indenting out
				int old_index = indent_index;

				while(indent_level[indent_index] > target) {
					indent_index--;

					// add a t_indent_out token
					struct lexeme_token *new_token = (struct lexeme_token*)calloc(1, sizeof(struct lexeme_token));
					new_token->token_category = t_indent_out;
					new_token->next = list->next;
					list->next = new_token;
					list = new_token;
				}
				if(indent_level[indent_index] != target)
					error("Inconsistent indentation?");
			}
		}
		list = list->next;
	}
}

// ----- SYNTACTICAL ANALYZER -----

// Moves onto the next token in the program
void next_token() {
	if(!token_current) {
		puts("Already at the end of the program");
		exit(0);
	}
	token_current = token_current->next;
//	puts(token_print(token_current));
}

enum {
	NEEDED = 1, // mandatory token
	OMIT   = 2, // don't include in syntax tree
	TEST   = 4, // just test, don't actually accept the token
};

// Accept a token from the program and move onto the next one
int accept(int flags, ...) {
	int passing = 0;
	va_list argptr;
	va_start(argptr, flags);

	// Loop through the list of acceptable token types
	while(1) {
		int value = va_arg(argptr,int);
		if(value == -1) // -1 means end of list
			break;
		if(token_current->token_category == value)
			passing = 1;
	}
	va_end(argptr);

	// If only testing, only return the passing variable
	if(flags & TEST)
		return passing;
	// If it's needed, it's an error if it's not found
	if(!passing && (flags & NEEDED))
		error("Unexpected token, %s", token_strings[token_current->token_category][0]);
	// If it's passing, accept it and get the next token
	if(passing) {
		if(!(flags & OMIT))
			tree_child();
		next_token();
	}
	return passing;
}

void expression();

// Allow there to be an array index after the identifier
void array_index() {
	struct syntax_node *save = tree_current;
	if(accept(0, t_lsquare, -1)) {
		expression();
		accept(NEEDED|OMIT, t_rsquare, -1);
	}
	tree_current = save;
}

// Factor, can be any identifier, number, string, boolean, etc.
void factor() {
	struct syntax_node *save = tree_current;
	accept(0, t_unary, -1);

	if(accept(0, t_identifier, -1)) {
		array_index();
		if(accept(0, t_lparen, -1)) { // function call
			if(!accept(OMIT, t_rparen, -1)) {
				do {
					expression();
				} while(accept(OMIT, t_comma, -1));
				accept(NEEDED|OMIT, t_rparen, -1);
			}
		}
	} else if(accept(0, t_lsquare, -1)) {
		// array
		if(!accept(OMIT, t_rsquare, -1)) {
			do {
				expression();
			} while(accept(OMIT, t_comma, -1));
			accept(NEEDED|OMIT, t_rsquare, -1);
		}
	} else if(accept(0, t_integer, t_real, t_string, t_none, t_true, t_false, -1)) {

	} else if(accept(0, t_lparen, -1)) {
		expression();
		accept(NEEDED|OMIT, t_rparen, -1);
	}
	tree_current = save;
}

// Allow multiplication
void term() {
	struct syntax_node *save = tree_current;

	// make a temporary node to attach the left hand side onto
	struct syntax_node temp = {NULL, NULL, NULL};
	tree_current = &temp;
	factor();
	tree_current = save;

	struct syntax_node *operator = NULL;
	if(accept(0, t_muldiv, -1)) {
		tree_add_child(tree_current, temp.child);
		term();
	} else {
		tree_add_child(save, temp.child);
	}
	tree_current = save;
}

// The most outer level, with addition
void addition() {
	struct syntax_node *save = tree_current;
	accept(0, t_addsub, -1); // optional sign

	// make a temporary node to attach the left hand side onto
	struct syntax_node temp = {NULL, NULL, NULL};
	tree_current = &temp;
	term();
	tree_current = save;

	struct syntax_node *operator = NULL;
	if(accept(0, t_addsub, -1)) {
		tree_add_child(tree_current, temp.child);
		addition();
	} else {
		tree_add_child(save, temp.child);
	}
	tree_current = save;
}

// The most outer level, with comparisons
void expression() {
	struct syntax_node *save = tree_current;

	// make a temporary node to attach the left hand side onto
	struct syntax_node temp = {NULL, NULL, NULL};
	tree_current = &temp;
	addition();
	tree_current = save;

	struct syntax_node *operator = NULL;
	if(accept(0, t_logical, -1)) {
		tree_add_child(tree_current, temp.child);
		expression();
	} else {
		tree_add_child(save, temp.child);
	}
	tree_current = save;
}

void variable_declaration();
void function_definition();

// One statement of any kind
void statement() {
	struct syntax_node *save = tree_current;

	if(accept(0, t_var, -1)) {
		variable_declaration();
	} else if(accept(0, t_def, -1)) {
		function_definition();
	} else if(accept(0, t_identifier, -1)) { 
		// Assignment or function call
		struct syntax_node *identifier = tree_current;

		// accept an array index if found
		int left_is_array = 0;
		struct syntax_node temp = {NULL, NULL, NULL};
		if(accept(TEST, t_lsquare, -1)) {
			tree_current = &temp;
			left_is_array = 1;
			array_index();
			tree_current = identifier;
		}
		if(accept(0, t_assignment, -1)) { // assignment
			struct syntax_node *assignment = tree_current;

			if(left_is_array)
				assignment->child = temp.child;

			struct lexeme_token *identifier_token = identifier->token;
			struct lexeme_token *assignment_token = assignment->token;

			// Swap the tokens
			identifier->token = assignment_token;
			assignment->token = identifier_token;

			tree_current = identifier;

			expression();
		} else if(accept(0, t_lparen, -1)) { // function call
			if(!accept(OMIT, t_rparen, -1)) {
				// If there's arguments,
				// read expressions until there's no more commas
				do {
					expression();
				} while(accept(OMIT, t_comma, -1));
				accept(NEEDED|OMIT, t_rparen, -1);
			}
			accept(NEEDED|OMIT, t_newline, -1);
		}
	} else if(accept(0, t_indent_in, -1)) {
	// Multiple statements
		while(!accept(OMIT, t_indent_out, -1))
			statement();
	} else if(accept(OMIT, t_newline, -1)) {
	// Empty statement
	} else if(accept(0, t_if, t_elif, t_while, t_until, -1)) {
		expression();
		accept(NEEDED|OMIT, t_colon, -1);
		accept(NEEDED|OMIT, t_newline, -1);
		statement();
	} else if(accept(0, t_for, -1)) {
		struct syntax_node *for_save = tree_current;
		accept(NEEDED, t_identifier, -1);
		tree_current = for_save;

		// range
		if(accept(0, t_assignment, -1)) {
			struct syntax_node *range_save = tree_current;
			expression();
			accept(NEEDED, t_to, -1);
			tree_current = range_save;
			expression();
			tree_current = range_save;
			// allow specifying a step
			if(accept(0, t_step, -1)) {
				expression();
			}
		} else if(accept(NEEDED, t_in, -1)) {
			expression();
		}
		accept(NEEDED|OMIT, t_colon, -1);
		accept(NEEDED|OMIT, t_newline, -1);
		tree_current = for_save;
		statement();
	} else if(accept(0, t_else, -1)) {
		statement();
	} else if(accept(0, t_return, -1)) {
		expression();
		accept(NEEDED|OMIT, t_newline, -1);
	} else {
		error("Bad token %s", token_print(token_current));
	}

	tree_current = save;
}

void variable_declaration() {
	struct syntax_node *save = tree_current;

	struct syntax_node *parameter_save = tree_current;
	do {
		tree_current = parameter_save;
		accept(NEEDED, t_identifier, -1);
		if(accept(OMIT, t_assignment, -1)) {
			expression();
		}
	} while(accept(OMIT, t_comma, -1)); // keep going if there's a comma
	accept(OMIT, t_newline, -1);

	tree_current = save;
}

void function_definition() {
	struct syntax_node *save = tree_current;

	accept(NEEDED, t_identifier, -1); // name
	struct syntax_node *function_save = tree_current;
	accept(NEEDED, t_lparen, -1);

	// parameters
	if(accept(OMIT, t_rparen, -1)) {

	} else {
		struct syntax_node *parameter_save = tree_current;
		do {
			tree_current = parameter_save;
			accept(NEEDED, t_identifier, -1);
		} while(accept(OMIT, t_comma, -1)); // keep going if there's a comma
		accept(NEEDED|OMIT, t_rparen, -1);
	}
	accept(NEEDED|OMIT, t_colon, -1);
	accept(OMIT, t_newline, -1);

	tree_current = function_save;
	statement();
	tree_current = save;
}

// Run the syntactical analyzer
void syntactical_analyzer(struct lexeme_token *list) {
	token_current = list;

	while(token_current) {
		tree_current = NULL; // NULL because it's in the global space

		if(accept(OMIT, t_newline)) {

		} else if(accept(0, t_var, -1)) {
			variable_declaration();
		} else if(accept(NEEDED, t_def, -1)) {
			function_definition();
		}
	}
}

// ----- MAIN PROGRAM -----

// Prints out a parse tree graphically
void print_parse_tree(struct syntax_node *node, int level) {
	while(node) {
		for(int i=0; i<level; i++)
			printf("   ");
		printf(token_print(node->token));
		putchar('\n');

		if(node->child)
			print_parse_tree(node->child, level+1);
		node = node->next;
	}
}

int main(int argc, char *argv[]) {
	FILE *File = fopen("test.txt", "rb");

	struct lexeme_token *list = lexical_analyzer(File);
	convert_indents(list);

	puts("Token list:");
	for(token_current = token_list; token_current; token_current = token_current->next) {
		puts(token_print(token_current));
	}

	puts("\n\n\nSymbol table:");
	for(struct symbol_data *symbol = symbol_table; symbol; symbol = symbol->next) {
		printf("(%s, %s)\n", symbol->lexeme, token_strings[symbol->token_category][0]);
	}

	syntactical_analyzer(list);

	puts("\n\n\nSyntax tree:");
	print_parse_tree(tree_head, 0);

	fclose(File);
}
