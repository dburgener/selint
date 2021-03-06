/*
* Copyright 2019 Tresys Technology, LLC
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include <stddef.h>
#include <stdlib.h>

#include "tree.h"
#include "maps.h"
#include "selint_error.h"

enum selint_error insert_policy_node_child(struct policy_node *parent,
                                           enum node_flavor flavor,
                                           union node_data data, unsigned int lineno)
{

	if (parent == NULL) {
		return SELINT_BAD_ARG;
	}

	struct policy_node *to_insert = malloc(sizeof(struct policy_node));
	if (!to_insert) {
		return SELINT_OUT_OF_MEM;
	}

	to_insert->parent = parent;
	to_insert->next = NULL;
	to_insert->first_child = NULL;
	to_insert->flavor = flavor;
	to_insert->data = data;
	to_insert->exceptions = NULL;
	to_insert->lineno = lineno;

	if (parent->first_child == NULL) {
		parent->first_child = to_insert;
		to_insert->prev = NULL;
	} else {

		struct policy_node *cur = parent->first_child;

		while (cur->next != NULL) {
			cur = cur->next;
		}

		cur->next = to_insert;
		to_insert->prev = cur;

	}

	return SELINT_SUCCESS;
}

enum selint_error insert_policy_node_next(struct policy_node *prev,
                                          enum node_flavor flavor,
                                          union node_data data, unsigned int lineno)
{

	if (prev == NULL) {
		return SELINT_BAD_ARG;
	}

	struct policy_node *to_insert = malloc(sizeof(struct policy_node));
	if (!to_insert) {
		return SELINT_OUT_OF_MEM;
	}

	prev->next = to_insert;

	to_insert->parent = prev->parent;
	to_insert->next = NULL;
	to_insert->first_child = NULL;
	to_insert->flavor = flavor;
	to_insert->data = data;
	to_insert->prev = prev;
	to_insert->exceptions = NULL;
	to_insert->lineno = lineno;

	return SELINT_SUCCESS;
}

int is_template_call(struct policy_node *node)
{
	if (node == NULL || node->data.ic_data == NULL) {
		return 0;
	}

	if (node->flavor != NODE_IF_CALL) {
		return 0;
	}

	char *call_name = node->data.ic_data->name;

	if (look_up_in_template_map(call_name)) {
		return 1;
	}
	return 0;
}

char *get_name_if_in_template(struct policy_node *cur)
{
	while (cur->parent) {
		cur = cur->parent;
		if (cur->flavor == NODE_TEMP_DEF) {
			return cur->data.str;
		}
	}
	return NULL;
}

struct string_list *get_types_in_node(const struct policy_node *node)
{

	struct string_list *ret = NULL;
	struct string_list *cur = NULL;
	struct av_rule_data *av_data;
	struct type_transition_data *tt_data;
	struct role_transition_data *rt_data;
	struct declaration_data *d_data;
	struct if_call_data *ifc_data;
	struct role_allow_data *ra_data;
	struct type_attribute_data *ta_data;

	switch (node->flavor) {
	case NODE_AV_RULE:
		av_data = node->data.av_data;
		cur = ret = copy_string_list(av_data->sources);
		if (cur) {
			while (cur->next) {
				cur = cur->next;
			}
			cur->next = copy_string_list(av_data->targets);
		} else {
			ret = copy_string_list(av_data->targets);
		}
		break;

	case NODE_TT_RULE:
		tt_data = node->data.tt_data;
		cur = ret = copy_string_list(tt_data->sources);
		if (cur) {
			while (cur->next) {
				cur = cur->next;
			}
			cur->next = copy_string_list(tt_data->targets);
		} else {
			cur = ret = copy_string_list(tt_data->targets);
		}
		if (cur) {
			while (cur->next) {
				cur = cur->next;
			}
			cur->next = calloc(1, sizeof(struct string_list));
			cur->next->string = strdup(tt_data->default_type);
		} else {
			cur = ret = calloc(1, sizeof(struct string_list));
			cur->string = strdup(tt_data->default_type);
		}
		break;

	case NODE_RT_RULE:
		rt_data = node->data.rt_data;
		ret = copy_string_list(rt_data->targets);
		break;

	case NODE_DECL:
		d_data = node->data.d_data;
		if (d_data->name) {
			ret = calloc(1, sizeof(struct string_list));
			ret->string = strdup(d_data->name);
			ret->next = copy_string_list(d_data->attrs);
		} else {
			ret = copy_string_list(d_data->attrs);
		}
		break;

	case NODE_IF_CALL:
		ifc_data = node->data.ic_data;
		ret = copy_string_list(ifc_data->args);
		break;

	case NODE_ROLE_ALLOW:
		ra_data = node->data.ra_data;
		ret = calloc(1, sizeof(struct string_list));
		ret->string = strdup(ra_data->from);
		ret->next = calloc(1, sizeof(struct string_list));
		ret->next->string = strdup(ra_data->to);
		break;
	case NODE_TYPE_ATTRIBUTE:
		ta_data = node->data.ta_data;
		ret = calloc(1, sizeof(struct string_list));
		ret->string = strdup(ta_data->type);
		ret->next = copy_string_list(ta_data->attrs);
		break;
	case NODE_ALIAS:
		ret = calloc(1, sizeof(struct string_list));
		ret->string = strdup(node->data.str);
		break;
	/*
	   NODE_M4_CALL,
	   NODE_OPTIONAL_POLICY,
	   NODE_OPTIONAL_ELSE,
	   NODE_M4_ARG,
	   NODE_START_BLOCK,
	   NODE_INTERFACE_DEF,
	   NODE_TEMP_DEF,
	   NODE_REQUIRE,
	   NODE_GEN_REQ,
	 */
	default:
		break;
	}

	// Check if any of the types are exclusions
	cur = ret;
	while (cur) {
		if (cur->string[0] == '-') {
			// memmove is safe for overlapping strings
			// Length is strlen exactly because it doesn't copy the first
			// character, but does copy the null terminator
			memmove(cur->string, cur->string + 1, strlen(cur->string));
		}
		cur = cur->next;
	}

	return ret;

}

struct string_list *get_types_required(const struct policy_node *node)
{
	struct string_list *ret = NULL;
	struct string_list *ret_cursor = NULL;

	struct policy_node *cur = node->first_child;

	while (cur) {
		if (ret_cursor) {
			ret_cursor->next = get_types_in_node(cur);
		} else {
			ret = ret_cursor = get_types_in_node(cur);
		}
		while (ret_cursor && ret_cursor->next) {
			ret_cursor = ret_cursor->next;
		}

		cur = cur->next;
	}

	return ret;
}

int is_in_require(const struct policy_node *cur)
{
	while (cur->parent) {
		cur = cur->parent;
		if (cur->flavor == NODE_GEN_REQ || cur->flavor == NODE_REQUIRE) {
			return 1;
		}
	}
	return 0;
}

struct policy_node *dfs_next(const struct policy_node *node)
{
	if (node->first_child) {
		return node->first_child;
	} else if (node->next) {
		return node->next;
	} else {
		while (node->parent && !node->parent->next) {
			node = node->parent;
		}
		if (node->parent) {
			return node->parent->next;
		} else {
			return NULL;
		}
	}
}

enum selint_error free_policy_node(struct policy_node *to_free)
{
	if (to_free == NULL) {
		return SELINT_BAD_ARG;
	}

	switch (to_free->flavor) {
	case NODE_AV_RULE:
		free_av_rule_data(to_free->data.av_data);
		break;
	case NODE_ROLE_ALLOW:
		free_ra_data(to_free->data.ra_data);
		break;
	case NODE_TT_RULE:
		free_type_transition_data(to_free->data.tt_data);
		break;
	case NODE_RT_RULE:
		free_role_transition_data(to_free->data.rt_data);
		break;
	case NODE_IF_CALL:
		free_if_call_data(to_free->data.ic_data);
		break;
	case NODE_DECL:
		free_declaration_data(to_free->data.d_data);
		break;
	case NODE_FC_ENTRY:
		free_fc_entry(to_free->data.fc_data);
		break;
	case NODE_TYPE_ATTRIBUTE:
		free_type_attribute_data(to_free->data.ta_data);
		break;
	default:
		if (to_free->data.str != NULL) {
			free(to_free->data.str);
		}
		break;
	}

	free(to_free->exceptions);
	to_free->exceptions = NULL;

	free_policy_node(to_free->first_child);
	to_free->first_child = NULL;

	free_policy_node(to_free->next);
	to_free->next = NULL;

	to_free->prev = NULL;

	free(to_free);

	return SELINT_SUCCESS;
}

enum selint_error free_av_rule_data(struct av_rule_data *to_free)
{

	if (to_free == NULL) {
		return SELINT_BAD_ARG;
	}

	free_string_list(to_free->sources);
	free_string_list(to_free->targets);
	free_string_list(to_free->object_classes);
	free_string_list(to_free->perms);

	to_free->sources = to_free->targets = to_free->object_classes =
		to_free->perms = NULL;

	free(to_free);

	return SELINT_SUCCESS;
}

enum selint_error free_ra_data(struct role_allow_data *to_free)
{

	if (to_free == NULL) {
		return SELINT_BAD_ARG;
	}

	free(to_free->from);
	free(to_free->to);
	free(to_free);

	return SELINT_SUCCESS;
}

enum selint_error free_type_transition_data(struct type_transition_data
                                            *to_free)
{

	if (to_free == NULL) {
		return SELINT_BAD_ARG;
	}

	free_string_list(to_free->sources);
	free_string_list(to_free->targets);
	free_string_list(to_free->object_classes);
	free(to_free->default_type);
	free(to_free->name);

	free(to_free);

	return SELINT_SUCCESS;
}

enum selint_error free_role_transition_data(struct role_transition_data
                                            *to_free)
{
	if (to_free == NULL) {
		return SELINT_BAD_ARG;
	}

	free_string_list(to_free->sources);
	free_string_list(to_free->targets);
	free(to_free->default_role);

	free(to_free);

	return SELINT_SUCCESS;
}

enum selint_error free_if_call_data(struct if_call_data *to_free)
{

	if (to_free == NULL) {
		return SELINT_BAD_ARG;
	}

	free(to_free->name);
	free_string_list(to_free->args);

	free(to_free);

	return SELINT_SUCCESS;
}

enum selint_error free_declaration_data(struct declaration_data *to_free)
{
	if (to_free == NULL) {
		return SELINT_BAD_ARG;
	}

	free_string_list(to_free->attrs);
	free(to_free->name);

	free(to_free);

	return SELINT_SUCCESS;
}

enum selint_error free_decl_list(struct decl_list *to_free)
{

	while (to_free) {
		free_declaration_data(to_free->decl);
		struct decl_list *tmp = to_free;
		to_free = to_free->next;
		free(tmp);
	}
	return SELINT_SUCCESS;
}

// The if call data structs in an if call list are pointers to data that is freed elsewhere
enum selint_error free_if_call_list(struct if_call_list *to_free)
{

	while (to_free) {
		struct if_call_list *tmp = to_free;
		to_free = to_free->next;
		free(tmp);
	}
	return SELINT_SUCCESS;
}

void free_fc_entry(struct fc_entry *to_free)
{
	if (to_free->path) {
		free(to_free->path);
	}
	if (to_free->context) {
		free_sel_context(to_free->context);
	}
	free(to_free);
}

void free_sel_context(struct sel_context *to_free)
{
	if (to_free->user) {
		free(to_free->user);
	}
	if (to_free->role) {
		free(to_free->role);
	}
	if (to_free->type) {
		free(to_free->type);
	}
	if (to_free->range) {
		free(to_free->range);
	}
	free(to_free);
}

void free_type_attribute_data(struct type_attribute_data *to_free)
{
	if (to_free->type) {
		free(to_free->type);
	}
	if (to_free->attrs) {
		free_string_list(to_free->attrs);
	}
	free(to_free);
}
