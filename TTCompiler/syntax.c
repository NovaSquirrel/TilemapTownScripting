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

// ----- PARSE TREE FUNCTIONS -----

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
