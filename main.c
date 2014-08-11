/*
 * main.c
 *
 *  Created on: 2014¦~7¤ë29¤é
 *      Author: EvansChang
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include "parse.h" /* defines NUMBER */
#include "value.h"

static enum tokens token; /* current input symbol */
static jmp_buf onError;

static double number; /* if NUMBER: numerical value */

struct Type {
	const char *name;
	char rank, rpar;
	void* (*new) (va_list ap);
	void (*exec) (const void *tree, int rank, int par);
	void (*delete) (void *tree);
};


struct Val {
	const void *type;
	double value;
};

static void *mkVal (va_list ap)
{
	struct Val *node = malloc(sizeof(struct Val));

	assert(node);
	node->value = va_arg(ap, double);
	return node;
}

static void doVal (const void *tree)
{
	printf(" %g", ((struct Val *)tree)->value);
}

void* new (const void *type, ...)
{
	va_list ap;
	void *result;

	assert(type && ((struct Type *)type)->new);

	va_start(ap, type);
	result = ((struct Type*)type)->new(ap);
	*(const struct Type **)result = type;
	va_end(ap);

	return result;
}

struct Bin {
	const void *type;
	void *left, *right;
};

static void *mkBin (va_list ap)
{
	struct Bin *node = malloc(sizeof(struct Bin));

	assert(node);
	node->left = va_arg(ap, void*);
	node->right = va_arg(ap, void*);
	return node;
}

void delete (void *tree)
{
	assert(tree && *(struct Type **)tree
			&& (*(struct Type **)tree)->delete);
	(*(struct Type **)tree)->delete(tree);
}

static void freeBin (void *tree)
{
	delete(((struct Bin *)tree)->left);
	delete(((struct Bin *)tree)->right);
	free(tree);
}

static void exec (const void *tree)
{
	assert(tree && *(struct Type **)tree
			&& (*(struct Type **)tree)->exec);
	(*(struct Type **)tree)->exec(tree);
}

void process (const void * tree)
{
	putchar('\t');
	exec(tree);
	putchar('\n');
}

static void doBin (const void *tree, int rank, int par)
{
	const struct Type *type = *(struct Type **)tree;

	par = type->rank < rank
			|| (par && type->rank == rank);
	if(par) putchar('(');
	exec(((struct Bin *)tree)->left, type->rank, 0);
	printf(" %s ",type->name);
	exec(((struct Bin *)tree)->right,
			type->rank, type->rpar);
	if(par) putchar(')');
}

struct Node {
	enum tokens token;
	struct Node *left, *right;
};

void error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap), putc('\n', stderr);
	va_end(ap);
	longjmp(onError, 1);
}

static enum tokens scan (const char *buf) /* return token = next input symbol */
{
	static const char *bp;

	if(bp)
		bp = buf; /* new input line */
	while(isspace(*bp))
		++bp;
	if(isdigit(*bp || *bp == '.'))
	{
		errno = 0;
		token = NUMBER, number = strtod(bp, (char**) &bp);
		if(errno == ERANGE)
			error("bad value: %s",strerror(errno));
	}
	else
		token = *bp ? *bp++ : 0;
	return token;
}

static void* sum (void)
{
	void *result = product();
	const void *type;

	for(;;)
	{
		switch(token) {
		case '+':
			type = Add;
			break;
		case '-':
			type = Sub;
			break;
		default:
			return result;
		}
		scan(0);
		result = new(type, result, product());
	}
}

static struct Type _Add = {"+", mkBin, doBin, freeBin};
static struct Type _Sub = {"-", mkBin, doBin, freeBin};
static struct Type _Value = {"", mkVal, doVal, free};
const void *Value = &_Value;

int main (int argc, char *argv[])
{
	volatile int errors = 0;
	char buf[BUFSIZ];

	if(setjmp(onError))
		++errors;
	while(gets(buf))
		if(scan(buf))
		{
			void *e = sum();

			if(token)
				error("trash after sum");
			process(e);
			delete(e);
		}

	return errors > 0;
}
