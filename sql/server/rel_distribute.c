/*
 * SPDX-License-Identifier: MPL-2.0
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2023 MonetDB B.V.
 */

#include "monetdb_config.h"
#include "rel_optimizer_private.h"
#include "rel_basetable.h"
#include "rel_exp.h"
#include "rel_remote.h"
#include "sql_privileges.h"

static int
has_remote_or_replica( sql_rel *rel )
{
	if (!rel)
		return 0;

	switch (rel->op) {
	case op_basetable: {
		sql_table *t = rel->l;

		return t && (isReplicaTable(t) || isRemote(t));
	}
	case op_table:
		if (IS_TABLE_PROD_FUNC(rel->flag) || rel->flag == TABLE_FROM_RELATION)
			return has_remote_or_replica( rel->l );
		break;
	case op_join:
	case op_left:
	case op_right:
	case op_full:

	case op_semi:
	case op_anti:

	case op_union:
	case op_inter:
	case op_except:
	case op_merge:

	case op_insert:
	case op_update:
	case op_delete:
		return has_remote_or_replica( rel->l ) || has_remote_or_replica( rel->r );
	case op_project:
	case op_select:
	case op_groupby:
	case op_topn:
	case op_sample:
	case op_truncate:
		return has_remote_or_replica( rel->l );
	case op_ddl:
		if (rel->flag == ddl_output || rel->flag == ddl_create_seq || rel->flag == ddl_alter_seq /*|| rel->flag == ddl_alter_table || rel->flag == ddl_create_table || rel->flag == ddl_create_view*/)
			return has_remote_or_replica( rel->l );
		if (rel->flag == ddl_list || rel->flag == ddl_exception)
			return has_remote_or_replica( rel->l ) || has_remote_or_replica( rel->r );
		break;
	}
	return 0;
}

static sql_rel *
rewrite_replica(mvc *sql, list *exps, sql_table *t, sql_table *p, int remote_prop)
{
	node *n, *m;
	sql_rel *r = rel_basetable(sql, p, t->base.name);

	for (n = exps->h; n; n = n->next) {
		sql_exp *e = n->data;
		const char *nname = exp_name(e);

		node *nn = ol_find_name(t->columns, nname);
		if (nn) {
			sql_column *c = nn->data;
			rel_base_use(sql, r, c->colnr);
		} else if (strcmp(nname, TID) == 0) {
			rel_base_use_tid(sql, r);
		} else {
			assert(0);
		}
	}
	r = rewrite_basetable(sql, r);
	for (n = exps->h, m = r->exps->h; n && m; n = n->next, m = m->next) {
		sql_exp *e = n->data;
		sql_exp *ne = m->data;

		exp_prop_alias(sql->sa, ne, e);
	}
	list_hash_clear(r->exps); /* the child table may have different column names, so clear the hash */

	/* set_remote() */
	if (remote_prop && p && isRemote(p)) {
		sqlid id = p->base.id;
		char *local_name = sa_strconcat(sql->sa, sa_strconcat(sql->sa, p->s->base.name, "."), p->base.name);
		prop *p = r->p = prop_create(sql->sa, PROP_REMOTE, r->p);
		p->id = id;
		p->value.pval = local_name;
	}
	return r;
}

static sql_rel *
replica_rewrite(visitor *v, sql_table *t, list *exps)
{
	sql_rel *res = NULL;
	const char *uri = (const char *) v->data;

	if (mvc_highwater(v->sql))
		return sql_error(v->sql, 10, SQLSTATE(42000) "Query too complex: running out of stack space");

	if (uri) {
		/* replace by the replica which matches the uri */
		for (node *n = t->members->h; n; n = n->next) {
			sql_part *p = n->data;
			sql_table *pt = find_sql_table_id(v->sql->session->tr, t->s, p->member);

			if (isRemote(pt) && strcmp(uri, pt->query) == 0) {
				res = rewrite_replica(v->sql, exps, t, pt, 0);
				break;
			}
		}
	}
	if (!res) { /* no match, find one without remote or use first */
		sql_table *pt = NULL;
		int remote = 1;

		for (node *n = t->members->h; n; n = n->next) {
			sql_part *p = n->data;
			sql_table *next = find_sql_table_id(v->sql->session->tr, t->s, p->member);

			/* give preference to local tables and avoid empty merge or replica tables */
			if (!isRemote(next) && ((!isReplicaTable(next) && !isMergeTable(next)) || !list_empty(next->members))) {
				pt = next;
				remote = 0;
				break;
			}
		}
		if (!pt) {
			sql_part *p = t->members->h->data;
			pt = find_sql_table_id(v->sql->session->tr, t->s, p->member);
		}

		if ((isMergeTable(pt) || isReplicaTable(pt)) && list_empty(pt->members))
			return sql_error(v->sql, 02, SQLSTATE(42000) "%s '%s'.'%s' should have at least one table associated",
							TABLE_TYPE_DESCRIPTION(pt->type, pt->properties), pt->s->base.name, pt->base.name);
		res = isReplicaTable(pt) ? replica_rewrite(v, pt, exps) : rewrite_replica(v->sql, exps, t, pt, remote);
	}
	return res;
}

static bool
eliminate_remote_or_replica_refs(visitor *v, sql_rel **rel)
{
	if (rel_is_ref(*rel) && !((*rel)->flag&MERGE_LEFT)) {
 		if (has_remote_or_replica(*rel)) {
 			sql_rel *nrel = rel_copy(v->sql, *rel, 1);
 			rel_destroy(*rel);
 			*rel = nrel;
 			return true;
 		} else {
 			// TODO why do we want to bail out if we have a non rmt/rpl ref?
 			return false;
 		}
 	}
 	return true;
}

static sql_rel *
rel_rewrite_replica_(visitor *v, sql_rel *rel)
{
	if (!eliminate_remote_or_replica_refs(v, &rel))
		return rel;

	if (is_basetable(rel->op)) {
		sql_table *t = rel->l;

		if (t && isReplicaTable(t)) {
			if (list_empty(t->members)) /* in DDL statement cases skip if replica is empty */
				return rel;

			sql_rel *r = replica_rewrite(v, t, rel->exps);
			rel_destroy(rel);
			rel = r;
		}
	}
	return rel;
}

static sql_rel *
rel_rewrite_replica(visitor *v, global_props *gp, sql_rel *rel)
{
	(void) gp;
	return rel_visitor_bottomup(v, rel, &rel_rewrite_replica_);
}

run_optimizer
bind_rewrite_replica(visitor *v, global_props *gp)
{
	(void) v;
	return gp->needs_mergetable_rewrite || gp->needs_remote_replica_rewrite ? rel_rewrite_replica : NULL;
}

static sql_rel *
rel_rewrite_remote_2_(visitor *v, sql_rel *rel)
{
	prop *p, *pl, *pr;

	if (!eliminate_remote_or_replica_refs(v, &rel))
		return rel;

	sql_rel *l = rel->l, *r = rel->r; /* look on left and right relations after possibly doing rel_copy */

	switch (rel->op) {
	case op_basetable: {
		sql_table *t = rel->l;

		/* when a basetable wraps a sql_table (->l) which is remote we want to store its remote
		 * uri to the REMOTE property. As the property is pulled up the tree it can be used in
		 * the case of binary rel operators (see later switch cases) in order to
		 * 1. resolve properly (same uri) replica tables in the other subtree (that's why we
		 *    call the rewrite_replica)
		 * 2. pull REMOTE over the binary op if the other subtree has a matching uri remote table
		 */
		if (t && isRemote(t) && (p = find_prop(rel->p, PROP_REMOTE)) == NULL) {
			if (t->query) {
				tid_uri *tu = SA_NEW(v->sql->sa, tid_uri);
				tu->id = t->base.id;
				tu->uri = mapiuri_uri(t->query, v->sql->sa);
				list *uris = sa_list(v->sql->sa);
				append(uris, tu);
				p = rel->p = prop_create(v->sql->sa, PROP_REMOTE, rel->p);
				p->value.pval = (void *)uris;
			}
		}
		if (t && isReplicaTable(t) && !list_empty(t->members)) {
			/* the replicas probably have at least one remote so
			 * 1. find all the remotes
			 * 2. store them in the PROP_REMOTE pval
			 */
			p = rel->p = prop_create(v->sql->sa, PROP_REMOTE, rel->p);
			list *uris = sa_list(v->sql->sa);
			for (node *n = t->members->h; n; n = n->next) {
				sql_part *part = n->data;
				sql_table *ptable = find_sql_table_id(v->sql->session->tr, t->s, part->member);
				if (isRemote(ptable)) {
					assert(ptable->query);
					tid_uri *tu = SA_NEW(v->sql->sa, tid_uri);
					tu->id = ptable->base.id;
					tu->uri = mapiuri_uri(ptable->query, v->sql->sa);
					append(uris, tu);
				}
			}
			p->value.pval = (void*)uris;
		}
	} break;
	case op_table:
		if (IS_TABLE_PROD_FUNC(rel->flag) || rel->flag == TABLE_FROM_RELATION) {
			if (l && (p = find_prop(l->p, PROP_REMOTE)) != NULL) {
				l->p = prop_remove(l->p, p);
				if (!find_prop(rel->p, PROP_REMOTE)) {
					p->p = rel->p;
					rel->p = p;
				}
			}
		} break;
	case op_join:
	case op_left:
	case op_right:
	case op_full:

	case op_semi:
	case op_anti:

	case op_union:
	case op_inter:
	case op_except:

	case op_insert:
	case op_update:
	case op_delete:
	case op_merge:

		if (rel->flag&MERGE_LEFT) /* search for any remote tables but don't propagate over to this relation */
			return rel;

		/* if both subtrees have REMOTE property with the common uri then pull it up */
		if (l && (pl = find_prop(l->p, PROP_REMOTE)) != NULL &&
			r && (pr = find_prop(r->p, PROP_REMOTE)) != NULL) {

			/* find the common uris */
			list* uris = sa_list(v->sql->sa);
			// TODO this double loop must go (maybe use the hashmap of the list)
			for (node* n = ((list*)pl->value.pval)->h; n; n = n->next) {
				for (node* m = ((list*)pr->value.pval)->h; m; m = m->next) {
					tid_uri* ltu = n->data;
					tid_uri* rtu = m->data;
					if (strcmp(ltu->uri, rtu->uri) == 0) {
						append(uris, n->data);
					}
				}
			}

			/* if there are common uris pull the REMOTE prop with the common uris up */
			if (!list_empty(uris)) {
				l->p = prop_remove(l->p, pl);
				r->p = prop_remove(r->p, pr);
				if (!find_prop(rel->p, PROP_REMOTE)) {
					/* set the new (matching) uris */
					pl->value.pval = uris;
					/* push the pl REMOTE property to the list of properties */
					pl->p = rel->p;
					rel->p = pl;
				} else {
					// TODO what if we are here? can that even happen?
				}
			}
		}
		break;
	case op_project:
	case op_select:
	case op_groupby:
	case op_topn:
	case op_sample:
	case op_truncate:
		/* if the subtree has the REMOTE property just pull it up */
		if (l && (p = find_prop(l->p, PROP_REMOTE)) != NULL) {
			l->p = prop_remove(l->p, p);
			if (!find_prop(rel->p, PROP_REMOTE)) {
				p->p = rel->p;
				rel->p = p;
			}
		}
		break;
	case op_ddl:
		// TODO properly handle the op_ddl
		/*if (rel->flag == ddl_output || rel->flag == ddl_create_seq || rel->flag == ddl_alter_seq [>|| rel->flag == ddl_alter_table || rel->flag == ddl_create_table || rel->flag == ddl_create_view<]) {*/
			/*if (l && (p = find_prop(l->p, PROP_REMOTE)) != NULL) {*/
				/*l->p = prop_remove(l->p, p);*/
				/*if (!find_prop(rel->p, PROP_REMOTE)) {*/
					/*p->p = rel->p;*/
					/*rel->p = p;*/
				/*}*/
			/*}*/
		/*} else if (rel->flag == ddl_list || rel->flag == ddl_exception) {*/
			/*if (l && (pl = find_prop(l->p, PROP_REMOTE)) != NULL &&*/
				/*r && (pr = find_prop(r->p, PROP_REMOTE)) != NULL &&*/
				/*strcmp(pl->value.pval, pr->value.pval) == 0) {*/
				/*l->p = prop_remove(l->p, pl);*/
				/*r->p = prop_remove(r->p, pr);*/
				/*if (!find_prop(rel->p, PROP_REMOTE)) {*/
					/*pl->p = rel->p;*/
					/*rel->p = pl;*/
				/*}*/
			/*}*/
		/*}*/
		break;
	}
	return rel;
}

static sql_rel *
rel_rewrite_remote_(visitor *v, sql_rel *rel)
{
	prop *p, *pl, *pr;

	if (!eliminate_remote_or_replica_refs(v, &rel))
		return rel;

	sql_rel *l = rel->l, *r = rel->r; /* look on left and right relations after possibly doing rel_copy */

	switch (rel->op) {
	case op_basetable: {
		sql_table *t = rel->l;

		/* when a basetable wraps a sql_table (->l) which is remote we want to store its remote
		 * uri to the REMOTE property. As the property is pulled up the tree it can be used in
		 * the case of binary rel operators (see later switch cases) in order to
		 * 1. resolve properly (same uri) replica tables in the other subtree (that's why we
		 *    call the rewrite_replica)
		 * 2. pull REMOTE over the binary op if the other subtree has a matching uri remote table
		 */
		if (t && isRemote(t) && (p = find_prop(rel->p, PROP_REMOTE)) == NULL) {
			p = rel->p = prop_create(v->sql->sa, PROP_REMOTE, rel->p);
			p->id = t->base.id;
			p->value.pval = (void *)mapiuri_uri(t->query, v->sql->sa);
		}
	} break;
	case op_table:
		if (IS_TABLE_PROD_FUNC(rel->flag) || rel->flag == TABLE_FROM_RELATION) {
			if (l && (p = find_prop(l->p, PROP_REMOTE)) != NULL) {
				l->p = prop_remove(l->p, p);
				if (!find_prop(rel->p, PROP_REMOTE)) {
					p->p = rel->p;
					rel->p = p;
				}
			}
		} break;
	case op_join:
	case op_left:
	case op_right:
	case op_full:

	case op_semi:
	case op_anti:

	case op_union:
	case op_inter:
	case op_except:

	case op_insert:
	case op_update:
	case op_delete:
	case op_merge:
		if (is_join(rel->op) && list_empty(rel->exps) &&
			find_prop(l->p, PROP_REMOTE) == NULL &&
			find_prop(r->p, PROP_REMOTE) == NULL) {
			/* cleanup replica's */
			visitor rv = { .sql = v->sql };

			l = rel->l = rel_visitor_bottomup(&rv, l, &rel_rewrite_replica_);
			rv.data = NULL;
			r = rel->r = rel_visitor_bottomup(&rv, r, &rel_rewrite_replica_);
			if ((!l || !r) && v->sql->session->status) /* if the recursive calls failed */
				return NULL;
		}
		if ((is_join(rel->op) || is_semi(rel->op) || is_set(rel->op)) &&
			(pl = find_prop(l->p, PROP_REMOTE)) != NULL &&
			find_prop(r->p, PROP_REMOTE) == NULL) {
			visitor rv = { .sql = v->sql, .data = pl->value.pval };

			if (!(r = rel_visitor_bottomup(&rv, r, &rel_rewrite_replica_)) && v->sql->session->status)
				return NULL;
			rv.data = NULL;
			if (!(r = rel->r = rel_visitor_bottomup(&rv, r, &rel_rewrite_remote_)) && v->sql->session->status)
				return NULL;
		} else if ((is_join(rel->op) || is_semi(rel->op) || is_set(rel->op)) &&
			find_prop(l->p, PROP_REMOTE) == NULL &&
			(pr = find_prop(r->p, PROP_REMOTE)) != NULL) {
			visitor rv = { .sql = v->sql, .data = pr->value.pval };

			if (!(l = rel_visitor_bottomup(&rv, l, &rel_rewrite_replica_)) && v->sql->session->status)
				return NULL;
			rv.data = NULL;
			if (!(l = rel->l = rel_visitor_bottomup(&rv, l, &rel_rewrite_remote_)) && v->sql->session->status)
				return NULL;
		}

		if (rel->flag&MERGE_LEFT) /* search for any remote tables but don't propagate over to this relation */
			return rel;

		/* if both subtrees have the REMOTE property with the same uri then pull it up */
		if (l && (pl = find_prop(l->p, PROP_REMOTE)) != NULL &&
			r && (pr = find_prop(r->p, PROP_REMOTE)) != NULL &&
			strcmp(pl->value.pval, pr->value.pval) == 0) {
			l->p = prop_remove(l->p, pl);
			r->p = prop_remove(r->p, pr);
			if (!find_prop(rel->p, PROP_REMOTE)) {
				pl->p = rel->p;
				rel->p = pl;
			}
		}
		break;
	case op_project:
	case op_select:
	case op_groupby:
	case op_topn:
	case op_sample:
	case op_truncate:
		if (l && (p = find_prop(l->p, PROP_REMOTE)) != NULL) {
			l->p = prop_remove(l->p, p);
			if (!find_prop(rel->p, PROP_REMOTE)) {
				p->p = rel->p;
				rel->p = p;
			}
		}
		break;
	case op_ddl:
		if (rel->flag == ddl_output || rel->flag == ddl_create_seq || rel->flag == ddl_alter_seq /*|| rel->flag == ddl_alter_table || rel->flag == ddl_create_table || rel->flag == ddl_create_view*/) {
			if (l && (p = find_prop(l->p, PROP_REMOTE)) != NULL) {
				l->p = prop_remove(l->p, p);
				if (!find_prop(rel->p, PROP_REMOTE)) {
					p->p = rel->p;
					rel->p = p;
				}
			}
		} else if (rel->flag == ddl_list || rel->flag == ddl_exception) {
			if (l && (pl = find_prop(l->p, PROP_REMOTE)) != NULL &&
				r && (pr = find_prop(r->p, PROP_REMOTE)) != NULL &&
				strcmp(pl->value.pval, pr->value.pval) == 0) {
				l->p = prop_remove(l->p, pl);
				r->p = prop_remove(r->p, pr);
				if (!find_prop(rel->p, PROP_REMOTE)) {
					pl->p = rel->p;
					rel->p = pl;
				}
			}
		}
		break;
	}
	return rel;
}

static sql_rel *
rel_rewrite_remote(visitor *v, global_props *gp, sql_rel *rel)
{
	(void) gp;
	// TODO next call is no-op remove together with the rel_rewrite_remote_ impl
	rel_visitor_bottomup(v, NULL, &rel_rewrite_remote_);
	rel = rel_visitor_bottomup(v, rel, &rel_rewrite_remote_2_);
	return rel;
}

run_optimizer
bind_rewrite_remote(visitor *v, global_props *gp)
{
	(void) v;
	return gp->needs_mergetable_rewrite || gp->needs_remote_replica_rewrite ? rel_rewrite_remote : NULL;
}


static sql_rel *
rel_remote_func_(visitor *v, sql_rel *rel)
{
	(void) v;

	/* Don't modify the same relation twice */
	if (is_rel_remote_func_used(rel->used))
		return rel;
	rel->used |= rel_remote_func_used;

	if (find_prop(rel->p, PROP_REMOTE) != NULL) {
		list *exps = rel_projections(v->sql, rel, NULL, 1, 1);
		rel = rel_relational_func(v->sql->sa, rel, exps);
	}
	return rel;
}

static sql_rel *
rel_remote_func(visitor *v, global_props *gp, sql_rel *rel)
{
	(void) gp;
	return rel_visitor_bottomup(v, rel, &rel_remote_func_);
}

run_optimizer
bind_remote_func(visitor *v, global_props *gp)
{
	(void) v;
	return gp->needs_mergetable_rewrite || gp->needs_remote_replica_rewrite ? rel_remote_func : NULL;
}
