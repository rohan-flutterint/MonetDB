
/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2020 MonetDB B.V.
 */

#include "monetdb_config.h"
#include "rel_statistics.h"
#include "rel_optimizer.h"
#include "rel_rel.h"
#include "rel_exp.h"
#include "rel_prop.h"
#include "rel_rewriter.h"
#include "sql_mvc.h"
#include "mtime.h"

sql_hash *sql_functions_lookup = NULL;

static void
sql_add_propagate_statistics(mvc *sql, sql_exp *e)
{
	list *l = e->l;
	sql_exp *first = l->h->data, *second = l->h->next->data;
	atom *lmax, *rmax, *lmin, *rmin, *res1 = NULL, *res2 = NULL;
	str msg1 = NULL, msg2 = NULL;

	if ((lmax = find_prop_and_get(first->p, PROP_MAX)) && (rmax = find_prop_and_get(second->p, PROP_MAX)) &&
		(lmin = find_prop_and_get(first->p, PROP_MIN)) && (rmin = find_prop_and_get(second->p, PROP_MIN))) {
		sql_subfunc *f = (sql_subfunc *)e->f;

		if (strcmp(f->func->mod, "calc") == 0) {
			res1 = atom_add(atom_dup(sql->sa, lmax), rmax);
			res2 = atom_add(atom_dup(sql->sa, lmin), rmin);
		} else {
			sql_subtype tp;

			assert(strcmp(f->func->mod, "mtime") == 0);
			if (strcmp(f->func->imp, "date_add_msec_interval") == 0) {
				date sub1, sub2;

				if (!(msg1 = date_add_msec_interval(&sub1, (date)lmax->data.val.ival, rmax->data.val.lval)) &&
					!(msg2 = date_add_msec_interval(&sub2, (date)lmin->data.val.ival, rmin->data.val.lval))) {
					sql_find_subtype(&tp, "date", 0, 0);
					res1 = atom_general_ptr(sql->sa, &tp, &sub1);
					res2 = atom_general_ptr(sql->sa, &tp, &sub2);
				}
			} else if (strcmp(f->func->imp, "addmonths") == 0) {
				date sub1, sub2;

				if (!(msg1 = date_addmonths(&sub1, (date)lmax->data.val.ival, rmax->data.val.ival)) &&
					!(msg2 = date_addmonths(&sub2, (date)lmin->data.val.ival, rmin->data.val.ival))) {
					sql_find_subtype(&tp, "date", 0, 0);
					res1 = atom_general_ptr(sql->sa, &tp, &sub1);
					res2 = atom_general_ptr(sql->sa, &tp, &sub2);
				}
			} else if (strcmp(f->func->imp, "time_add_msec_interval") == 0) {
				daytime sub1 = time_add_msec_interval((daytime)lmax->data.val.lval, rmax->data.val.lval),
						sub2 = time_add_msec_interval((daytime)lmin->data.val.lval, rmin->data.val.lval);

				sql_find_subtype(&tp, "time", 0, 0);
				res1 = atom_general_ptr(sql->sa, &tp, &sub1);
				res2 = atom_general_ptr(sql->sa, &tp, &sub2);
			} else if (strcmp(f->func->imp, "timestamp_add_msec_interval") == 0) {
				timestamp sub1, sub2;

				if (!(msg1 = timestamp_add_msec_interval(&sub1, (timestamp)lmax->data.val.lval, rmax->data.val.lval)) &&
					!(msg2 = timestamp_add_msec_interval(&sub2, (timestamp)lmin->data.val.lval, rmin->data.val.lval))) {
					sql_find_subtype(&tp, "timestamp", 0, 0);
					res1 = atom_general_ptr(sql->sa, &tp, &sub1);
					res2 = atom_general_ptr(sql->sa, &tp, &sub2);
				}
			} else if (strcmp(f->func->imp, "timestamp_add_month_interval") == 0) {
				timestamp sub1, sub2;

				if (!(msg1 = timestamp_add_month_interval(&sub1, (timestamp)lmax->data.val.lval, rmax->data.val.ival)) &&
					!(msg2 = timestamp_add_month_interval(&sub2, (timestamp)lmin->data.val.lval, rmin->data.val.ival))) {
					sql_find_subtype(&tp, "timestamp", 0, 0);
					res1 = atom_general_ptr(sql->sa, &tp, &sub1);
					res2 = atom_general_ptr(sql->sa, &tp, &sub2);
				}
			}
		}

		if (res1 && res2) { /* if the min/max pair overflows, then don't propagate */
			set_property(sql, e, PROP_MAX, res1);
			set_property(sql, e, PROP_MIN, res2);
		}
		freeException(msg1);
		freeException(msg2);
	}
}

static void
sql_sub_propagate_statistics(mvc *sql, sql_exp *e)
{
	list *l = e->l;
	sql_exp *first = l->h->data, *second = l->h->next->data;
	atom *lmax, *rmax, *lmin, *rmin, *res1 = NULL, *res2 = NULL;

	if ((lmax = find_prop_and_get(first->p, PROP_MAX)) && (rmax = find_prop_and_get(second->p, PROP_MAX)) &&
		(lmin = find_prop_and_get(first->p, PROP_MIN)) && (rmin = find_prop_and_get(second->p, PROP_MIN))) {
		sql_subfunc *f = (sql_subfunc *)e->f;
		str msg1 = NULL, msg2 = NULL;

		if (strcmp(f->func->mod, "calc") == 0) {
			res1 = atom_sub(atom_dup(sql->sa, lmax), rmax);
			res2 = atom_sub(atom_dup(sql->sa, lmin), rmin);
		} else {
			sql_subtype tp;

			assert(strcmp(f->func->mod, "mtime") == 0);
			if (strcmp(f->func->imp, "diff") == 0) {
				sql_subtype *t1 = exp_subtype(first);

				switch (t1->type->eclass) {
				case EC_DATE: {
					res1 = atom_int(sql->sa, sql_bind_localtype("int"), date_diff_imp((date)lmax->data.val.ival, (date)rmax->data.val.ival));
					res2 = atom_int(sql->sa, sql_bind_localtype("int"), date_diff_imp((date)lmin->data.val.ival, (date)rmin->data.val.ival));
				} break;
				case EC_TIME: {
					res1 = atom_int(sql->sa, sql_bind_localtype("lng"), daytime_diff((daytime)lmax->data.val.lval, (daytime)rmax->data.val.lval));
					res2 = atom_int(sql->sa, sql_bind_localtype("lng"), daytime_diff((daytime)lmin->data.val.lval, (daytime)rmin->data.val.lval));
				} break;
				case EC_TIMESTAMP: {
					res1 = atom_int(sql->sa, sql_bind_localtype("lng"), tsdiff((timestamp)lmax->data.val.lval, (timestamp)rmax->data.val.lval));
					res2 = atom_int(sql->sa, sql_bind_localtype("lng"), tsdiff((timestamp)lmin->data.val.lval, (timestamp)rmin->data.val.lval));
				} break;
				default:
					break;
				}
			} else if (strcmp(f->func->imp, "date_sub_msec_interval") == 0) {
				date sub1, sub2;

				if (!(msg1 = date_sub_msec_interval(&sub1, (date)lmax->data.val.ival, rmax->data.val.lval)) &&
					!(msg2 = date_sub_msec_interval(&sub2, (date)lmin->data.val.ival, rmin->data.val.lval))) {
					sql_find_subtype(&tp, "date", 0, 0);
					res1 = atom_general_ptr(sql->sa, &tp, &sub1);
					res2 = atom_general_ptr(sql->sa, &tp, &sub2);
				}
			} else if (strcmp(f->func->imp, "date_sub_month_interval") == 0) {
				date sub1, sub2;

				if (!(msg1 = date_submonths(&sub1, (date)lmax->data.val.ival, rmax->data.val.ival)) &&
					!(msg2 = date_submonths(&sub2, (date)lmin->data.val.ival, rmin->data.val.ival))) {
					sql_find_subtype(&tp, "date", 0, 0);
					res1 = atom_general_ptr(sql->sa, &tp, &sub1);
					res2 = atom_general_ptr(sql->sa, &tp, &sub2);
				}
			} else if (strcmp(f->func->imp, "time_sub_msec_interval") == 0) {
				daytime sub1 = time_sub_msec_interval((daytime)lmax->data.val.lval, rmax->data.val.lval),
						sub2 = time_sub_msec_interval((daytime)lmin->data.val.lval, rmin->data.val.lval);

				sql_find_subtype(&tp, "time", 0, 0);
				res1 = atom_general_ptr(sql->sa, &tp, &sub1);
				res2 = atom_general_ptr(sql->sa, &tp, &sub2);
			} else if (strcmp(f->func->imp, "timestamp_sub_msec_interval") == 0) {
				timestamp sub1, sub2;

				if (!(msg1 = timestamp_sub_msec_interval(&sub1, (timestamp)lmax->data.val.lval, rmax->data.val.lval)) &&
					!(msg2 = timestamp_sub_msec_interval(&sub2, (timestamp)lmin->data.val.lval, rmin->data.val.lval))) {
					sql_find_subtype(&tp, "timestamp", 0, 0);
					res1 = atom_general_ptr(sql->sa, &tp, &sub1);
					res2 = atom_general_ptr(sql->sa, &tp, &sub2);
				}
			} else if (strcmp(f->func->imp, "timestamp_sub_month_interval") == 0) {
				timestamp sub1, sub2;

				if (!(msg1 = timestamp_sub_month_interval(&sub1, (timestamp)lmax->data.val.lval, rmax->data.val.ival)) &&
					!(msg2 = timestamp_sub_month_interval(&sub2, (timestamp)lmin->data.val.lval, rmin->data.val.ival))) {
					sql_find_subtype(&tp, "timestamp", 0, 0);
					res1 = atom_general_ptr(sql->sa, &tp, &sub1);
					res2 = atom_general_ptr(sql->sa, &tp, &sub2);
				}
			}
		}

		if (res1 && res2) { /* if the min/max pair overflows, then don't propagate */
			atom *zero1 = atom_zero_value(sql->sa, &(lmax->tpe)), *zero2 = atom_zero_value(sql->sa, &(rmax->tpe));
			int cmp1 = atom_cmp(lmax, zero1), cmp2 = atom_cmp(lmin, zero1), cmp3 = atom_cmp(rmin, zero2), cmp4 = atom_cmp(rmax, zero2);

			if (cmp1 >= 0 && cmp2 >= 0 && cmp3 >= 0 && cmp4 >= 0) { /* if all positive then propagate */
				set_property(sql, e, PROP_MAX, res1);
				set_property(sql, e, PROP_MIN, res2);
			} else if (cmp1 < 0 && cmp2 < 0 && cmp3 < 0 && cmp4 < 0) { /* if all negative propagate by swapping min and max */
				set_property(sql, e, PROP_MAX, res2);
				set_property(sql, e, PROP_MIN, res1);
			}
		}
		freeException(msg1);
		freeException(msg2);
	}
}

static void
sql_mul_propagate_statistics(mvc *sql, sql_exp *e)
{
	list *l = e->l;
	sql_exp *first = l->h->data, *second = l->h->next->data;
	atom *lmax, *rmax, *lmin, *rmin;

	if ((lmax = find_prop_and_get(first->p, PROP_MAX)) && (rmax = find_prop_and_get(second->p, PROP_MAX)) &&
		(lmin = find_prop_and_get(first->p, PROP_MIN)) && (rmin = find_prop_and_get(second->p, PROP_MIN))) {
		atom *res1 = atom_mul(atom_dup(sql->sa, lmax), rmax);
		atom *res2 = atom_mul(atom_dup(sql->sa, lmin), rmin);

		if (res1 && res2) { /* if the min/max pair overflows, then don't propagate */
			atom *zero1 = atom_zero_value(sql->sa, &(lmax->tpe)), *zero2 = atom_zero_value(sql->sa, &(rmax->tpe));
			int cmp1 = atom_cmp(lmax, zero1), cmp2 = atom_cmp(lmin, zero1), cmp3 = atom_cmp(rmin, zero2), cmp4 = atom_cmp(rmax, zero2);

			if (cmp1 >= 0 && cmp2 >= 0 && cmp3 >= 0 && cmp4 >= 0) { /* if all positive then propagate */
				set_property(sql, e, PROP_MAX, res1);
				set_property(sql, e, PROP_MIN, res2);
			} else if (cmp1 < 0 && cmp2 < 0 && cmp3 < 0 && cmp4 < 0) { /* if all negative propagate by swapping min and max */
				set_property(sql, e, PROP_MAX, res2);
				set_property(sql, e, PROP_MIN, res1);
			}
		}
	}
}

static void
sql_div_propagate_statistics(mvc *sql, sql_exp *e)
{
	list *l = e->l;
	sql_exp *first = l->h->data, *second = l->h->next->data;
	atom *lmax, *rmax, *lmin, *rmin;

	if ((lmax = find_prop_and_get(first->p, PROP_MAX)) && (rmax = find_prop_and_get(second->p, PROP_MAX)) &&
		(lmin = find_prop_and_get(first->p, PROP_MIN)) && (rmin = find_prop_and_get(second->p, PROP_MIN))) {
		atom *res1 = atom_div(atom_dup(sql->sa, lmax), rmin);
		atom *res2 = atom_div(atom_dup(sql->sa, lmin), rmax);

		if (res1 && res2) { /* on div by zero don't propagate */
			atom *zero1 = atom_zero_value(sql->sa, &(lmax->tpe)), *zero2 = atom_zero_value(sql->sa, &(rmax->tpe));
			int cmp1 = atom_cmp(lmax, zero1), cmp2 = atom_cmp(lmin, zero1), cmp3 = atom_cmp(rmin, zero2), cmp4 = atom_cmp(rmax, zero2);

			if (cmp1 >= 0 && cmp2 >= 0 && cmp3 >= 0 && cmp4 >= 0) { /* if all positive then propagate */
				set_property(sql, e, PROP_MAX, res1);
				set_property(sql, e, PROP_MIN, res2);
			} else if (cmp1 < 0 && cmp2 < 0 && cmp3 < 0 && cmp4 < 0) { /* if all negative propagate by swapping min and max */
				set_property(sql, e, PROP_MAX, res2);
				set_property(sql, e, PROP_MIN, res1);
			}
		}
	}
}

static void
sql_extend_min_max(mvc *sql, sql_exp *e, sql_exp *first, sql_exp *second)
{
	atom *lval, *rval;

	if ((lval = find_prop_and_get(first->p, PROP_MAX)) && (rval = find_prop_and_get(second->p, PROP_MAX)))
		set_property(sql, e, PROP_MAX, atom_cmp(lval, rval) > 0 ? lval : rval);
	if ((lval = find_prop_and_get(first->p, PROP_MIN)) && (rval = find_prop_and_get(second->p, PROP_MIN)))
		set_property(sql, e, PROP_MIN, atom_cmp(lval, rval) > 0 ? rval : lval);
}

static void
sql_least_greatest_propagate_statistics(mvc *sql, sql_exp *e)
{
	list *l = e->l;
	sql_extend_min_max(sql, e, l->h->data, l->h->next->data);
}

static void
sql_ifthenelse_propagate_statistics(mvc *sql, sql_exp *e)
{
	list *l = e->l;
	sql_extend_min_max(sql, e, l->h->next->data, l->h->next->next->data);
}

static void
sql_nullif_propagate_statistics(mvc *sql, sql_exp *e)
{
	list *l = e->l;
	sql_exp *first = l->h->data;
	atom *lval;

	if ((lval = find_prop_and_get(first->p, PROP_MAX)))
		set_property(sql, e, PROP_MAX, lval);
	if ((lval = find_prop_and_get(first->p, PROP_MIN)))
		set_property(sql, e, PROP_MIN, lval);
}

static void
sql_neg_propagate_statistics(mvc *sql, sql_exp *e)
{
	list *l = e->l;
	sql_exp *first = l->h->data;
	atom *lval;

	if ((lval = find_prop_and_get(first->p, PROP_MIN))) {
		atom *res = atom_dup(sql->sa, lval);
		if (!atom_neg(res))
			set_property(sql, e, PROP_MAX, res);
	}
	if ((lval = find_prop_and_get(first->p, PROP_MAX))) {
		atom *res = atom_dup(sql->sa, lval);
		if (!atom_neg(res))
			set_property(sql, e, PROP_MIN, res);
	}
}

static void
sql_sign_propagate_statistics(mvc *sql, sql_exp *e)
{
	list *l = e->l;
	sql_exp *first = l->h->data;
	atom *omin, *omax;
	sql_subtype *bte = sql_bind_localtype("bte");
	bool properties_set = false;

	if ((omin = find_prop_and_get(first->p, PROP_MIN)) && (omax = find_prop_and_get(first->p, PROP_MAX))) {
		atom *zero1 = atom_zero_value(sql->sa, &(omin->tpe));
		int cmp1 = atom_cmp(omax, zero1), cmp2 = atom_cmp(omin, zero1);

		if (cmp1 >= 0 && cmp2 >= 0) {
			set_property(sql, e, PROP_MAX, atom_int(sql->sa, bte, 1));
			set_property(sql, e, PROP_MIN, atom_int(sql->sa, bte, 1));
			properties_set = true;
		} else if (cmp1 < 0 && cmp2 < 0) {
			set_property(sql, e, PROP_MAX, atom_int(sql->sa, bte, -1));
			set_property(sql, e, PROP_MIN, atom_int(sql->sa, bte, -1));
			properties_set = true;
		}
	}
	if (!properties_set) {
		set_property(sql, e, PROP_MAX, atom_int(sql->sa, bte, 1));
		set_property(sql, e, PROP_MIN, atom_int(sql->sa, bte, -1));
	}
}

static void
sql_abs_propagate_statistics(mvc *sql, sql_exp *e)
{
	list *l = e->l;
	sql_exp *first = l->h->data;
	atom *omin, *omax;

	if ((omin = find_prop_and_get(first->p, PROP_MIN)) && (omax = find_prop_and_get(first->p, PROP_MAX))) {
		atom *zero = atom_zero_value(sql->sa, &(omin->tpe));
		int cmp1 = atom_cmp(omax, zero), cmp2 = atom_cmp(omin, zero);

		if (cmp1 >= 0 && cmp2 >= 0) {
			set_property(sql, e, PROP_MAX, omax);
			set_property(sql, e, PROP_MIN, omin);
		} else if (cmp1 < 0 && cmp2 < 0) {
			atom *res1 = atom_dup(sql->sa, omin), *res2 = atom_dup(sql->sa, omax);

			if (!atom_absolute(res1) && !atom_absolute(res2)) {
				set_property(sql, e, PROP_MAX, res1);
				set_property(sql, e, PROP_MIN, res2);
			}
		} else {
			atom *res1 = atom_dup(sql->sa, omin);

			if (!atom_absolute(res1)) {
				set_property(sql, e, PROP_MAX, atom_cmp(res1, omax) > 0 ? res1 : omax);
				set_property(sql, e, PROP_MIN, zero);
			}
		}
	}
}

static void
sql_coalesce_propagate_statistics(mvc *sql, sql_exp *e)
{
	list *l = e->l;
	sql_exp *first = l->h->data;
	atom *curmin = NULL, *curmax = NULL, *lval;

	if ((lval = find_prop_and_get(first->p, PROP_MAX)))
		curmax = lval;
	if ((lval = find_prop_and_get(first->p, PROP_MIN)))
		curmin = lval;
	for (node *n = l->h->next ; n && curmin && curmax ; n = n->next) {
		sql_exp *next = n->data;

		if ((lval = find_prop_and_get(next->p, PROP_MAX))) {
			curmax = atom_cmp(lval, curmax) > 0 ? lval : curmax;
		} else {
			curmax = NULL;
		}
		if ((lval = find_prop_and_get(next->p, PROP_MIN))) {
			curmin = atom_cmp(lval, curmin) > 0 ? curmin : lval;
		} else {
			curmin = NULL;
		}
	}

	if (curmin && curmax) {
		set_property(sql, e, PROP_MAX, curmax);
		set_property(sql, e, PROP_MIN, curmin);
	}
}

static struct function_properties functions_list[14] = {
	{"sql_add", &sql_add_propagate_statistics},
	{"sql_sub", &sql_sub_propagate_statistics},
	{"sql_mul", &sql_mul_propagate_statistics},
	{"sql_div", &sql_div_propagate_statistics},
	{"sql_neg", &sql_neg_propagate_statistics},
	{"sign", &sql_sign_propagate_statistics},
	{"abs", &sql_abs_propagate_statistics},
	{"sql_min", &sql_least_greatest_propagate_statistics},
	{"sql_max", &sql_least_greatest_propagate_statistics},
	{"least", &sql_least_greatest_propagate_statistics},
	{"greatest", &sql_least_greatest_propagate_statistics},
	{"ifthenelse", &sql_ifthenelse_propagate_statistics},
	{"nullif", &sql_nullif_propagate_statistics},
	{"coalesce", &sql_coalesce_propagate_statistics}
};

void
initialize_sql_functions_lookup(sql_allocator *sa)
{
	int nentries = sizeof(functions_list) / sizeof(functions_list[0]);

	sql_functions_lookup = hash_new(sa, nentries, (fkeyvalue)&hash_key);
	for (int i = 0; i < nentries ; i++) {
		int key = hash_key(functions_list[i].name);

		hash_add(sql_functions_lookup, key, &(functions_list[i]));
	}
}
