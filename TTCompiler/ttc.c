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
