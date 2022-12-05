// We need at least 30 points on this programming assignment to get an A in the clas

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
int var_declaration(int number_of_variables);
int proc_declaration();
void statement();
void condition();
void expression();
void term();
void factor();
int rel_operator();

instruction *parse(int code_flag, int table_flag, lexeme *list)
{
	tokens = list;
	table = calloc(ARRAY_SIZE, sizeof(symbol));
	code = calloc(ARRAY_SIZE, sizeof(instruction));

	program();

	code[code_index].op = -1;

	if (table_flag) {
		print_symbol_table();
	}

	if (code_flag) {
		print_assembly_code();
	}
	
}

void program() {
	emit(7, 0, 0);
	add_symbol(3, "main", 0, 0, 0);
	level = -1;
	block();

	if(error == -1) return;

	// Parser error 1 (missing .)
	if (tokens[token_index].type != period) {
		print_parser_error(1, 0);
		error = 1;
	}

	for (int i = 0; i < code_index; i++) {
		if (code[i].op == 5) {
			if (code[i].m == 0) {
				// Error 21
				print_parser_error(21,0);
			}

			code[i].m = table[code[i].m].address;
		}
	}

	
}

void block() {
	level++;
	declaractions();
	statement();
	mark();
	level--;
}

void declaractions() {
	int vars_declared = 0;

	while (tokens[token_index].type == (keyword_const || keyword_var || keyword_procedure)) {

		if (tokens[token_index].type == keyword_const) {
			const_declaration();
		}

		else if (tokens[token_index].type == keyword_var) {
			vars_declared = var_declaration(vars_declared);
		}

		else if (tokens[token_index].type == keyword_procedure) {
			proc_declaration();
		}
	}

	emit(7, 0, vars_declared + 3);
}

void const_declaration() {
	
}

int var_declaration(int number_of_variables) {
	return 0;
}

void condition() {
	int type;

	expression();
	if (error == -1) return;

	type = tokens[token_index].type;

	// missing relational operator
	// need to change this logic around to avoid looking like his
	if (type != equal_to && type != not_equal_to && type != less_than && type != less_than_or_equal_to && type != greater_than && type != greater_than_or_equal_to) {
		print_parser_error(17, 0);

		// im not sure why this is here. why check identifier number and left parenthesis
		if (type == identifier || type == number || type == left_parenthesis)
			error = 1;
		
		else {
			error = -1;
			return;
		}
	} 

	else 
		token_index++;
	

	expression();
	if (error == -1) return;

	switch (type) {
		case equal_to:
			emit(OPR, 0, EQL);
			break;

		case not_equal_to:
			emit(OPR, 0, NEQ);
			break;

		case less_than:
			emit(OPR, 0, LSS);
			break;

		case greater_than:
			emit(OPR, 0, GTR);
			break;

		case greater_than_or_equal_to:
			emit(OPR, 0, GEQ);
			break;
		
		default:
			emit(OPR, 0, -1);
			break;
	}
}

void expression() {
	// int type;

	term();

	if (error == -1) return; // found stopping error

	// type = tokens[token_index].type;

	while (tokens[token_index].type == plus || tokens[token_index].type == minus) {
		token_index++;
		term();

		if (tokens[token_index].type == plus)
			emit(OPR, 0, ADD);
		else
			emit(OPR, 0, SUB);

		// type = tokens[token_index].type;
	}
}

void term() {
	// int type;

	factor();
	if (error == -1) return; // found stopping error

	// type = tokens[token_index].type;

	while (tokens[token_index].type == times || tokens[token_index].type == division) {
		token_index++;

		factor();

		if (tokens[token_index].type == times)
			emit(OPR, 0, MUL);
		else
			emit(OPR, 0, DIV);

		// type = tokens[token_index].type;
	}
}

void factor() {
	int type;
	int const_idx, var_idx;

	if (tokens[token_index].type == identifier) {
		const_idx = find_symbol(tokens[token_index].identifier_name, 1);
		var_idx = find_symbol(tokens[token_index].identifier_name, 2);

		if (const_idx == var_idx) {

			if (find_symbol(tokens[token_index].identifier_name, 1) == -1) {
				print_parser_error(8, 5);
				error = 1;
			}

			else {
				print_parser_error(18,0);
				error = 1;
			}

			emit(LOD, -1, -1);
		}

		else 
		{
			if (const_idx == -1)
				emit(LOD, level - table[var_idx].level, table[var_idx].address);
			else if (var_idx == -1)
				emit(LIT, 0, table[const_idx].value);
			else if (table[const_idx].level > table[var_idx].level)
				emit(LIT, 0, table[const_idx].value);
			else 
				emit(LOD, level - table[var_idx].level, table[var_idx].address);
			
			token_index++;
		}
	}

	else if (tokens[token_index].type == number) {
		emit(LIT, 0, tokens[token_index].number_value);
		token_index++;
	}

	else if (tokens[token_index].type == left_parenthesis)
	{
		token_index++;

		expression();
		if (error == -1) return;
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
		case 1 :
			printf("Parser Error 1: missing . \n");
			break;
		case 2 :
			switch (case_code)
			{
				case 1 :
					printf("Parser Error 2: missing identifier after keyword const\n");
					break;
				case 2 :
					printf("Parser Error 2: missing identifier after keyword var\n");
					break;
				case 3 :
					printf("Parser Error 2: missing identifier after keyword procedure\n");
					break;
				case 4 :
					printf("Parser Error 2: missing identifier after keyword call\n");
					break;
				case 5 :
					printf("Parser Error 2: missing identifier after keyword read\n");
					break;
				case 6 :
					printf("Parser Error 2: missing identifier after keyword def\n");
					break;
				default :
					printf("Implementation Error: unrecognized error code\n");
			}
			break;
		case 3 :
			printf("Parser Error 3: identifier is declared multiple times by a procedure\n");
			break;
		case 4 :
			switch (case_code)
			{
				case 1 :
					printf("Parser Error 4: missing := in constant declaration\n");
					break;
				case 2 :
					printf("Parser Error 4: missing := in assignment statement\n");
					break;
				default :				
					printf("Implementation Error: unrecognized error code\n");
			}
			break;
		case 5 :
			printf("Parser Error 5: missing number in constant declaration\n");
			break;
		case 6 :
			switch (case_code)
			{
				case 1 :
					printf("Parser Error 6: missing ; after constant declaration\n");
					break;
				case 2 :
					printf("Parser Error 6: missing ; after variable declaration\n");
					break;
				case 3 :
					printf("Parser Error 6: missing ; after procedure declaration\n");
					break;
				case 4 :
					printf("Parser Error 6: missing ; after statement in begin-end\n");
					break;
				default :				
					printf("Implementation Error: unrecognized error code\n");
			}
			break;
		case 7 :
			printf("Parser Error 7: procedures and constants cannot be assigned to\n");
			break;
		case 8 :
			switch (case_code)
			{
				case 1 :
					printf("Parser Error 8: undeclared identifier used in assignment statement\n");
					break;
				case 2 :
					printf("Parser Error 8: undeclared identifier used in call statement\n");
					break;
				case 3 :
					printf("Parser Error 8: undeclared identifier used in read statement\n");
					break;
				case 4 :
					printf("Parser Error 8: undeclared identifier used in define statement\n");
					break;
				case 5 :
					printf("Parser Error 8: undeclared identifier used in arithmetic expression\n");
					break;
				default :				
					printf("Implementation Error: unrecognized error code\n");
			}
			break;
		case 9 :
			printf("Parser Error 9: variables and constants cannot be called\n");
			break;
		case 10 :
			printf("Parser Error 10: begin must be followed by end\n");
			break;
		case 11 :
			printf("Parser Error 11: if must be followed by then\n");
			break;
		case 12 :
			printf("Parser Error 12: while must be followed by do\n");
			break;
		case 13 :
			printf("Parser Error 13: procedures and constants cannot be read\n");
			break;
		case 14 :
			printf("Parser Error 14: variables and constants cannot be defined\n");
			break;
		case 15 :
			printf("Parser Error 15: missing {\n");
			break;
		case 16 :
			printf("Parser Error 16: { must be followed by }\n");
			break;
		case 17 :
			printf("Parser Error 17: missing relational operator\n");
			break;
		case 18 :
			printf("Parser Error 18: procedures cannot be used in arithmetic\n");
			break;
		case 19 :
			printf("Parser Error 19: ( must be followed by )\n");
			break;
		case 20 :
			printf("Parser Error 20: invalid expression\n");
			break;
		case 21 :
			printf("Parser Error 21: procedure being called has not been defined\n");
			break;
		case 22 :
			printf("Parser Error 22: procedures can only be defined within the procedure that declares them\n");
			break;
		case 23 :
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
		switch(code[i].op)
		{
			case LIT :
				printf("LIT\t");
				break;
			case OPR :
				switch (code[i].m)
				{
					case ADD :
						printf("ADD\t");
						break;
					case SUB :
						printf("SUB\t");
						break;
					case MUL :
						printf("MUL\t");
						break;
					case DIV :
						printf("DIV\t");
						break;
					case EQL :
						printf("EQL\t");
						break;
					case NEQ :
						printf("NEQ\t");
						break;
					case LSS :
						printf("LSS\t");
						break;
					case LEQ :
						printf("LEQ\t");
						break;
					case GTR :
						printf("GTR\t");
						break;
					case GEQ :
						printf("GEQ\t");
						break;
					default :
						printf("err\t");
						break;
				}
				break;
			case LOD :
				printf("LOD\t");
				break;
			case STO :
				printf("STO\t");
				break;
			case CAL :
				printf("CAL\t");
				break;
			case RTN :
				printf("RTN\t");
				break;
			case INC :
				printf("INC\t");
				break;
			case JMP :
				printf("JMP\t");
				break;
			case JPC :
				printf("JPC\t");
				break;
			case SYS :
				switch (code[i].m)
				{
					case WRT :
						printf("WRT\t");
						break;
					case RED :
						printf("RED\t");
						break;
					case HLT :
						printf("HLT\t");
						break;
					default :
						printf("err\t");
						break;
				}
				break;
			default :
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