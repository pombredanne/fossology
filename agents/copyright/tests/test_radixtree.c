/*********************************************************************
Copyright (C) 2010 Hewlett-Packard Development Company, L.P.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
version 2 as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*********************************************************************/

/* includes for files that will be tested */
#include <radixtree.h>
#include <cvector.h>

/* library includes */
#include <stdio.h>
#include <string.h>

/* cunit includes */
#include <CUnit/CUnit.h>

/* external function to test if a particular test failed */
extern void (*test_failure)(void);

/* ************************************************************************** */
/* **** radix local declarations ******************************************** */
/* ************************************************************************** */

typedef struct tree_internal node;

struct tree_internal
{
  char character;               // the character represented by this node
  int terminal;                 // true if a word terminates at this node, false otherwise
  node* children[NODE_SIZE];    // the list of children of this node
};

void radix_init_local(radix_tree* tree_ptr, char value);
int radix_match_local(radix_tree tree, char* dst, char* src);
void radix_append(radix_tree tree, cvector vec, char* string);

/* ************************************************************************** */
/* **** cunit tests ********************************************************* */
/* ************************************************************************** */

void test_radix_init_local()
{
  radix_tree tree;

  /* start the test */
  printf("Test radix_init_local: ");

  /* run the functions */
  radix_init_local(&tree, 'a');

  /* do the asserts */
  CU_ASSERT_EQUAL(tree->character, 'a');
  CU_ASSERT_FALSE(tree->terminal);
  CU_ASSERT_EQUAL(tree->children[10], NULL);

  radix_destroy(tree);
  test_failure();
  printf("\n");
}

void test_radix_init()
{
  radix_tree tree;

  /* start the test */
  printf("Test radix_init: ");

  /* run the functions */
  radix_init(&tree);

  /* do the asserts */
  CU_ASSERT_EQUAL(tree->character, '\0');
  CU_ASSERT_FALSE(tree->terminal);
  CU_ASSERT_EQUAL(tree->children[10], NULL);

  radix_destroy(tree);
  test_failure();
  printf("\n");
}

void test_radix_insert()
{
  radix_tree tree;

  /* start the test */
  printf("Test radix_insert: ");

  /* run the functions */
  radix_init(&tree);
  radix_insert(tree, "hi");

  /* do the asserts */
  CU_ASSERT_NOT_EQUAL(tree->children['h'], NULL);
  CU_ASSERT_EQUAL(tree->children['h']->character, 'h');
  CU_ASSERT_NOT_EQUAL(tree->children['h']->children['i'], NULL);
  CU_ASSERT_EQUAL(tree->children['h']->children['i']->character, 'i');
  CU_ASSERT_EQUAL(tree->children['h']->children['j'], NULL);
  CU_ASSERT_EQUAL(tree->children['h']->children['k'], NULL);

  radix_destroy(tree);
  test_failure();
  printf("\n");
}

void test_radix_insert_all()
{
  radix_tree tree;
  char* Dict[3] = {"hello", "world", "!"};

  /* start the test */
  printf("Test radix_insert_all: ");

  /* run the functions */
  radix_init(&tree);
  radix_insert_all(tree, Dict, Dict + 3);

  /* do the asserts */
  CU_ASSERT_NOT_EQUAL(tree->children['h'], NULL);
  CU_ASSERT_NOT_EQUAL(tree->children['w'], NULL);
  CU_ASSERT_NOT_EQUAL(tree->children['!'], NULL);
  CU_ASSERT_EQUAL(tree->children['h']->character, 'h');
  CU_ASSERT_EQUAL(tree->children['w']->character, 'w');
  CU_ASSERT_EQUAL(tree->children['!']->character, '!');

  radix_destroy(tree);
  test_failure();
  printf("\n");
}

void test_radix_contains()
{
  radix_tree tree;

  /* start the test */
  printf("Test radix_contains: ");

  /* run the functions */
  radix_init(&tree);
  radix_insert(tree, "hello");
  radix_insert(tree, "world");

  /* do the asserts */
  CU_ASSERT_TRUE(radix_contains(tree, "hello"));
  CU_ASSERT_FALSE(radix_contains(tree, "hello "));
  CU_ASSERT_TRUE(radix_contains(tree, "world"));
  CU_ASSERT_FALSE(radix_contains(tree, "worl"));

  radix_destroy(tree);
  test_failure();
  printf("\n");
}

void test_radix_match_local()
{
  radix_tree tree;
  char buf[1024];

  /* start the test */
  printf("Test radix_match_local: ");

  /* run the functions */
  radix_init(&tree);
  radix_insert(tree, "hello ");
  memset(buf, '\0', sizeof(buf));

  /* do the asserts */
  CU_ASSERT_EQUAL(radix_match_local(tree, buf, "hello "), 6);
  CU_ASSERT_EQUAL(radix_match_local(tree, buf, "hello"), 5);
  CU_ASSERT_EQUAL(radix_match_local(tree, buf, "world "), 0);
  CU_ASSERT_EQUAL(radix_match_local(tree, buf, "help"), 3);
  CU_ASSERT_TRUE(!strcmp(buf, "hello hellohel"));

  radix_destroy(tree);
  test_failure();
  printf("\n");
}

void test_radix_match()
{
  radix_tree tree;
  char buf[1024];

  /* start the test */
  printf("Test radix_match: ");

  /* run the functions */
  radix_init(&tree);
  radix_insert(tree, "hello ");
  memset(buf, '\0', sizeof(buf));

  /* do the asserts */
  CU_ASSERT_EQUAL(radix_match(tree, buf, "hello "), 6);
  CU_ASSERT_TRUE(!strcmp(buf, "hello "));
  CU_ASSERT_EQUAL(radix_match(tree, buf, "hello"), 5);
  CU_ASSERT_TRUE(!strcmp(buf, "hello"));
  CU_ASSERT_EQUAL(radix_match(tree, buf, "world "), 0);
  CU_ASSERT_TRUE(!strcmp(buf, ""));
  CU_ASSERT_EQUAL(radix_match(tree, buf, "help"), 3);
  CU_ASSERT_TRUE(!strcmp(buf, "hel"));

  radix_destroy(tree);
  test_failure();
  printf("\n");
}

/* ************************************************************************** */
/* **** cunit test info ***************************************************** */
/* ************************************************************************** */

CU_TestInfo radix_testcases[] =
{
    {"Testing radix_init_local:", test_radix_init_local},
    {"Testing radix_init:", test_radix_init},
    {"Testing radix_insert:", test_radix_insert},
    {"Testing radix_insert_all", test_radix_insert_all},
    {"Testing radix_contains:", test_radix_contains},
    {"Testing radix_match_local:", test_radix_match_local},
    {"Testing radix_match:", test_radix_match},
    CU_TEST_INFO_NULL
};
