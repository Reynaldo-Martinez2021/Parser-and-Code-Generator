/*
	COP 3402: System Software
	Fall 2022
	Homework #3 Parser-Code Generator
	Reynaldo Martinez
	John Dufresne
	Tyler Slakman
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "compiler.h"

lexeme *tokens;
int token_index = 0;
symbol *table;
int table_index = 0;
instruction *code;
int code_index = 0;

int error = 0;
int level;

void emit(int op, int l, int m);
void add_symbol(int kind, char name[], int value, int level, int address);
void mark();
int multiple_declaration_check(char name[]);
int find_symbol(char name[], int kind);

void print_parser_error(int error_code, int case_code);
void print_assembly_code();
void print_symbol_table();

void program();
void block();
void declarations();
void const_declaration();
void var_declaration(int number_of_variables);
void proc_declaration();
void statement();
void condition();
void expression();
void term();
void factor();
int get_token_type();

instruction *parse(int code_flag, int table_flag, lexeme *list)
{
	tokens = list;
	table = calloc(ARRAY_SIZE, sizeof(symbol)); 		// stores the information for all procedures, variables, and constants in the program
	code = calloc(ARRAY_SIZE, sizeof(instruction));		// will store the instructions and will eventually be returned if there were no errors (error == 0)

	// initialize level to -1 before calling BLOCK
	level = -1;
	block();

	// if stopping error occurred (error == -1) 
	// free both table and code and return NULL
	if (error == -1)
	{
		free(table);
		free(code);
		return NULL;
	}

	// Parser Error 1: missing . (always non-stopping)
	if (tokens[token_index].type != period)
	{
		print_parser_error(1,0);
		error = 1;
	}

	for (int i = 0; i < code_index; i++)
	{
		// 1. Find every call instruction in the code array
		if (code[i].op == CAL)
		{
			// 2. If the M field is -1, move to the next one
			if (code[i].m == -1)
			{
				continue;
			}
			// Parser Error 21: procedure being called has not been defined (always non stopping)
			else if (table[code[i].m].address == -1)
			{
				print_parser_error(21, 0);
				error = 1;
			}
			// 3. Otherwise the current M field is the index of the desired procedure in the symbol table
			else
			{
				code[i].m = table[code[i].m].address;
			}
		}
	}

	// no errors occurred
	if (error == 0)
	{
		// print assembly code if code_flag is true (1)
		if (code_flag)
		{
			print_assembly_code();
		}

		// print symbol table if table_flag is true(1)
		if (table_flag)
		{
			print_symbol_table();
		}

		return code;
	}
 
   	return NULL;
}

// BLOCK ::= DECLARATIONS STATEMENT
void block()
{
	// level should be incremented before DECLARATIONS
	level++;

	declarations();
	if (error == -1) return;

	statement();
	if (error == -1) return;

	// the symbols belong to the current procedure must be marked so the parent procedures can't use them
	mark();
	level--;
}

// DECLARATIONS ::= { CONST | VAR | PROC } INC
void declarations()
{
	// keep track of how many variables have been declared, start at 0
	int vars = 0;
	int token_type;

	token_type = get_token_type();
	// only loop while the current token matches one of the three first tokens (keyword_const, keyword_var, keyword_procedure)
	while (token_type == keyword_const || token_type == keyword_var || token_type == keyword_procedure)
	{
		if (token_type == keyword_const)
		{
			const_declaration();
		}
		else if (token_type == keyword_var)
		{
			// the VAR function should be passed the number of variables declared
			var_declaration(vars);
			// increment after each call to 
			vars++;
		}

		else if (token_type == keyword_procedure)
		{
			proc_declaration();
		}

		token_type = get_token_type();
	}
	
	// the M field of INC is equal to the number of variables declared + 3 (the activation record size)
	emit(7, 0, vars + 3);
}

// CONST ::= [const (ident [3] | 2-1) (:= | 4-1) [-] (number | 5) (; | 6-1)]
void const_declaration()
{
	char name[11];
	int symbol_index, token_type, value, minus_flag = 0;

	token_index++;
	// if tokens.type != ident then Parser Error 2-1: missing identifier after keyword const
	if (tokens[token_index].type != identifier)
	{
		print_parser_error(2,1);

		// Parser Error 2-1 (non stopping): if next symbol is the assignment_symbol
		if (tokens[token_index].type == assignment_symbol)
		{
			error = 1;
			// if error 2-1 is non stopping, you can use "null" for the symbol name
			strcpy(name, "null");
		}
		// Parser Error 2-1 (stopping): if next symbol is anything other than the assignment_symbol
		else
		{
			error = -1;
			return;
		}
	}
	// if token_type is identifier
	else
	{
		// call multiple_declaration_check (used in declaration functions to check if potential symbol name has already been used)
		symbol_index = multiple_declaration_check(tokens[token_index].identifier_name);

		// Parser Error 3-0 (always non-stopping): identifier is declared multiple times by a procedure
		if (symbol_index != -1)
		{
			print_parser_error(3,0);
			error = 1;
		}
		
		strcpy(name, tokens[token_index].identifier_name);
		token_index++;
	}

	// if tokens.type != assingment symbol then Parser Error 4-1: missing := in constant declaration
	if (tokens[token_index].type != assignment_symbol)
	{
		print_parser_error(4,1);
		
		// Parser Error 4-1 (non stopping): if next symbol is the optional minus or mandatory number symbol
		if (tokens[token_index].type == minus || tokens[token_index].type == number)
		{
			error = 1;
		}
		// Parser Error 4-1 (stopping): if next symbol is anything other than minus || assignment symbol
		else
		{
			error = -1;
			return;
		}
		
	}
	// if tokens.type is the assignment symbol, no actions needed move on to the next token
	else
		token_index++;

	// check for optional "minus" symbol
	if (tokens[token_index].type == minus)
	{
		// set minus flag to 1 for processing later
		minus_flag = 1;
		// move on to the next token
		token_index++;
	}

	// if tokens.type != number then Parser Error 5-0: missing number in constant declaration
	if (tokens[token_index].type != number)
	{
		print_parser_error(5,0);

		// Parser Error 5-0 (non stopping): if the next symbol is a semicolon
		if (tokens[token_index].type == semicolon)
		{
			error = 1;
			// if Error 5-0 is non stopping use 0 for the symbol value
			value = 0;
		}
		// Parser Error 5-0 (stopping): next symbol is anything other than semicolon
		else
		{
			error = -1;
			return;
		}
	}
	else
	{
		value = tokens[token_index].number_value;
		token_index++;
	}

	// if minus_flag multiply value by -1 to reflect intended number
	if (minus_flag)
		value = value * -1;

	// use the global level for the level of the sybmol, constants don't have addresses use 0
	add_symbol(1, name, value, level, 0);

	// if tokens.type != semicolon Parser Error 6-1: missing ; after constant declaration
	if (tokens[token_index].type != semicolon)
	{
		print_parser_error(6,1);

		token_type = get_token_type();
		// Parser Error 6-1 (non stopping): if the follow set is [const var procedure ident call begin if while read write def return . } ]
		if (token_type == keyword_const || token_type == keyword_var || token_type == keyword_procedure || token_type == identifier ||
			token_type == keyword_call || token_type == keyword_begin || token_type == keyword_if || token_type == keyword_while ||
			token_type == keyword_read || token_type == keyword_write || token_type == keyword_def || token_type == keyword_return ||
			token_type == period || token_type == right_curly_brace)
		{
			error = 1;
		}
		// Parser Error 6-1 (stopping): if the next symbol is anything other than the follow set
		else
		{
			error = -1;
			return;
		}
	}
	else
		token_index++;
}

// VAR ::= [var (ident [3] | 2-2) (; | 6-2) ]
void var_declaration(int variables)
{
	char name[11];
	int symbol_index, token_type;

	token_index++;
	// if tokens.type != ident then Parser Error 2-2: missing identifier after keyword var
	if (tokens[token_index].type != identifier)
	{
		print_parser_error(2,2);

		// Parser Error 2-2 (non stopping): if next symbol is a semicolon
		if (tokens[token_index].type == semicolon)
		{
			error = 1;
			// if error 2-2 is non stopping, you can use "null" for the symbol name
			strcpy(name, "null");
		}
		// Parser Error 2-2 (stopping): if next symbol is not a semicolon
		else
		{
			error = -1;
			return;
		}
	}
	else
	{
		// determine if potential symbol name has already been used
		symbol_index = multiple_declaration_check(tokens[token_index].identifier_name);
		
		// Parser Error 3-0 (always non-stopping): identifier is declared multiple times by a procedure
		if (symbol_index != -1)
		{
			print_parser_error(3, 0);
			error = 1;
		}

		strcpy(name, tokens[token_index].identifier_name);
		token_index++;
	}

	// use global level for the level of the symbol
	// variables don't have values, use 0
	// the address of the symbol is the function's arguments + 3
	add_symbol(2, name, 0, level, variables + 3);
	
	// if tokens.type != semicolon then Parser Error 6-2: missing ; after variable declaration
	if (tokens[token_index].type != semicolon)
	{
		print_parser_error(6,2);
		token_type = get_token_type();

		// Parser Error 6-2 (non-stopping): if the follow set is [const var procedure ident call begin if while read write def return . } ]
		if (token_type == keyword_const || token_type == keyword_var || token_type == keyword_procedure || token_type == identifier ||
			token_type == keyword_call  || token_type == keyword_begin || token_type == keyword_if || token_type == keyword_while ||
			token_type == keyword_read || token_type == keyword_write || token_type == keyword_def || token_type == keyword_return ||
			token_type == period || token_type == right_curly_brace)
		{
			error = 1;
		}
		// Parser Error 6-2 (stopping): next symbol is not in follow set
		else
		{
			error = -1;
			return;
		}
	}

	else
		token_index++;

}


// PROC ::= [procedure (ident [3] | 2-3) (; | 6-3) ]
void proc_declaration() {
	char name[11];
	int symbol_index, token_type;

	token_index++;
	// if tokens.type != identifier then Parser Error 2-3: missing identifier after keyword procedure
	if (tokens[token_index].type != identifier)
	{
		print_parser_error(2,3);
		// Parser Error 2-3 (non-stopping): if next token is semicolon
		if (tokens[token_index].type == semicolon)
		{
			error = 1;
			// if error 2-3 is non-stopping, you can use "null" for the symbol name
			strcpy(name, "null");
		}
		// Parser Error 2-3 (stopping): if next token is anything other than semicolon
		else
		{
			error = -1;
			return;
		}
		
	}

	else
	{
		// determine if potential symbol name has already been used
		symbol_index = multiple_declaration_check(tokens[token_index].identifier_name);
		
		// Parser Error 3-0 (always non-stopping): identifier is declared multiple times by a procedure
		if (symbol_index != -1)
		{
			print_parser_error(3, 0);
			error = 1;
		}

		strcpy(name, tokens[token_index].identifier_name);
		token_index++;
	}

	// procedures don't have values, use 0
	// set the address field to -1
	add_symbol(3, name, 0, level, -1);

	// if tokens.type != semicolon then Praser Error 6-3: missing ; after procedure declaration
	if (tokens[token_index].type != semicolon)
	{
		print_parser_error(6,3);

		token_type = get_token_type();
		// Parser Error 6-3 (non-stopping): if the follow set is [const var procedure ident call begin if while read write def return . } ]
		if (token_type == keyword_const || token_type == keyword_var || token_type == keyword_procedure || token_type == identifier ||
			token_type == keyword_call || token_type == keyword_begin || token_type == keyword_if || token_type == keyword_while ||
			token_type == keyword_read || token_type == keyword_write || token_type == keyword_def || token_type == keyword_return ||
			token_type == period || token_type == left_curly_brace)
		{
			error = 1;
		}
		// Parser Error 6-3 (stopping): if the next symbol is not in the follow set
		else
		{
			error = -1;
			return;
		}
	}
	else
		token_index++;
}

/*
STATEMENT ::= [ ident [8-1 | 7] (:= | 4-2) EXPRESSION STO
				| call (ident [8-2 | 9] | 2-4) CAL
				| begin STATEMENT {; STATEMENT} (end | 6-4 | 10)
				| if CONDITION JPC (then | 11) STATEMENT
				| while CONDITION JPC (do | 12) STATEMENT JMP
				| read (ident [8-3 | 13] | 2-5) RED STO
				| write EXPRESSION WRT
				| def (ident [8-4 | 14 | 22 | 23] | 2-6) ({ | 15) JMP
					BLOCK [RTN] (} | 16)
				| return [HLT | RTN] ] 
*/
void statement()
{
	// int if_jpc_op, if_jmp_op, while_jpc_op, while_jmp_op, def_jmp_op;
	int symbol_idx, token_type;
	int l = -1, m = -1;
	
	// structure statement with a switch case based on current token
	switch (tokens[token_index].type)
	{
		// case identifier
		case identifier:
		{
			// use support function find_symbol to gget the index of the desire symbol
			symbol_idx = find_symbol(tokens[token_index].identifier_name, 2);

			// if find_symbol returns -1 you'll have an error
			if (symbol_idx == -1)
			{
				// Parser Error 8-1 (always non-stopping): undeclared identifier used in assignmnet statement
				// find_symbol(desired name, kind 3) and find_symbol(desired name, kind 1) both also return -1
				if (find_symbol(tokens[token_index].identifier_name, 3) == -1 && find_symbol(tokens[token_index].identifier_name, 1) == -1)
				{
					print_parser_error(8, 1);
				}
				// Parser Error 7 (always non-stopping): procedures and constants cannot be assigned to
				// find_symbol(desired name, kind 3) or find_symbol(desired name, kind 1) returned something other than -1
				else
				{
					print_parser_error(7, 0);
				}

				error = 1;

				// use -1 for L and M if you encounter error 8-1 or 7-0
			}
			// no error occurred
			else
			{
				// L value is gloabl value level minus the level field of the symbol from the table
				l = level - table[symbol_idx].level;
				// M value is the address field of the symbol from the table
				m = table[symbol_idx].address;
			}

			token_index++;

			// if tokens.type != assignment symbol Parser Error 4-2: missing := in assignment statement
			if (tokens[token_index].type != assignment_symbol)
			{
				print_parser_error(4, 2);
				token_type = get_token_type();
				// Parser Error 4-2 (non-stopping): if follow set is [ ident number ( ]
				if (token_type == identifier || token_type == number || token_type == left_parenthesis)
				{
					error = 1;
				}
				// Parser Error 4-2 (stopping): if token_type is not in follow set
				else
				{
					error = -1;
					return;
				}
			}
			else
			{
				token_index++;
			}

			expression();
			if (error == -1) return;

			emit(STO, l, m);
			break;
		}

		// case call
		case keyword_call:
		{
			token_index++;

			// if token.type != identifer then Parser Error 2-4: missing identifier after keyword call
			if (tokens[token_index].type != identifier)
			{
				print_parser_error(2, 4);
				token_type = get_token_type();

				// Parser Error 2-4 (non stopping): if follow set is [ . } ; end ]
				if (token_type == period || token_type == right_curly_brace || token_type == semicolon || token_type == keyword_end)
				{
					error = 1;
				}
				// Parser Error 2-4 (stopping): if token is not in follow set
				else
				{
					error = -1;
					return;
				}

				// use -1 for L and M field if you found non-stopping error
			}

			else
			{
				// use support function find_symbol to get the index of the desired symbol
				symbol_idx = find_symbol(tokens[token_index].identifier_name, 3);

				// if find_symbol returns -1, you'll have an error
				if (symbol_idx == -1)
				{
					// Parser Error 8-2 (always non-stopping): undeclared identifier used in call statement
					// find_symbol(desired name, kind 2) and find_symbol(desired name, kind 1) both return -1
					if (find_symbol(tokens[token_index].identifier_name, 2) == -1 && find_symbol(tokens[token_index].identifier_name, 1) == -1)
					{
						print_parser_error(8, 2);
					}
					// Parser Error 9 (always non-stopping): variables and constants cannot be called
					// find_symbol(desired name, kind 2) or find_symbol(desired name, kind 1) return something other than -1
					else
					{
						print_parser_error(9, 0);
					}
					error = 1;
				}
				// no error occurred
				else
				{
					// L value is global value minus the level field of the symbol from the table
					l = level - table[symbol_idx].level;
					// M value is the index of the symbol in the table (the value returned by find_symbol)
					m = symbol_idx;
				}

				token_index++;
			}

			emit(CAL, l, m);
			break;
		}

		// case begin
		case keyword_begin:
		{
			// we recommend a do-while loop
			do
			{
				// within the loop increment token_index before calling STATEMENT
				token_index++;

				statement();
				if (error == -1) return;
			// loop while the current token is a semicolon
			} while (tokens[token_index].type == semicolon);

			// if tokens.type != keyword_end then Parser Error 6-4 | Parser Error 10-0
			if (tokens[token_index].type != keyword_end)
			{
				token_type = get_token_type();

				// Parser Error 6-4 (always stopping): missing ; after statement in begin-end
				// if the next token is in follow set [ ident call begin if while read write def return]
				if (token_type == identifier || token_type == keyword_call || token_type == keyword_begin ||
					token_type == keyword_if || token_type == keyword_while || token_type == keyword_read ||
					token_type == keyword_write || token_type == keyword_def || token_type == keyword_return)

				{
					print_parser_error(6, 4);
					error = -1;
					return;
				}
				// Parser Error 10: begin must be followed by end
				else
				{
					print_parser_error(10, 0);
					// Parser Error 10 (non-stopping): if token is in follow set [ . } ; ] (emit the end keyword, because we won't have this error if its present)
					if (token_type == period || token_type == right_curly_brace || token_type == semicolon)
					{
						error = 1;
					}
					// Parser Error 10 (stopping): if token isn't in follow set
					else
					{
						error = -1;
						return;
					}
				}
			}

			else
			{
				token_index++;
			}

			break;
		}

		// case keyword_if
		case keyword_if:
		{
			int jpc;
			token_index++;

			condition();
			if (error == -1) return;

			//save the location of the jpc instruction in the code array
			jpc = code_index;
			// when you emit the instruction, use 0 for the M value
			emit(JPC, 0, 0);

			// tokens.type != keyword_then then Parser Error 11: if must be followed by then
			if (tokens[token_index].type != keyword_then)
			{
				print_parser_error(11, 0);
				token_type = get_token_type();

				// Parser Error 11 (non stopping): if follow set is [ . } ; end ident call begin if while read write def return ]
				if (token_type == period || token_type == right_curly_brace || token_type == semicolon || token_type == keyword_end ||
					token_type == identifier || token_type == keyword_call || token_type == keyword_begin || token_type == keyword_if ||
					token_type == keyword_while || token_type == keyword_read || token_type == keyword_write || token_type == keyword_def ||
					token_type == keyword_read)
				{
					error = 1;
				}
				// Parser Error 11 (stopping): if token is not in the follow set
				else
				{
					error = -1;
					return;
				}
			}
			else
			{
				token_index++;
			}

			statement();
			if (error == -1) return;

			// you'll know where to jump to after you call STATEMENT: the index of the next instruction in the code array
			code[jpc].m = code_index;
			break;
		}

		case keyword_while:
		{
			int jpc, jmp;
			token_index++;

			// we'll need to save this information before we call CONDITION so save the jmp instruction
			jmp = code_index;

			condition();
			if (error == -1) return;

			// if tokens.type != keyword_do then Parser Error 12: while must be followed by do
			if (tokens[token_index].type != keyword_do)
			{
				print_parser_error(12, 0);

				token_type = get_token_type();

				// Parser Error 12 (non stopping): if next symbol is in follow set [ . } ; end ident call begin if while read write def return ]
				if (token_type == period || token_type == right_curly_brace || token_type == semicolon || token_type == keyword_end ||
					token_type == identifier || token_type == keyword_call || token_type == keyword_begin || token_type == keyword_if ||
					token_type == keyword_while || token_type == keyword_read || token_type == keyword_write || token_type == keyword_def ||
					token_type == keyword_return)
				{
					error = 1;
				}
				// Parser Error 12 (stopping): if next symbol isn't in follow set
				else
				{
					error = -1;
					return;
				}
			}
			else
			{
				token_index++;
			}

			// save the location of the JPC instruction in the code array
			jpc = code_index;
			// when you emit instruction, use 0 for the M value
			emit(JPC, 0, 0);

			statement();
			if (error == -1) return;

			// the M value should be the index of the first instruction of the condition calculation
			emit(JMP, 0, jmp);

			// we can fix the JPC's m value so it's the index of the next instruction code 
			code[jpc].m = code_index;
			break;
		}

		// case keyword_read
		case keyword_read:
		{
			token_index++;

			// if tokens.type != identifier then Parser Error 2-5: missing identifier after keyword_read
			if (tokens[token_index].type != identifier) 
			{
				print_parser_error(2, 5);

				token_type = get_token_type();
				// Parser Error 2-5 (non stopping): if next symbol is in follow set [ . } ; end ]
				if (token_type == period || token_type == right_curly_brace || token_type == semicolon || token_type == keyword_end)
				{
					error = 1;
				}
				// Parser Error 2-5 (stopping); if the next symbol isn't in the follow set
				else 
				{
					error = -1;
					return;
				}
			}

			else
			{
				// use support function find_symbol to get the index of the desired symbol in the symbol table
				symbol_idx = find_symbol(tokens[token_index].identifier_name, 2);

				// if find_symbol returns -1 you'll have an error
				if (symbol_idx == -1)
				{
					// Parser Error 8-3 (always nonstopping): Undeclared identifier used in read statement
					// find_symbol(desired name, kind 1) and find_symbol(desired name, kind 3) both equal -1
					if (find_symbol(tokens[token_index].identifier_name, 1) == -1 && find_symbol(tokens[token_index].identifier_name, 3) == -1)
					{
						print_parser_error(8,3);
					}
					// Parser Error 13: procedures and constants cannot be read
					// find_symbol(desired name, kind 1) or find_symbol(desired name, kind 3) return something other than -1
					else
					{
						print_parser_error(13,0);
					}
					error = 1;

					// if you had an error use -1 for both L and M
				}
				// no error occurred
				else
				{
					// L value is global level minus the level field of the symbol from the table
					l = level - table[symbol_idx].level;
					// M value is the address field of the symbol from the table
					m = table[symbol_idx].address;
				}

				token_index++;
			}

			emit(SYS, 0, RED);

			emit(STO, l, m);
			break;
		}

		// case keyword_write
		case keyword_write:
		{
			// increment token after keyword_write
			token_index++;
			
			// call EXPRESSION
			expression();
			// return if there was a stopping error
			if (error == -1) return;
			
			// emit WRT
			emit(SYS, 0, WRT);
			break;
		}

		// case keyword def
		case keyword_def:
		{
			int jmp;
			token_index++;

			// if tokens.type != identifier then Parser Error 2-6: missing identifier after keyword def
			if (tokens[token_index].type != identifier) 
			{
				print_parser_error(2,6);

				// Parser Error 2-6 (non stopping): if the next token is a left curly brace
				if (tokens[token_index].type == left_curly_brace)
				{
					error = 1;
					// if you have error 2-6 and it's non stopping, use a flag to remember not to save procedure
					symbol_idx = -1;
				}
				// Parser Error 2-6 (stopping): if next token is anything other than left curly brace
				else
				{
					error = -1;
					return;
				}
			}
			else
			{
				// use support function find_symbol to get the index of the desired symbol in the symbol table
				symbol_idx = find_symbol(tokens[token_index].identifier_name, 3);

				// if find_symbol returns -1 you'll have an error
				if (symbol_idx == -1)
				{
					// Parser Error 8-4 (always non stopping): undeclared identifier used in define statement
					// find_symbol(desired name, kind 1) and find_symbol(desired name, kind 2) both equal -1
					if (find_symbol(tokens[token_index].identifier_name, 1) == -1 && find_symbol(tokens[token_index].identifier_name, 2) == -1)
					{
						print_parser_error(8,4);
					}
					// Parser Error 14: variables and constant cannot be defined
					// find_symbol(desired name, kind 1) or find_symbol(desired name, kind 2) return something other than -1
					else
					{
						print_parser_error(14, 0);
					}
					error = 1;
				}

				else
				{
					// Parser Error 22: procedures can only be defined within the procedure that declares them
					// check that the level field of procedure in the symbol table if its not equal to the global level
					if (table[symbol_idx].level != level)
					{
						print_parser_error(22, 0);
						error = 1;
						symbol_idx = -1;
					}

					// Parser Error 23: procedures cannot be defined multiple times
					// if the procedure address is not -1 before we define it
					else if (table[symbol_idx].address != -1)
					{
						print_parser_error(23, 0);
						error = 1;
						symbol_idx = -1;
					}
				}

				token_index++;
			}

			// if tokens.type != left curly brace then Parser Error 15: missing {}
			if (tokens[token_index].type != left_curly_brace)
			{
				print_parser_error(15, 0);
				token_type = get_token_type();

				// Parser Error 15 (non stopping): if next token is in follow set [ const var procedure ident call begin if while read write def return } ]
				if (token_type == keyword_const || token_type == keyword_var || token_type == keyword_procedure || token_type == identifier ||
					token_type == keyword_call || token_type == keyword_begin || token_type == keyword_if || token_type == keyword_while ||
					token_type == keyword_read || token_type == keyword_write || token_type == keyword_def || token_type == right_curly_brace ||
					token_type == keyword_return)
				{
					error = 1;
				}
				// Parser Error 15 (stopping): if next token isn't in follow set
				else 
				{
					error = -1;
					return;
				}
			}
			else
			{
				token_index++;
			}
			
			// save the index of the JMP in the code array
			jmp = code_index;
			emit(JMP, 0, 0);

			// if symbol index != -1 add the procedure
			if (symbol_idx != -1)
				table[symbol_idx].address = code_index;

			block();
			if (error == -1) return;

			// if there was an explicit RTN, we don't need to do anything, otherwise emit a RTN instruction
			if (code[code_index - 1].op != RTN)
				emit(RTN, 0, 0);

			// we can fix the M value of the JMP before so it equals th eindex of the next instruction
			code[jmp].m = code_index;

			// if tokens.type != right curly brace Parser Error 16: { must be followed by }
			if (tokens[token_index].type != right_curly_brace)
			{
				print_parser_error(16,0);
				token_type = get_token_type();

				// Parser Error 16 (non stopping): if next token is in follow set [ . } ; end ] 
				if (token_type == period || token_type == right_curly_brace || token_type == semicolon || token_type == keyword_end)
				{
					error = 1;
				}

				// Parser Error 16 (stopping): if the next token isn't in follow set
				else
				{
					error = -1;
					return;
				}
			}
			else
			{
				token_index++;
			}
			break;
		}

		// case keyword_return
		case keyword_return:
		{
			// For main, emit a hlt and we know we're in main if global level is 0
			if (level == 0)
			{
				emit(SYS, 0, HLT);
			}
			else
			{
				emit(RTN, 0, 0);
			}

			token_index++;
			break; 
		}

		// the default case should just be a return
		default:
			return;
	}
}

// CONDITION ::= EXPRESSION (== | != | < | <= | > | >= | 17) EXPRESSION
//					(EQL | NEQ | LSS | GTR | GEQ | OPR )
void condition()
{
	int token_type;

	expression();
	if (error == -1) return;

	// must save which relation operator it is so you know which instruction to emit
	token_type = get_token_type();

	// if tokens.type doesn't equal the relational operators Parser Error 17: missing relational operator
	if (token_type != equal_to && token_type != not_equal_to && token_type != less_than && token_type != less_than_or_equal_to && token_type != greater_than && token_type != greater_than_or_equal_to)
	{
		print_parser_error(17, 0);

		// Parser Error 17 (non stopping): if the token is in follow set [ ident number ( ]
		if (token_type == identifier || token_type == number || token_type == left_parenthesis)
		{
			error = 1;
		}
		// Parser Error 17 (stopping): if next token wasn't in follow set
		else
		{
			error = -1;
			return;
		}
	}

	else
	{
		token_index++;
	}

	expression();
	if (error == -1) return;

	// check which relation operator the token type was to emit
	switch (token_type)
	{
		case equal_to:
			emit(OPR, 0, EQL);
			break;

		case not_equal_to:
			emit(OPR, 0, NEQ);
			break;

		case less_than:
			emit(OPR, 0, LSS);
			break;

		case less_than_or_equal_to:
			emit(OPR, 0, LEQ);
			break;

		case greater_than:
			emit(OPR, 0, GTR);
			break;

		case greater_than_or_equal_to:
			emit(OPR, 0, GEQ);
			break;

		// if we encounter an error use -1 for M value
		default:
			emit(OPR, 0, -1);
			break;
	}
}

// EXPRESSION ::= TERM { (+ | -) TERM (ADD | SUB) }
void expression()
{
	int token_type;

	term();
	if (error == -1) return; 

	// save the token type so you know which instruction to emit after the second call
	token_type = get_token_type();
	// loop while the current token is plus or minus
	while (token_type == plus || token_type == minus)
	{
		token_index++;

		term();
		if (error == -1) return;

		// use the saved token_type to determine which emit instruction to emit
		if (token_type == plus)
		{
			emit(OPR, 0, ADD);
		}
		else
		{
			emit(OPR, 0, SUB);
		}

		token_type = get_token_type();
	}
}

// TERM ::= FACTOR { (* | /) FACTOR (MUL | DIV) }
void term()
{
	int token_type;

	factor();
	if (error == -1) return;

	// save the token_type to determine which instruction to emit
	token_type = get_token_type();
	// loop while the current token is times or division
	while (token_type == times || token_type == division)
	{
		token_index++;

		factor();
		if (error == -1) return;

		// use the saved token_type to determine which emit instruction to emit
		if (token_type == division)
		{
			emit(OPR, 0, DIV);
		}
		else
		{
			emit(OPR, 0, MUL);
		}

		token_type = get_token_type();
	}
}

// FACTOR ::= ident [8-5 | 18] (LOD | LIT) | number LIT | 
//				( EXPRESSION ( ')' | 19) | 20
void factor()
{
	int token_type;
	int const_idx, var_idx;

	// if tokens.type == identifier
	if (tokens[token_index].type == identifier)
	{
		// use find_symbol for desired name for kind 1 (constant) and kind 2 (variable)
		const_idx = find_symbol(tokens[token_index].identifier_name, 1);
		var_idx = find_symbol(tokens[token_index].identifier_name, 2);

		// if both const and variable both returned -1 you'll have an error
		if (const_idx == -1 && var_idx == -1)
		{
			// Parser Error 8-5 (always non stopping): undeclared identifier used in arithmetic expression
			// if find_symbol(desired name, kind 1) and find_symbol(desired name, kind 2) and find_symbol(desired name, kind 3) all returned -1
			if (find_symbol(tokens[token_index].identifier_name, 3) == -1)
			{
				print_parser_error(8, 5);
				error = 1;
			}

			// Parser Errro 18 (always non stopping): procedures cannot be used in arithmetic
			// if find_symbol(desired name, kind 3) returned something other than -1
			else
			{
				print_parser_error(18, 0);
				error = 1;
			}

			// if we encounter error use -1 for both L and M
			emit(LOD, -1, -1);
		}

		else
		{
			// if it returns just a variable
			if (const_idx == -1)
			{
				// emit a LOD instruction
				// L is the global level minus the level of the symbol from the table
				// M is the address of the symbol from the table
				emit(LOD, level - table[var_idx].level, table[var_idx].address);
			}
			// if it returns just a const
			else if (var_idx == -1)
			{
				// emit a LIT instruction
				// M is the value of the symbol from the table
				emit(LIT, 0, table[const_idx].value);
			}
			// if it returns an index for both
			// if const level > var level
			else if (table[const_idx].level > table[var_idx].level)
			{
				// emit a LIT instruction
				// the M value is the value of the symbol from the table
				emit(LIT, 0, table[const_idx].value);
			}
			// if const level < var level
			else
			{
				// emit a LOD instruction
				// L is the global level minus the level of the symbol from the table
				// M is the address of the symbol from the table
				emit(LOD, level - table[var_idx].level, table[var_idx].address);
			}
		}

		token_index++;
	}

	// The number case
	else if (tokens[token_index].type == number)
	{
		// emit the LIT instruction
		// the M field of the LIT instruction is the value of the number token
		emit(LIT, 0, tokens[token_index].number_value);
		token_index++;
	}

	// if tokens.type == left parenthesis
	else if (tokens[token_index].type == left_parenthesis)
	{
		token_index++;

		expression();
		if (error == -1) return;

		// if tokens.type != right parenthesis Parser Error 19: ( must be followed by )
		if (tokens[token_index].type != right_parenthesis)
		{
			print_parser_error(19, 0);
			token_type = get_token_type();

			// Parser Error 19 (non stopping): if the token is in follow set [ * / + - . } ; end == != < <= > >= then do ]
			if (token_type == times || token_type == division || token_type == minus || token_type == period || token_type == right_curly_brace || token_type == semicolon ||
				token_type == keyword_end || token_type == equal_to || token_type == not_equal_to || token_type == less_than || token_type == less_than_or_equal_to ||
				token_type == greater_than || token_type == greater_than_or_equal_to || token_type == keyword_then || token_type == keyword_do)
			{
				error = 1;
			}
			// Parser Error 19 (stopping): if the token isn't in the follow set
			else
			{
				error = -1;
				return;
			}
		}
		else
		{
			token_index++;
		}
	}

	// if the current token is not a number, ident, or left parenthesis you have Parser Error 20: invalid expression
	else
	{
		print_parser_error(20, 0);

		token_type = get_token_type();

		// Parser Error 20 (non stopping): if token is in follow set [ * / + - . } ; end == != < <= > >= then do ]
		if (token_type == times || token_type == division || token_type == minus || token_type == period || token_type == right_curly_brace || token_type == semicolon ||
			token_type == keyword_end || token_type == equal_to || token_type == not_equal_to || token_type == less_than || token_type == less_than_or_equal_to ||
			token_type == greater_than || token_type == greater_than_or_equal_to || token_type == keyword_then || token_type == keyword_do)
		{
			error = 1;
		}
		// Parser Error 20 (stopping): if the token isn't in the follow set
		else
		{
			error = -1;
			return;
		}
	}
}

int find_symbol(char name[], int kind)
{
	int i;
	int max_idx = -1;
	int max_lvl = -1;
	for (i = 0; i < table_index; i++)
	{
		if (table[i].mark == 0 && table[i].kind == kind && strcmp(name, table[i].name) == 0)
		{
			if (max_idx == -1 || table[i].level > max_lvl)
			{
				max_idx = i;
				max_lvl = table[i].level;
			}
		}
	}
	return max_idx;
}

int get_token_type()
{
	return tokens[token_index].type;
}

void emit(int op, int l, int m)
{
	code[code_index].op = op;
	code[code_index].l = l;
	code[code_index].m = m;
	code_index++;
}

void add_symbol(int kind, char name[], int value, int level, int address)
{
	table[table_index].kind = kind;
	strcpy(table[table_index].name, name);
	table[table_index].value = value;
	table[table_index].level = level;
	table[table_index].address = address;
	table[table_index].mark = 0;
	table_index++;
}

void mark()
{
	int i;
	for (i = table_index - 1; i >= 0; i--)
	{
		if (table[i].mark == 1)
			continue;
		if (table[i].level < level)
			return;
		table[i].mark = 1;
	}
}

int multiple_declaration_check(char name[])
{
	int i;
	for (i = 0; i < table_index; i++)
		if (table[i].mark == 0 && table[i].level == level && strcmp(name, table[i].name) == 0)
			return i;
	return -1;
}

void print_parser_error(int error_code, int case_code)
{
	switch (error_code)
	{
	case 1:
		printf("Parser Error 1: missing . \n");
		break;
	case 2:
		switch (case_code)
		{
		case 1:
			printf("Parser Error 2: missing identifier after keyword const\n");
			break;
		case 2:
			printf("Parser Error 2: missing identifier after keyword var\n");
			break;
		case 3:
			printf("Parser Error 2: missing identifier after keyword procedure\n");
			break;
		case 4:
			printf("Parser Error 2: missing identifier after keyword call\n");
			break;
		case 5:
			printf("Parser Error 2: missing identifier after keyword read\n");
			break;
		case 6:
			printf("Parser Error 2: missing identifier after keyword def\n");
			break;
		default:
			printf("Implementation Error: unrecognized error code\n");
		}
		break;
	case 3:
		printf("Parser Error 3: identifier is declared multiple times by a procedure\n");
		break;
	case 4:
		switch (case_code)
		{
		case 1:
			printf("Parser Error 4: missing := in constant declaration\n");
			break;
		case 2:
			printf("Parser Error 4: missing := in assignment statement\n");
			break;
		default:
			printf("Implementation Error: unrecognized error code\n");
		}
		break;
	case 5:
		printf("Parser Error 5: missing number in constant declaration\n");
		break;
	case 6:
		switch (case_code)
		{
		case 1:
			printf("Parser Error 6: missing ; after constant declaration\n");
			break;
		case 2:
			printf("Parser Error 6: missing ; after variable declaration\n");
			break;
		case 3:
			printf("Parser Error 6: missing ; after procedure declaration\n");
			break;
		case 4:
			printf("Parser Error 6: missing ; after statement in begin-end\n");
			break;
		default:
			printf("Implementation Error: unrecognized error code\n");
		}
		break;
	case 7:
		printf("Parser Error 7: procedures and constants cannot be assigned to\n");
		break;
	case 8:
		switch (case_code)
		{
		case 1:
			printf("Parser Error 8: undeclared identifier used in assignment statement\n");
			break;
		case 2:
			printf("Parser Error 8: undeclared identifier used in call statement\n");
			break;
		case 3:
			printf("Parser Error 8: undeclared identifier used in read statement\n");
			break;
		case 4:
			printf("Parser Error 8: undeclared identifier used in define statement\n");
			break;
		case 5:
			printf("Parser Error 8: undeclared identifier used in arithmetic expression\n");
			break;
		default:
			printf("Implementation Error: unrecognized error code\n");
		}
		break;
	case 9:
		printf("Parser Error 9: variables and constants cannot be called\n");
		break;
	case 10:
		printf("Parser Error 10: begin must be followed by end\n");
		break;
	case 11:
		printf("Parser Error 11: if must be followed by then\n");
		break;
	case 12:
		printf("Parser Error 12: while must be followed by do\n");
		break;
	case 13:
		printf("Parser Error 13: procedures and constants cannot be read\n");
		break;
	case 14:
		printf("Parser Error 14: variables and constants cannot be defined\n");
		break;
	case 15:
		printf("Parser Error 15: missing {\n");
		break;
	case 16:
		printf("Parser Error 16: { must be followed by }\n");
		break;
	case 17:
		printf("Parser Error 17: missing relational operator\n");
		break;
	case 18:
		printf("Parser Error 18: procedures cannot be used in arithmetic\n");
		break;
	case 19:
		printf("Parser Error 19: ( must be followed by )\n");
		break;
	case 20:
		printf("Parser Error 20: invalid expression\n");
		break;
	case 21:
		printf("Parser Error 21: procedure being called has not been defined\n");
		break;
	case 22:
		printf("Parser Error 22: procedures can only be defined within the procedure that declares them\n");
		break;
	case 23:
		printf("Parser Error 23: procedures cannot be defined multiple times\n");
		break;
	default:
		printf("Implementation Error: unrecognized error code\n");
	}
}

void print_assembly_code()
{
	int i;
	printf("Assembly Code:\n");
	printf("Line\tOP Code\tOP Name\tL\tM\n");
	for (i = 0; i < code_index; i++)
	{
		printf("%d\t%d\t", i, code[i].op);
		switch (code[i].op)
		{
		case LIT:
			printf("LIT\t");
			break;
		case OPR:
			switch (code[i].m)
			{
			case ADD:
				printf("ADD\t");
				break;
			case SUB:
				printf("SUB\t");
				break;
			case MUL:
				printf("MUL\t");
				break;
			case DIV:
				printf("DIV\t");
				break;
			case EQL:
				printf("EQL\t");
				break;
			case NEQ:
				printf("NEQ\t");
				break;
			case LSS:
				printf("LSS\t");
				break;
			case LEQ:
				printf("LEQ\t");
				break;
			case GTR:
				printf("GTR\t");
				break;
			case GEQ:
				printf("GEQ\t");
				break;
			default:
				printf("err\t");
				break;
			}
			break;
		case LOD:
			printf("LOD\t");
			break;
		case STO:
			printf("STO\t");
			break;
		case CAL:
			printf("CAL\t");
			break;
		case RTN:
			printf("RTN\t");
			break;
		case INC:
			printf("INC\t");
			break;
		case JMP:
			printf("JMP\t");
			break;
		case JPC:
			printf("JPC\t");
			break;
		case SYS:
			switch (code[i].m)
			{
			case WRT:
				printf("WRT\t");
				break;
			case RED:
				printf("RED\t");
				break;
			case HLT:
				printf("HLT\t");
				break;
			default:
				printf("err\t");
				break;
			}
			break;
		default:
			printf("err\t");
			break;
		}
		printf("%d\t%d\n", code[i].l, code[i].m);
	}
	printf("\n");
}

void print_symbol_table()
{
	int i;
	printf("Symbol Table:\n");
	printf("Kind | Name        | Value | Level | Address | Mark\n");
	printf("---------------------------------------------------\n");
	for (i = 0; i < table_index; i++)
		printf("%4d | %11s | %5d | %5d | %5d | %5d\n", table[i].kind, table[i].name, table[i].value, table[i].level, table[i].address, table[i].mark);
	printf("\n");
}