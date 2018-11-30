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
#include "ttc.h"

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

struct symbol_data *symbol_table = NULL;

// ----- TOKEN INFORMATION -----

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
