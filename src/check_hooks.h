#ifndef CHECK_HOOKS_H
#define CHECK_HOOKS_H

#include "tree.h"
#include "selint_error.h"

enum style_ids {
	S_ID_FC_TYPE = 2
};

enum error_ids {
	E_ID_FC_ERROR = 2,
	E_ID_FC_USER = 3,
	E_ID_FC_ROLE = 4,
	E_ID_FC_TYPE = 5
};

enum fatal_ids {
	F_ID_POLICY_SYNTAX = 1,
	F_ID_INTERNAL = 2
};

enum file_flavor {
	FILE_TE_FILE,
	FILE_IF_FILE,
	FILE_FC_FILE
};

struct check_data {
	char *mod_name;
	char *filename;
	enum file_flavor flavor;
};

// A check is responsible for filling out all fields except lineno
// which is filled out by the calling function.`
struct check_result {
	unsigned int lineno;
	char severity;
	unsigned int check_id;
	char *message;
};

struct check_node {
	struct check_result * (*check_function)(const struct check_data *data, const struct policy_node *node);
	struct check_node *next;
};

struct checks {
	struct check_node *fc_entry_node_checks;
	struct check_node *error_node_checks;
};

/*********************************************
 * Add an check to be called on check_flavor nodes
 * check_flavor - The flavor of node to call the check for
 * ck - The check structure to add the check to
 * check_function - the check to add
 * returns SELINT_SUCCESS or an error code on failure
 *********************************************/
enum selint_error add_check(enum node_flavor check_flavor, struct checks *ck, struct check_result * (*check_function)(const struct check_data *check_data, const struct policy_node *node));

/*********************************************
 * Call all registered checks for node->flavor node types
 * and write any error messages to STDOUT 
 * ck - The checks structure
 * data - Metadata about the file
 * node - the node to check
 * returns SELINT_SUCCESS or an error code on failure
 *********************************************/
enum selint_error call_checks(struct checks *ck, struct check_data *data, struct policy_node *node);

/*********************************************
 * Helper function for call_checks that takes the appropriate
 * list of checks for the node flavor and writes any error messages to STDOUT
 * ck_list - The checks to run
 * data - Metadata about the file
 * node - the node to check
 * returns SELINT_SUCCESS or an error code on failure
 *********************************************/
enum selint_error call_checks_for_node_type(struct check_node *ck_list, struct check_data *data, struct policy_node *node);

/*********************************************
 * Display a result message for a positive check finding
 * res - Information about the result of the check
 * data - Metadata about the file
 *********************************************/
void display_check_result(struct check_result *res, struct check_data *data);

struct check_result *alloc_internal_error(char *string);

void free_check_result(struct check_result *);

void free_checks(struct checks *to_free);

void free_check_node(struct check_node *to_free);

#endif
