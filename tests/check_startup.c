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

#include <check.h>

#include "../src/startup.h"
#include "../src/maps.h"
#include "../src/selint_error.h"

#define MODULES_CONF_PATH SAMPLE_POL_DIR "modules.conf"
#define BAD_MODULES_CONF_PATH SAMPLE_POL_DIR "bad_modules.conf"
#define SAMPLE_AV_PATH SAMPLE_AV_DIR

START_TEST (test_load_access_vectors_normal) {

	load_access_vectors_normal(SAMPLE_AV_PATH);

	ck_assert_int_eq(decl_map_count(DECL_CLASS), 3);
	ck_assert_int_eq(decl_map_count(DECL_PERM), 37);

	ck_assert_str_eq(look_up_in_decl_map("file", DECL_CLASS), "class");
	ck_assert_str_eq(look_up_in_decl_map("append", DECL_PERM), "perm");
	ck_assert_str_eq(look_up_in_decl_map("listen", DECL_PERM), "perm");
	ck_assert_str_eq(look_up_in_decl_map("use", DECL_PERM), "perm");

	free_all_maps();

}
END_TEST

START_TEST (test_load_modules_source) {

	enum selint_error res = load_modules_source(MODULES_CONF_PATH);

	ck_assert_int_eq(SELINT_SUCCESS, res);

	ck_assert_str_eq("base", look_up_in_mods_map("sysadm"));
	ck_assert_str_eq("module", look_up_in_mods_map("sudo"));
	ck_assert_str_eq("off", look_up_in_mods_map("games"));

	res = load_modules_source(BAD_MODULES_CONF_PATH);

	ck_assert_int_eq(SELINT_PARSE_ERROR, res);

	free_all_maps();

}
END_TEST

Suite *startup_suite(void) {
	Suite *s;
	TCase *tc_core;

	s = suite_create("Startup");

	tc_core = tcase_create("Core");

	tcase_add_test(tc_core, test_load_access_vectors_normal);
	tcase_add_test(tc_core, test_load_modules_source);
	suite_add_tcase(s, tc_core);

	return s;
}

int main(void) {

	int number_failed = 0;
	Suite *s;
	SRunner *sr;

	s = startup_suite();
	sr = srunner_create(s);
	srunner_run_all(sr, CK_NORMAL);
	number_failed = srunner_ntests_failed(sr);
	srunner_free(sr);

	return (number_failed == 0)? 0 : -1;
}
