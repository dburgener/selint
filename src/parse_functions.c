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

#include <stdlib.h>
#include <string.h>

#include "parse_functions.h"
#include "selint_error.h"
#include "tree.h"
#include "template.h"
#include "ordering.h"

char *module_name = NULL;
char *parsing_filename = NULL;

enum selint_error begin_parsing_te(struct policy_node **cur, const char *mn,
                                   unsigned int lineno)
{
	(*cur)->data.str = strdup(mn);
	(*cur)->lineno = lineno;

	return SELINT_SUCCESS;
}

void set_current_module_name(const char *mn)
{
	if (module_name != NULL) {
		free(module_name);
	}
	module_name = strdup(mn);
}

char *get_current_module_name()
{
	return module_name;
}

enum selint_error insert_comment(struct policy_node **cur, unsigned int lineno)
{
	union node_data data;

	data.str = NULL;
	enum selint_error ret = insert_policy_node_next(*cur, NODE_COMMENT, data, lineno);
	if (ret != SELINT_SUCCESS) {
		return ret;
	}
	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_declaration(struct policy_node **cur,
                                     enum decl_flavor flavor, const char *name,
                                     struct string_list *attrs,
                                     unsigned int lineno)
{

	if (!is_in_require(*cur)) {
		// In a require block, the objects arent being declared
		// Otherwise, we need to insert them into the appropriate map

		char *temp_name = get_name_if_in_template(*cur);

		if (temp_name) {
			// We are inside a template, so we need to save declarations in the template map
			// TODO: What about nested templates?  This case may require some thought
			if (! (flavor == DECL_ROLE && attrs)) {
				// A role declaration with "attributes" in a template is:
				// role foo types bar_t, baz_t;
				// This is a role association, not a declaration, so we don't
				// insert declarations of this form
				insert_decl_into_template_map(temp_name, flavor, name);
			}
		} else if (name && '$' != name[0]) {
			// If the name starts with $ we're probably doing something like associating
			// a role with types in interfaces

			char *mn = get_current_module_name();

			if (!mn) {
				return SELINT_NO_MOD_NAME;
			}

			insert_into_decl_map(name, mn, flavor);

		}
	}
	if (flavor == DECL_ROLE &&
	    (*cur)->parent &&
	    (*cur)->parent->flavor == NODE_INTERFACE_DEF) {
		struct string_list *cur_sl_item = attrs;
		while (cur_sl_item) {
			if (cur_sl_item->string[0] == '$') {
				// Role interfaces are only those where the types are passed in, not the roles
				mark_role_if((*cur)->parent->data.str);
				break;
			}
			cur_sl_item = cur_sl_item->next;
		}
	}

	struct declaration_data *data = (struct declaration_data *)malloc(sizeof(struct declaration_data));
	if (!data) {
		return SELINT_OUT_OF_MEM;
	}

	memset(data, 0, sizeof(struct declaration_data));

	data->flavor = flavor;
	if (name) {
		data->name = strdup(name);
	} else {
		data->name = NULL;
	}
	data->attrs = attrs;

	union node_data nd;
	nd.d_data = data;

	enum selint_error ret =
		insert_policy_node_next(*cur, NODE_DECL, nd, lineno);

	if (ret != SELINT_SUCCESS) {
		free(data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_aliases(struct policy_node **cur,
                                 struct string_list *aliases,
                                 enum decl_flavor flavor, unsigned int lineno)
{

	struct string_list *alias = aliases;

	while (alias) {
		char *temp_name = get_name_if_in_template(*cur);
		if (temp_name) {
			insert_decl_into_template_map(temp_name, flavor,
			                              alias->string);
		} else {
			char *mn = get_current_module_name();
			if (!mn) {
				free_string_list(aliases);
				return SELINT_NO_MOD_NAME;
			}

			insert_into_decl_map(alias->string, mn, flavor);
		}
		union node_data nd;
		nd.str = strdup(alias->string);
		enum selint_error ret = insert_policy_node_child(*cur,
		                                                 NODE_ALIAS,
		                                                 nd,
		                                                 lineno);
		if (ret != SELINT_SUCCESS) {
			return ret;
		}
		alias = alias->next;
	}

	free_string_list(aliases);

	return SELINT_SUCCESS;
}

enum selint_error insert_type_alias(struct policy_node **cur, const char *type,
                                    unsigned int lineno)
{

	union node_data nd;

	nd.str = strdup(type);
	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_TYPE_ALIAS,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		return ret;
	}

	*cur = (*cur)->next;
	return SELINT_SUCCESS;
}

enum selint_error insert_av_rule(struct policy_node **cur,
                                 enum av_rule_flavor flavor,
                                 struct string_list *sources,
                                 struct string_list *targets,
                                 struct string_list *object_classes,
                                 struct string_list *perms, unsigned int lineno)
{

	struct av_rule_data *av_data = malloc(sizeof(struct av_rule_data));

	av_data->flavor = flavor;
	av_data->sources = sources;
	av_data->targets = targets;
	av_data->object_classes = object_classes;
	av_data->perms = perms;

	union node_data nd;
	nd.av_data = av_data;

	if ((*cur)->parent && (*cur)->parent->flavor == NODE_INTERFACE_DEF &&
	    check_transform_interface_suffix((*cur)->parent->data.str) &&
	    (str_in_sl("associate", perms) ||
	     str_in_sl("mounton", perms))) {
		mark_transform_if((*cur)->parent->data.str);
	}

	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_AV_RULE,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		free_av_rule_data(av_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_role_allow(struct policy_node **cur, const char *from_role,
                                    const char *to_role, unsigned int lineno)
{
	struct role_allow_data *ra_data = malloc(sizeof(struct role_allow_data));

	ra_data->from = strdup(from_role);
	ra_data->to = strdup(to_role);

	union node_data nd;
	nd.ra_data = ra_data;

	enum selint_error ret =
		insert_policy_node_next(*cur, NODE_ROLE_ALLOW, nd, lineno);
	if (ret != SELINT_SUCCESS) {
		free_ra_data(ra_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_type_transition(struct policy_node **cur,
                                         enum tt_flavor flavor,
                                         struct string_list *sources,
                                         struct string_list *targets,
                                         struct string_list *object_classes,
                                         const char *default_type, const char *name,
                                         unsigned int lineno)
{

	struct type_transition_data *tt_data =
		malloc(sizeof(struct type_transition_data));

	tt_data->sources = sources;
	tt_data->targets = targets;
	tt_data->object_classes = object_classes;
	tt_data->default_type = strdup(default_type);
	if (name) {
		tt_data->name = strdup(name);
	} else {
		tt_data->name = NULL;
	}
	tt_data->flavor = flavor;

	if (!str_in_sl("process", object_classes) &&
	    (*cur)->parent &&
	    (*cur)->parent->flavor == NODE_INTERFACE_DEF) {
		mark_filetrans_if((*cur)->parent->data.str);
	}

	union node_data nd;
	nd.tt_data = tt_data;

	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_TT_RULE,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		free_type_transition_data(tt_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_role_transition(struct policy_node **cur,
                                         struct string_list *sources,
                                         struct string_list *targets,
                                         char *default_role,
                                         unsigned int lineno)
{
	struct role_transition_data *rt_data =
	        malloc(sizeof(struct role_transition_data));

	rt_data->sources = sources;
	rt_data->targets = targets;
	rt_data->default_role = strdup(default_role);

	union node_data nd;
	nd.rt_data = rt_data;

	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_RT_RULE,
	                                                nd,
	                                                lineno);

	if (ret != SELINT_SUCCESS) {
		free_role_transition_data(rt_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}


enum selint_error insert_interface_call(struct policy_node **cur, const char *if_name,
                                        struct string_list *args,
                                        unsigned int lineno)
{
	struct if_call_data *if_data = malloc(sizeof(struct if_call_data));

	if_data->name = strdup(if_name);
	if_data->args = args;

	const char *template_name = get_name_if_in_template(*cur);

	if (template_name) {
		insert_call_into_template_map(template_name, if_data);
	} else {
		add_template_declarations(if_name, args, NULL, module_name);
	}

	if (0 == strcmp(if_name, "filetrans_pattern") &&
	    (*cur)->parent &&
	    (*cur)->parent->flavor == NODE_INTERFACE_DEF) {
		mark_filetrans_if((*cur)->parent->data.str);
	}

	union node_data nd;
	nd.ic_data = if_data;
	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_IF_CALL,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		free_if_call_data(if_data);
		return ret;
	}

	*cur = (*cur)->next;

	return SELINT_SUCCESS;
}

enum selint_error insert_permissive_statement(struct policy_node **cur,
                                              const char *domain, unsigned int lineno)
{
	union node_data nd;

	nd.str = strdup(domain);
	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_PERMISSIVE,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		return ret;
	}

	*cur = (*cur)->next;
	return SELINT_SUCCESS;
}

enum selint_error insert_semicolon(struct policy_node **cur, unsigned int lineno)
{

	union node_data nd;
	nd.str = NULL;

	enum selint_error ret = insert_policy_node_next(*cur,
	                                                NODE_SEMICOLON,
	                                                nd,
	                                                lineno);
	if (ret != SELINT_SUCCESS) {
		return ret;
	}

	*cur = (*cur)->next;
	return SELINT_SUCCESS;
}

static enum selint_error begin_block(struct policy_node **cur,
                              enum node_flavor block_type, char *data,
                              unsigned int lineno)
{
	union node_data nd;

	nd.str = data;
	enum selint_error ret = insert_policy_node_next(*cur,
	                                                block_type,
	                                                nd,
	                                                lineno);

	if (ret != SELINT_SUCCESS) {
		return ret;
	}

	*cur = (*cur)->next;

	nd.str = NULL;
	ret = insert_policy_node_child(*cur, NODE_START_BLOCK, nd, lineno);
	if (ret != SELINT_SUCCESS) {
		*cur = (*cur)->prev;
		free_policy_node((*cur)->next);
		return ret;
	}

	*cur = (*cur)->first_child;

	return SELINT_SUCCESS;
}

static enum selint_error end_block(struct policy_node **cur,
                            enum node_flavor block_type)
{

	if ((*cur)->parent == NULL || (*cur)->parent->flavor != block_type) {
		return SELINT_NOT_IN_BLOCK;
	}

	*cur = (*cur)->parent;
	return SELINT_SUCCESS;
}

enum selint_error begin_optional_policy(struct policy_node **cur,
                                        unsigned int lineno)
{

	return begin_block(cur, NODE_OPTIONAL_POLICY, (char *)NULL, lineno);
}

enum selint_error end_optional_policy(struct policy_node **cur)
{

	return end_block(cur, NODE_OPTIONAL_POLICY);
}

enum selint_error begin_optional_else(struct policy_node **cur,
                                      unsigned int lineno)
{
	return begin_block(cur, NODE_OPTIONAL_ELSE, (char *)NULL, lineno);
}

enum selint_error end_optional_else(struct policy_node **cur)
{
	return end_block(cur, NODE_OPTIONAL_ELSE);
}

enum selint_error begin_tunable_policy(struct policy_node **cur,
                                       unsigned int lineno)
{

	return begin_block(cur, NODE_TUNABLE_POLICY, (char *)NULL, lineno);
}

enum selint_error end_tunable_policy(struct policy_node **cur)
{

	return end_block(cur, NODE_TUNABLE_POLICY);
}

enum selint_error begin_interface_def(struct policy_node **cur,
                                      enum node_flavor flavor, const char *name,
                                      unsigned int lineno)
{

	switch (flavor) {
	case NODE_INTERFACE_DEF:
		break;
	case NODE_TEMP_DEF:
		insert_template_into_template_map(name);
		break;
	default:
		return SELINT_BAD_ARG;
	}

	insert_into_ifs_map(name, get_current_module_name());

	return begin_block(cur, flavor, strdup(name), lineno);
}

enum selint_error end_interface_def(struct policy_node **cur)
{

	if (end_block(cur, NODE_INTERFACE_DEF) == SELINT_NOT_IN_BLOCK) {
		return end_block(cur, NODE_TEMP_DEF);
	} else {
		return SELINT_SUCCESS;
	}
}

enum selint_error begin_gen_require(struct policy_node **cur,
                                    unsigned int lineno)
{

	return begin_block(cur, NODE_GEN_REQ, (char *)NULL, lineno);
}

enum selint_error end_gen_require(struct policy_node **cur)
{

	return end_block(cur, NODE_GEN_REQ);
}

enum selint_error begin_require(struct policy_node **cur, unsigned int lineno)
{

	return begin_block(cur, NODE_REQUIRE, (char *)NULL, lineno);
}

enum selint_error end_require(struct policy_node **cur)
{

	return end_block(cur, NODE_REQUIRE);
}

enum selint_error begin_ifdef(struct policy_node **cur, unsigned int lineno)
{
	return begin_block(cur, NODE_IFDEF, (char *)NULL, lineno);
}

enum selint_error end_ifdef(struct policy_node **cur)
{
	return end_block(cur, NODE_IFDEF);
}

enum selint_error save_command(struct policy_node *cur, const char *comm)
{
	if (comm == NULL || cur == NULL) {
		return SELINT_BAD_ARG;
	}
	while (*comm != 's' && *comm != '\0') {
		comm++;
	}
	if (0 != strncmp("selint-", comm, 7)) {
		return SELINT_PARSE_ERROR;
	}
	comm += strlen("selint-");
	if (0 == strncmp("disable:", comm, 8)) {
		cur->exceptions = strdup(comm + strlen("disable:"));
	} else {
		return SELINT_PARSE_ERROR;
	}

	return SELINT_SUCCESS;
}

enum selint_error insert_type_attribute(struct policy_node **cur, const char *type, struct string_list *attrs, unsigned int lineno)
{
	struct type_attribute_data *data = calloc(1, sizeof(struct type_attribute_data));
	if (!data) {
		return SELINT_OUT_OF_MEM;
	}
	union node_data nd;
	nd.ta_data = data;

	data->type = strdup(type);
	data->attrs = attrs;

	enum selint_error ret = insert_policy_node_next(*cur, NODE_TYPE_ATTRIBUTE, nd, lineno);
	if (ret != SELINT_SUCCESS) {
		free(data);
		return ret;
	}

	*cur = (*cur)->next;

	if ((*cur)->parent &&
	    (*cur)->parent->flavor == NODE_INTERFACE_DEF &&
	    check_transform_interface_suffix((*cur)->parent->data.str)) {
		mark_transform_if((*cur)->parent->data.str);
	}

	return SELINT_SUCCESS;
}

void cleanup_parsing()
{
	if (module_name) {
		free(module_name);
		module_name = NULL;
	}

	free_all_maps();
}
