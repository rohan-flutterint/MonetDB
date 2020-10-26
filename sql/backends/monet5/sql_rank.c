/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2020 MonetDB B.V.
 */

#include "monetdb_config.h"
#include "sql_rank.h"
#include "gdk_analytic.h"

#define voidresultBAT(r,tpe,cnt,b,err)					\
	do {								\
		r = COLnew(b->hseqbase, tpe, cnt, TRANSIENT);		\
		if (r == NULL) {					\
			BBPunfix(b->batCacheid);			\
			throw(MAL, err, SQLSTATE(HY013) MAL_MALLOC_FAIL); \
		}							\
		r->tsorted = false;					\
		r->trevsorted = false;					\
		r->tnonil = true;					\
	} while (0)

static void
unfix_inputs(int nargs, ...)
{
	va_list valist;

	va_start(valist, nargs);
	for (int i = 0; i < nargs; i++) {
		BAT *b = va_arg(valist, BAT *);
		if (b)
			BBPunfix(b->batCacheid);
	}
	va_end(valist);
}

static void
finalize_output(bat *res, BAT *r, str msg)
{
	if (r && !msg) {
		r->tsorted = BATcount(r) <= 1;
		r->trevsorted = BATcount(r) <= 1;
		BBPkeepref(*res = r->batCacheid);
	} else if (r)
		BBPreclaim(r);
}

str
SQLdiff(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	(void)cntxt;
	if (isaBatType(getArgType(mb, pci, 1))) {
		bat *res = getArgReference_bat(stk, pci, 0);
		bat *bid = getArgReference_bat(stk, pci, 1);
		BAT *b = BATdescriptor(*bid), *c, *r;
		gdk_return gdk_code = GDK_SUCCEED;

		if (!b)
			throw(SQL, "sql.diff", SQLSTATE(HY005) "Cannot access column descriptor");
		if (pci->argc > 2) {
			if (isaBatType(getArgType(mb, pci, 2))) {
				voidresultBAT(r, TYPE_bit, BATcount(b), b, "sql.diff");
				c = b;
				bid = getArgReference_bat(stk, pci, 2);
				b = BATdescriptor(*bid);
				if (!b) {
					BBPunfix(c->batCacheid);
					throw(SQL, "sql.diff", SQLSTATE(HY005) "Cannot access column descriptor");
				}
				gdk_code = GDKanalyticaldiff(r, b, c, b->ttype);
				BBPunfix(c->batCacheid);
			} else { /* the input is a constant, so the output is the previous sql.diff output */
				assert(b->ttype == TYPE_bit);
				r = COLcopy(b, TYPE_bit, false, TRANSIENT);
				if (!r) {
					BBPunfix(b->batCacheid);
					throw(SQL, "sql.diff", SQLSTATE(HY013) MAL_MALLOC_FAIL);
				}
			}
		} else {
			voidresultBAT(r, TYPE_bit, BATcount(b), b, "sql.diff");
			gdk_code = GDKanalyticaldiff(r, b, NULL, b->ttype);
		}
		BBPunfix(b->batCacheid);
		if (gdk_code == GDK_SUCCEED)
			BBPkeepref(*res = r->batCacheid);
		else
			throw(SQL, "sql.diff", GDK_EXCEPTION);
	} else if (pci->argc > 2 && isaBatType(getArgType(mb, pci, 2))) {
		bat *res = getArgReference_bat(stk, pci, 0);
		bit prev = *getArgReference_bit(stk, pci, 1);
		bat *bid = getArgReference_bat(stk, pci, 2);
		BAT *b = BATdescriptor(*bid), *r, *c;
		bit *restrict cb;
		gdk_return gdk_code = GDK_SUCCEED;

		if (!b)
			throw(SQL, "sql.diff", SQLSTATE(HY005) "Cannot access column descriptor");
		voidresultBAT(r, TYPE_bit, BATcount(b), b, "sql.diff");

		c = COLnew(0, TYPE_bit, BATcount(b), TRANSIENT);
		if (!c) {
			BBPunfix(b->batCacheid);
			throw(SQL, "sql.diff", SQLSTATE(HY013) MAL_MALLOC_FAIL);
		}
		cb = (bit *) Tloc(c, 0);
		memset(cb, prev, BATcount(b));

		gdk_code = GDKanalyticaldiff(r, b, c, b->ttype);
		BBPunfix(b->batCacheid);
		BBPreclaim(c);
		if (gdk_code == GDK_SUCCEED)
			BBPkeepref(*res = r->batCacheid);
		else
			throw(SQL, "sql.diff", GDK_EXCEPTION);
	} else {
		bit *res = getArgReference_bit(stk, pci, 0);
		*res = FALSE;
	}
	return MAL_SUCCEED;
}

str
SQLwindow_bound(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	str msg = MAL_SUCCEED;
	bool preceding;
	lng first_half;
	int unit, bound, excl, part_offset = (pci->argc > 6);

	if ((pci->argc != 6 && pci->argc != 7) || getArgType(mb, pci, part_offset + 2) != TYPE_int ||
		getArgType(mb, pci, part_offset + 3) != TYPE_int || getArgType(mb, pci, part_offset + 4) != TYPE_int) {
		throw(SQL, "sql.window_bound", SQLSTATE(42000) "Invalid arguments");
	}

	unit = *getArgReference_int(stk, pci, part_offset + 2);
	bound = *getArgReference_int(stk, pci, part_offset + 3);
	excl = *getArgReference_int(stk, pci, part_offset + 4);

	assert(unit >= 0 && unit <= 3);
	assert(bound >= 0 && bound <= 5);
	assert(excl >= 0 && excl <= 2);
	preceding = (bound % 2 == 0);
	first_half = (bound < 2 || bound == 4);

	(void)cntxt;
	if (isaBatType(getArgType(mb, pci, 1))) {
		bat *res = getArgReference_bat(stk, pci, 0);
		BAT *b = BATdescriptor(*getArgReference_bat(stk, pci, part_offset + 1)), *p = NULL, *r, *l = NULL;
		int tp1, tp2 = getArgType(mb, pci, part_offset + 5);
		void *limit = NULL;
		bool is_a_bat;

		if (!b)
			throw(SQL, "sql.window_bound", SQLSTATE(HY005) "Cannot access column descriptor");
		tp1 = b->ttype;

		if (excl != 0) {
			BBPunfix(b->batCacheid);
			throw(SQL, "sql.window_bound", SQLSTATE(42000) "Only EXCLUDE NO OTHERS exclusion is currently implemented");
		}

		is_a_bat = isaBatType(tp2);
		if (is_a_bat)
			tp2 = getBatType(tp2);

		voidresultBAT(r, TYPE_lng, BATcount(b), b, "sql.window_bound");
		if (is_a_bat) { //SQL_CURRENT_ROW shall never fall in limit validation
			if (!(l = BATdescriptor(*getArgReference_bat(stk, pci, part_offset + 5)))) {
				BBPunfix(b->batCacheid);
				throw(SQL, "sql.window_bound", SQLSTATE(HY005) "Cannot access column descriptor");
			}
		} else {
			limit = getArgReference(stk, pci, part_offset + 5);
		}
		if (part_offset) {
			if (!(p = BATdescriptor(*getArgReference_bat(stk, pci, 1)))) {
				if(l) BBPunfix(l->batCacheid);
				BBPunfix(b->batCacheid);
				throw(SQL, "sql.window_bound", SQLSTATE(HY005) "Cannot access column descriptor");
			}
		}

		//On RANGE frame, when "CURRENT ROW" is not specified, the ranges are calculated with SQL intervals in mind
		if (GDKanalyticalwindowbounds(r, b, p, l, limit, tp1, tp2, unit, preceding, first_half) == GDK_SUCCEED)
			BBPkeepref(*res = r->batCacheid);
		else
			msg = createException(SQL, "sql.window_bound", GDK_EXCEPTION);
		if(l) BBPunfix(l->batCacheid);
		if(p) BBPunfix(p->batCacheid);
		BBPunfix(b->batCacheid);
	} else {
		lng *res = getArgReference_lng(stk, pci, 0);

		*res = preceding ? -first_half : first_half;
	}
	return msg;
}

str
SQLrow_number(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	if (pci->argc != 4 ||
		(getArgType(mb, pci, 2) != TYPE_bit && getBatType(getArgType(mb, pci, 2)) != TYPE_bit) ||
		(getArgType(mb, pci, 3) != TYPE_bit && getBatType(getArgType(mb, pci, 3)) != TYPE_bit)){
		throw(SQL, "sql.row_number", SQLSTATE(42000) "row_number(:any_1,:bit,:bit)");
	}
	(void)cntxt;
	if (isaBatType(getArgType(mb, pci, 1))) {
		bat *res = getArgReference_bat(stk, pci, 0);
		BAT *b = BATdescriptor(*getArgReference_bat(stk, pci, 1)), *p, *r;
		BUN cnt;
		int j, *rp, *end;
		bit *np;

		if (!b)
			throw(SQL, "sql.row_number", SQLSTATE(HY005) "Cannot access column descriptor");
		cnt = BATcount(b);
	 	voidresultBAT(r, TYPE_int, cnt, b, "sql.row_number");
		rp = (int*)Tloc(r, 0);
		if (isaBatType(getArgType(mb, pci, 2))) {
			/* order info not used */
			p = BATdescriptor(*getArgReference_bat(stk, pci, 2));
			if (!p) {
				BBPunfix(b->batCacheid);
				throw(SQL, "sql.row_number", SQLSTATE(HY005) "Cannot access column descriptor");
			}
			np = (bit*)Tloc(p, 0);
			end = rp + cnt;
			for(j=1; rp<end; j++, np++, rp++) {
				if (*np)
					j=1;
				*rp = j;
			}
			BBPunfix(p->batCacheid);
		} else { /* single value, ie no partitions, order info not used */
			int icnt = (int) cnt;
			for(j=1; j<=icnt; j++, rp++)
				*rp = j;
			r->tsorted = true;
			r->tkey = true;
		}
		BATsetcount(r, cnt);
		r->tnonil = true;
		r->tnil = false;
		BBPunfix(b->batCacheid);
		BBPkeepref(*res = r->batCacheid);
	} else {
		int *res = getArgReference_int(stk, pci, 0);

		*res = 1;
	}
	return MAL_SUCCEED;
}

str
SQLrank(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	if (pci->argc != 4 ||
		(getArgType(mb, pci, 2) != TYPE_bit && getBatType(getArgType(mb, pci, 2)) != TYPE_bit) ||
		(getArgType(mb, pci, 3) != TYPE_bit && getBatType(getArgType(mb, pci, 3)) != TYPE_bit)){
		throw(SQL, "sql.rank", SQLSTATE(42000) "rank(:any_1,:bit,:bit)");
	}
	(void)cntxt;
	if (isaBatType(getArgType(mb, pci, 1))) {
		bat *res = getArgReference_bat(stk, pci, 0);
		BAT *b = BATdescriptor(*getArgReference_bat(stk, pci, 1)), *p, *o, *r;
		BUN cnt;
		int j, k, *rp, *end;
		bit *np, *no;

		if (!b)
			throw(SQL, "sql.rank", SQLSTATE(HY005) "Cannot access column descriptor");
		cnt = BATcount(b);
		voidresultBAT(r, TYPE_int, cnt, b, "sql.rank");
		rp = (int*)Tloc(r, 0);
		end = rp + cnt;
		if (isaBatType(getArgType(mb, pci, 2))) {
			if (isaBatType(getArgType(mb, pci, 3))) {
				p = BATdescriptor(*getArgReference_bat(stk, pci, 2));
				o = BATdescriptor(*getArgReference_bat(stk, pci, 3));
				if (!p || !o) {
					BBPunfix(b->batCacheid);
					if (p) BBPunfix(p->batCacheid);
					if (o) BBPunfix(o->batCacheid);
					throw(SQL, "sql.rank", SQLSTATE(HY005) "Cannot access column descriptor");
				}
				np = (bit*)Tloc(p, 0);
				no = (bit*)Tloc(o, 0);
				for(j=1,k=1; rp<end; k++, np++, no++, rp++) {
					if (*np)
						j=k=1;
					if (*no)
						j=k;
					*rp = j;
				}
				BBPunfix(p->batCacheid);
				BBPunfix(o->batCacheid);
			} else { /* single value, ie no ordering */
				p = BATdescriptor(*getArgReference_bat(stk, pci, 2));
				if (!p) {
					BBPunfix(b->batCacheid);
					throw(SQL, "sql.rank", SQLSTATE(HY005) "Cannot access column descriptor");
				}
				np = (bit*)Tloc(p, 0);
				for(j=1,k=1; rp<end; k++, np++, rp++) {
					if (*np)
						j=k=1;
					*rp = j;
				}
				BBPunfix(p->batCacheid);
			}
		} else { /* single value, ie no partitions */
			if (isaBatType(getArgType(mb, pci, 3))) {
				o = BATdescriptor(*getArgReference_bat(stk, pci, 3));
				if (!o) {
					BBPunfix(b->batCacheid);
					throw(SQL, "sql.rank", SQLSTATE(HY005) "Cannot access column descriptor");
				}
				no = (bit*)Tloc(o, 0);
				for(j=1,k=1; rp<end; k++, no++, rp++) {
					if (*no)
						j=k;
					*rp = j;
				}
				BBPunfix(o->batCacheid);
			} else { /* single value, ie no ordering */
				int icnt = (int) cnt;
				for(j=1; j<=icnt; j++, rp++)
					*rp = j;
				r->tsorted = true;
				r->tkey = true;
			}
		}
		BATsetcount(r, cnt);
		r->tnonil = true;
		r->tnil = false;
		BBPunfix(b->batCacheid);
		BBPkeepref(*res = r->batCacheid);
	} else {
		int *res = getArgReference_int(stk, pci, 0);

		*res = 1;
	}
	return MAL_SUCCEED;
}

str
SQLdense_rank(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	if (pci->argc != 4 ||
		(getArgType(mb, pci, 2) != TYPE_bit && getBatType(getArgType(mb, pci, 2)) != TYPE_bit) ||
		(getArgType(mb, pci, 3) != TYPE_bit && getBatType(getArgType(mb, pci, 3)) != TYPE_bit)){
		throw(SQL, "sql.dense_rank", SQLSTATE(42000) "dense_rank(:any_1,:bit,:bit)");
	}
	(void)cntxt;
	if (isaBatType(getArgType(mb, pci, 1))) {
		bat *res = getArgReference_bat(stk, pci, 0);
		BAT *b = BATdescriptor(*getArgReference_bat(stk, pci, 1)), *p, *o, *r;
		BUN cnt;
		int j, *rp, *end;
		bit *np, *no;

		if (!b)
			throw(SQL, "sql.dense_rank", SQLSTATE(HY005) "Cannot access column descriptor");
		cnt = BATcount(b);
		voidresultBAT(r, TYPE_int, cnt, b, "sql.dense_rank");
		rp = (int*)Tloc(r, 0);
		end = rp + cnt;
		if (isaBatType(getArgType(mb, pci, 2))) {
			if (isaBatType(getArgType(mb, pci, 3))) {
				p = BATdescriptor(*getArgReference_bat(stk, pci, 2));
				o = BATdescriptor(*getArgReference_bat(stk, pci, 3));
				if (!p || !o) {
					BBPunfix(b->batCacheid);
					if (p) BBPunfix(p->batCacheid);
					if (o) BBPunfix(o->batCacheid);
					throw(SQL, "sql.dense_rank", SQLSTATE(HY005) "Cannot access column descriptor");
				}
				np = (bit*)Tloc(p, 0);
				no = (bit*)Tloc(o, 0);
				for(j=1; rp<end; np++, no++, rp++) {
					if (*np)
						j=1;
					else if (*no)
						j++;
					*rp = j;
				}
				BBPunfix(p->batCacheid);
				BBPunfix(o->batCacheid);
			} else { /* single value, ie no ordering */
				p = BATdescriptor(*getArgReference_bat(stk, pci, 2));
				if (!p) {
					BBPunfix(b->batCacheid);
					throw(SQL, "sql.dense_rank", SQLSTATE(HY005) "Cannot access column descriptor");
				}
				np = (bit*)Tloc(p, 0);
				for(j=1; rp<end; np++, rp++) {
					if (*np)
						j=1;
					*rp = j;
				}
				BBPunfix(p->batCacheid);
			}
		} else { /* single value, ie no partitions */
			if (isaBatType(getArgType(mb, pci, 3))) {
				o = BATdescriptor(*getArgReference_bat(stk, pci, 3));
				if (!o) {
					BBPunfix(b->batCacheid);
					throw(SQL, "sql.dense_rank", SQLSTATE(HY005) "Cannot access column descriptor");
				}
				no = (bit*)Tloc(o, 0);
				for(j=1; rp<end; no++, rp++) {
					if (*no)
						j++;
					*rp = j;
				}
				BBPunfix(o->batCacheid);
			} else { /* single value, ie no ordering */
				int icnt = (int) cnt;
				for(j=1; j<=icnt; j++, rp++)
					*rp = j;
				r->tsorted = true;
				r->tkey = true;
			}
		}
		BATsetcount(r, cnt);
		r->tnonil = true;
		r->tnil = false;
		BBPunfix(b->batCacheid);
		BBPkeepref(*res = r->batCacheid);
	} else {
		int *res = getArgReference_int(stk, pci, 0);

		*res = 1;
	}
	return MAL_SUCCEED;
}

str
SQLpercent_rank(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	if (pci->argc != 4 ||
		(getArgType(mb, pci, 2) != TYPE_bit && getBatType(getArgType(mb, pci, 2)) != TYPE_bit) ||
		(getArgType(mb, pci, 3) != TYPE_bit && getBatType(getArgType(mb, pci, 3)) != TYPE_bit)){
		throw(SQL, "sql.percent_rank", SQLSTATE(42000) "percent_rank(:any_1,:bit,:bit)");
	}
	(void)cntxt;
	if (isaBatType(getArgType(mb, pci, 1))) {
		bat *res = getArgReference_bat(stk, pci, 0);
		BAT *b = BATdescriptor(*getArgReference_bat(stk, pci, 1)), *p, *o, *r;
		BUN cnt;
		int j, k;
		dbl *rp, *end, cnt_cast;
		bit *np, *no;

		if (!b)
			throw(SQL, "sql.percent_rank", SQLSTATE(HY005) "Cannot access column descriptor");
		cnt = BATcount(b);
		cnt_cast = (dbl) (cnt - 1);
		voidresultBAT(r, TYPE_dbl, cnt, b, "sql.percent_rank");
		rp = (dbl*)Tloc(r, 0);
		end = rp + cnt;
		if (isaBatType(getArgType(mb, pci, 2))) {
			if (isaBatType(getArgType(mb, pci, 3))) {
				p = BATdescriptor(*getArgReference_bat(stk, pci, 2));
				o = BATdescriptor(*getArgReference_bat(stk, pci, 3));
				if (!p || !o) {
					BBPunfix(b->batCacheid);
					if (p) BBPunfix(p->batCacheid);
					if (o) BBPunfix(o->batCacheid);
					throw(SQL, "sql.percent_rank", SQLSTATE(HY005) "Cannot access column descriptor");
				}
				np = (bit*)Tloc(p, 0);
				no = (bit*)Tloc(o, 0);
				for(j=0,k=0; rp<end; k++, np++, no++, rp++) {
					if (*np)
						j=k=0;
					if (*no)
						j=k;
					*rp = j / cnt_cast;
				}
				BBPunfix(p->batCacheid);
				BBPunfix(o->batCacheid);
			} else { /* single value, ie no ordering */
				p = BATdescriptor(*getArgReference_bat(stk, pci, 2));
				if (!p) {
					BBPunfix(b->batCacheid);
					throw(SQL, "sql.percent_rank", SQLSTATE(HY005) "Cannot access column descriptor");
				}
				np = (bit*)Tloc(p, 0);
				for(j=0; rp<end; np++, rp++) {
					if (*np)
						j=0;
					*rp = j / cnt_cast;
				}
				BBPunfix(p->batCacheid);
			}
		} else { /* single value, ie no partitions */
			if (isaBatType(getArgType(mb, pci, 3))) {
				o = BATdescriptor(*getArgReference_bat(stk, pci, 3));
				if (!o) {
					BBPunfix(b->batCacheid);
					throw(SQL, "sql.percent_rank", SQLSTATE(HY005) "Cannot access column descriptor");
				}
				no = (bit*)Tloc(o, 0);
				for(j=0,k=0; rp<end; k++, no++, rp++) {
					if (*no)
						j=k;
					*rp = j / cnt_cast;
				}
				BBPunfix(o->batCacheid);
			} else { /* single value, ie no ordering - the outcome will always be 0 */
				for(; rp<end; rp++)
					*rp = 0;
				r->tsorted = true;
				r->trevsorted = true;
			}
		}
		BATsetcount(r, cnt);
		r->tnonil = true;
		r->tnil = false;
		BBPunfix(b->batCacheid);
		BBPkeepref(*res = r->batCacheid);
	} else {
		dbl *res = getArgReference_dbl(stk, pci, 0);

		*res = 1;
	}
	return MAL_SUCCEED;
}

str
SQLcume_dist(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	if (pci->argc != 4 ||
		(getArgType(mb, pci, 2) != TYPE_bit && getBatType(getArgType(mb, pci, 2)) != TYPE_bit) ||
		(getArgType(mb, pci, 3) != TYPE_bit && getBatType(getArgType(mb, pci, 3)) != TYPE_bit)){
		throw(SQL, "sql.cume_dist", SQLSTATE(42000) "cume_dist(:any_1,:bit,:bit)");
	}
	(void)cntxt;
	if (isaBatType(getArgType(mb, pci, 1))) {
		bat *res = getArgReference_bat(stk, pci, 0);
		BAT *b = BATdescriptor(*getArgReference_bat(stk, pci, 1)), *p, *o, *r;
		BUN ncnt, j = 0;
		bit *np, *no, *bo1, *bo2, *end;
		dbl *rb, *rp, cnt_cast, nres;

		if (!b)
			throw(SQL, "sql.cume_dist", SQLSTATE(HY005) "Cannot access column descriptor");
		voidresultBAT(r, TYPE_dbl, BATcount(b), b, "sql.cume_dist");
		rb = rp = (dbl*)Tloc(r, 0);
		if (isaBatType(getArgType(mb, pci, 2))) {
			if (isaBatType(getArgType(mb, pci, 3))) {
				p = BATdescriptor(*getArgReference_bat(stk, pci, 2));
				o = BATdescriptor(*getArgReference_bat(stk, pci, 3));
				if (!p || !o) {
					BBPunfix(b->batCacheid);
					if (p) BBPunfix(p->batCacheid);
					if (o) BBPunfix(o->batCacheid);
					throw(SQL, "sql.cume_dist", SQLSTATE(HY005) "Cannot access column descriptor");
				}
				np = (bit*)Tloc(p, 0);
				end = np + BATcount(p);
				bo1 = bo2 = no = (bit*)Tloc(o, 0);

				for (; np<end; np++, no++) {
					if (*np) {
						ncnt = no - bo2;
						cnt_cast = (dbl) ncnt;
						j = 0;
						for (; bo2<no; bo2++) {
							if (*bo2) {
								j += (bo2 - bo1);
								nres = j / cnt_cast;
								for (; bo1 < bo2; bo1++, rb++)
									*rb = nres;
							}
						}
						for (; bo1 < bo2; bo1++, rb++)
							*rb = 1;
					}
				}
				j = 0;
				ncnt = no - bo2;
				cnt_cast = (dbl) ncnt;
				for (; bo2<no; bo2++) {
					if (*bo2) {
						j += (bo2 - bo1);
						nres = j / cnt_cast;
						for (; bo1 < bo2; bo1++, rb++)
							*rb = nres;
					}
				}
				for (; bo1 < bo2; bo1++, rb++)
					*rb = 1;
			} else { /* single value, ie no ordering */
				rp = rb + BATcount(b);
				for (; rb<rp; rb++)
					*rb = 1;
				r->tsorted = true;
				r->trevsorted = true;
			}
		} else { /* single value, ie no partitions */
			if (isaBatType(getArgType(mb, pci, 3))) {
				o = BATdescriptor(*getArgReference_bat(stk, pci, 3));
				if (!o) {
					BBPunfix(b->batCacheid);
					throw(SQL, "sql.cume_dist", SQLSTATE(HY005) "Cannot access column descriptor");
				}
				bo1 = bo2 = (bit*)Tloc(o, 0);
				no = bo1 + BATcount(b);
				cnt_cast = (dbl) BATcount(b);
				for (; bo2<no; bo2++) {
					if (*bo2) {
						j += (bo2 - bo1);
						nres = j / cnt_cast;
						for (; bo1 < bo2; bo1++, rb++)
							*rb = nres;
					}
				}
				for (; bo1 < bo2; bo1++, rb++)
					*rb = 1;
				BBPunfix(o->batCacheid);
			} else { /* single value, ie no ordering */
				rp = rb + BATcount(b);
				for (; rb<rp; rb++)
					*rb = 1;
				r->tsorted = true;
				r->trevsorted = true;
			}
		}
		BATsetcount(r, BATcount(b));
		r->tnonil = true;
		r->tnil = false;
		BBPunfix(b->batCacheid);
		BBPkeepref(*res = r->batCacheid);
	} else {
		dbl *res = getArgReference_dbl(stk, pci, 0);

		*res = 1;
	}
	return MAL_SUCCEED;
}

#define NTILE_VALUE_SINGLE_IMP(TPE) \
	do { \
		TPE val = *(TPE*) VALget(ntile); \
		if (!is_##TPE##_nil(val) && val < 1) { \
			msg = createException(SQL, "sql.ntile", SQLSTATE(42000) "ntile must be greater than zero"); \
			goto bailout; 	\
		}	\
		if (!is_##TPE##_nil(val)) \
			val = 1; \
		VALset(res, tp2, &val); \
	} while(0)

str
SQLntile(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	BAT *r = NULL, *b = NULL, *p = NULL, *n = NULL;
	bat *res = NULL;
	str msg = MAL_SUCCEED;

	(void)cntxt;
	if (pci->argc != 5 || (getArgType(mb, pci, 3) != TYPE_bit && getBatType(getArgType(mb, pci, 3)) != TYPE_bit) ||
		(getArgType(mb, pci, 4) != TYPE_bit && getBatType(getArgType(mb, pci, 4)) != TYPE_bit)) {
		throw(SQL, "sql.ntile", SQLSTATE(42000) "ntile(:any_1,:number,:bit,:bit)");
	}

	if (isaBatType(getArgType(mb, pci, 1))) {
		int tp2 = 0;
		void *ntile = NULL;
		res = getArgReference_bat(stk, pci, 0);

		if (!(b = BATdescriptor(*getArgReference_bat(stk, pci, 1)))) {
			msg = createException(SQL, "sql.ntile", SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}
		if (isaBatType(getArgType(mb, pci, 2))) {
			tp2 = getBatType(getArgType(mb, pci, 2));
			if (!(n = BATdescriptor(*getArgReference_bat(stk, pci, 2)))) {
				msg = createException(SQL, "sql.ntile", SQLSTATE(HY005) "Cannot access column descriptor");
				goto bailout;
			}
		} else {
			tp2 = getArgType(mb, pci, 2);
			ntile = getArgReference(stk, pci, 2);
		}
		if (!(r = COLnew(b->hseqbase, tp2, BATcount(b), TRANSIENT))) {
			msg = createException(MAL, "sql.ntile", SQLSTATE(HY013) MAL_MALLOC_FAIL);
			goto bailout;
		}
		if (isaBatType(getArgType(mb, pci, 3)) && !(p = BATdescriptor(*getArgReference_bat(stk, pci, 3)))) {
			msg = createException(SQL, "sql.ntile", SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}

		if (GDKanalyticalntile(r, b, p, n, tp2, ntile) != GDK_SUCCEED)
			msg = createException(SQL, "sql.ntile", GDK_EXCEPTION);
	} else {
		ValRecord *res = &(stk)->stk[(pci)->argv[0]];
		ValRecord *ntile = &(stk)->stk[(pci)->argv[2]];
		int tp2 = getArgType(mb, pci, 2);

		switch (tp2) {
			case TYPE_bte:
				NTILE_VALUE_SINGLE_IMP(bte);
				break;
			case TYPE_sht:
				NTILE_VALUE_SINGLE_IMP(sht);
				break;
			case TYPE_int:
				NTILE_VALUE_SINGLE_IMP(int);
				break;
			case TYPE_lng:
				NTILE_VALUE_SINGLE_IMP(lng);
				break;
#ifdef HAVE_HGE
			case TYPE_hge:
				NTILE_VALUE_SINGLE_IMP(hge);
				break;
#endif
			default:
				msg = createException(SQL, "sql.ntile", SQLSTATE(42000) "ntile not available for %s", ATOMname(tp2));
		}
	}

bailout:
	unfix_inputs(3, b, p, n);
	finalize_output(res, r, msg);
	return MAL_SUCCEED;
}

static str
SQLanalytics_args(BAT **r, BAT **b, int *frame_type, BAT **p, BAT **o, BAT **s, BAT **e, Client cntxt,
				  MalBlkPtr mb, MalStkPtr stk, InstrPtr pci, int rtype, bool has_bounds,
				  int max_arg, const char *mod)
{
	(void) cntxt;
	if ((has_bounds && pci->argc != max_arg && pci->argc != max_arg - 1) ||
		(!has_bounds && pci->argc != max_arg && pci->argc != max_arg - 1 && pci->argc != max_arg + 1))
		throw(SQL, mod, "%s: wrong number of arguments to function %s", mod, mod);

	if (isaBatType(getArgType(mb, pci, 1)) && !(*b = BATdescriptor(*getArgReference_bat(stk, pci, 1))))
		throw(SQL, mod, SQLSTATE(HY005) "Cannot access column descriptor");
	if (*b && !(*r = COLnew((*b)->hseqbase, rtype ? rtype : (*b)->ttype, BATcount(*b), TRANSIENT)))
		throw(MAL, mod, SQLSTATE(HY013) MAL_MALLOC_FAIL); 
	*frame_type = *getArgReference_int(stk, pci, 2);

	if (has_bounds) {
		if (isaBatType(getArgType(mb, pci, 3)) && !(*s = BATdescriptor(*getArgReference_bat(stk, pci, 3))))
			throw(SQL, mod, SQLSTATE(HY005) "Cannot access column descriptor");
		if (isaBatType(getArgType(mb, pci, 4)) && !(*e = BATdescriptor(*getArgReference_bat(stk, pci, 4))))
			throw(SQL, mod, SQLSTATE(HY005) "Cannot access column descriptor");
	} else if (*frame_type == 3 || *frame_type == 4) {
		if (isaBatType(getArgType(mb, pci, 3)) && !(*o = BATdescriptor(*getArgReference_bat(stk, pci, 3))))
			throw(SQL, mod, SQLSTATE(HY005) "Cannot access column descriptor");
		max_arg++;
	}
	if (pci->argc == max_arg && isaBatType(getArgType(mb, pci, max_arg - 1))) {
		if (!(*p = BATdescriptor(*getArgReference_bat(stk, pci, max_arg - 1))))
			throw(SQL, mod, SQLSTATE(HY005) "Cannot access column descriptor");
	}

	return MAL_SUCCEED;
}

static str
SQLanalytical_func(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci, bool has_bounds, int max_arg, const char *op,
				   gdk_return (*func)(BAT *, BAT *, BAT *, BAT *, BAT *, BAT *, int, int))
{
	int tpe = getArgType(mb, pci, 1), frame_type;
	BAT *r = NULL, *b = NULL, *p = NULL, *o = NULL, *s = NULL, *e = NULL;
	str msg = SQLanalytics_args(&r, &b, &frame_type, &p, &o, &s, &e, cntxt, mb, stk, pci, 0, has_bounds, max_arg, op);
	bat *res = NULL;

	if (msg)
		goto bailout;
	if (b) {
		res = getArgReference_bat(stk, pci, 0);

		if (func(r, p, o, b, s, e, getBatType(tpe), frame_type) != GDK_SUCCEED)
			msg = createException(SQL, op, GDK_EXCEPTION);
	} else {
		ValRecord *res = &(stk)->stk[(pci)->argv[0]];
		ValRecord *in = &(stk)->stk[(pci)->argv[1]];

		if (!VALcopy(res, in))
			msg = createException(SQL, op, SQLSTATE(HY013) MAL_MALLOC_FAIL);
	}

bailout:
	unfix_inputs(5, b, p, o, s, e);
	finalize_output(res, r, msg);
	return msg;
}

static str
do_limit_value(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci, const char *op,
			   gdk_return (*func)(BAT *, BAT *, BAT *, BAT *, int))
{
	int tpe = getArgType(mb, pci, 1);
	BAT *r = NULL, *b = NULL, *s = NULL, *e = NULL;
	bat *res = NULL;
	str msg = MAL_SUCCEED;

	(void) cntxt;
	if (pci->argc != 4 || (getArgType(mb, pci, 2) != TYPE_lng && getBatType(getArgType(mb, pci, 2)) != TYPE_lng) ||
		(getArgType(mb, pci, 3) != TYPE_lng && getBatType(getArgType(mb, pci, 3)) != TYPE_lng)) {
		throw(SQL, op, SQLSTATE(42000) "wrong number of arguments to function %s", op);
	}
	tpe = getArgType(mb, pci, 1);

	if (isaBatType(tpe))
		tpe = getBatType(tpe);
	if (isaBatType(getArgType(mb, pci, 1))) {
		res = getArgReference_bat(stk, pci, 0);

		if (!(b = BATdescriptor(*getArgReference_bat(stk, pci, 1)))) {
			msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}
		if (!(r = COLnew(b->hseqbase, b->ttype, BATcount(b), TRANSIENT))) {
			msg = createException(MAL, op, SQLSTATE(HY013) MAL_MALLOC_FAIL);
			goto bailout;
		}
		if (!(s = BATdescriptor(*getArgReference_bat(stk, pci, 2)))) {
			msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}
		if (!(e = BATdescriptor(*getArgReference_bat(stk, pci, 3)))) {
			msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}

		if (func(r, b, s, e, tpe) != GDK_SUCCEED)
			msg = createException(SQL, op, GDK_EXCEPTION);
	} else {
		ValRecord *res = &(stk)->stk[(pci)->argv[0]];
		ValRecord *in = &(stk)->stk[(pci)->argv[1]];

		if (!VALcopy(res, in))
			msg = createException(SQL, op, SQLSTATE(HY013) MAL_MALLOC_FAIL);
	}

bailout:
	unfix_inputs(3, b, s, e);
	finalize_output(res, r, msg);
	return MAL_SUCCEED;
}

str
SQLfirst_value(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_limit_value(cntxt, mb, stk, pci, "sql.first_value", GDKanalyticalfirst);
}

str
SQLlast_value(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_limit_value(cntxt, mb, stk, pci, "sql.last_value", GDKanalyticallast);
}

#define NTH_VALUE_SINGLE_IMP(TPE) \
	do { \
		TPE val = *(TPE*) VALget(nth); \
		if (!VALisnil(nth) && val < 1) \
			throw(SQL, "sql.nth_value", SQLSTATE(42000) "nth_value must be greater than zero"); \
		if (VALisnil(nth) || val > 1) { \
			ValRecord def = (ValRecord) {.vtype = TYPE_void,}; \
			if (!VALinit(&def, tp1, ATOMnilptr(tp1)) || !VALcopy(res, &def)) { \
				VALclear(&def); \
				throw(SQL, "sql.ntile", SQLSTATE(HY013) MAL_MALLOC_FAIL); \
			} \
			VALclear(&def); \
		} else { \
			if (!VALcopy(res, in)) \
				throw(SQL, "sql.nth_value", SQLSTATE(HY013) MAL_MALLOC_FAIL); \
		} \
	} while(0)

str
SQLnth_value(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	BAT *r = NULL, *b = NULL, *l = NULL, *s = NULL, *e = NULL;
	int tpe;
	bat *res = NULL;
	str msg = MAL_SUCCEED;
	bool is_a_bat;

	(void) cntxt;
	if (pci->argc != 5 || (getArgType(mb, pci, 3) != TYPE_lng && getBatType(getArgType(mb, pci, 3)) != TYPE_lng) ||
		(getArgType(mb, pci, 4) != TYPE_lng && getBatType(getArgType(mb, pci, 4)) != TYPE_lng)) {
		throw(SQL, "sql.nth_value", SQLSTATE(42000) "nth_value(:any_1,:number,:lng,:lng)");
	}

	tpe = getArgType(mb, pci, 1);
	is_a_bat = isaBatType(getArgType(mb, pci, 2));

	if (isaBatType(tpe)) {
		lng *nth = NULL;
		res = getArgReference_bat(stk, pci, 0);

		if (!(b = BATdescriptor(*getArgReference_bat(stk, pci, 1)))) {
			msg = createException(SQL, "sql.nth_value", SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}
		tpe = getBatType(tpe);
		if (b && !(r = COLnew(b->hseqbase, tpe, BATcount(b), TRANSIENT))) {
			msg = createException(SQL, "sql.nth_value", SQLSTATE(HY013) MAL_MALLOC_FAIL);
			goto bailout;
		}
		if (is_a_bat) {
			if (!(l = BATdescriptor(*getArgReference_bat(stk, pci, 2)))) {
				msg = createException(SQL, "sql.nth_value", SQLSTATE(HY005) "Cannot access column descriptor");
				goto bailout;
			}
		} else {
			nth = getArgReference_lng(stk, pci, 2);
		}
		if (!(s = BATdescriptor(*getArgReference_bat(stk, pci, 3)))) {
			msg = createException(SQL, "sql.nth_value", SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}
		if (!(e = BATdescriptor(*getArgReference_bat(stk, pci, 4)))) {
			msg = createException(SQL, "sql.nth_value", SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}

		if (GDKanalyticalnthvalue(r, b, s, e, l, nth, tpe) != GDK_SUCCEED)
			msg = createException(SQL, "sql.nth_value", GDK_EXCEPTION);
	} else {
		ValRecord *res = &(stk)->stk[(pci)->argv[0]];
		ValRecord *in = &(stk)->stk[(pci)->argv[1]];
		lng nth = 0;

		if (getArgType(mb, pci, 2) != TYPE_lng) {
			msg = createException(SQL, "sql.nth_value", SQLSTATE(42000) "nth_value offset not available for type %s", ATOMname(getArgType(mb, pci, 2)));
			goto bailout;
		}
		nth = *getArgReference_lng(stk, pci, 2);
		if (!is_lng_nil(nth) && nth < 1) {
			msg = createException(SQL, "sql.nth_value", SQLSTATE(42000) "nth_value must be greater than zero");
			goto bailout;
		}
		if (is_lng_nil(nth) || nth > 1) {
			ValRecord def = (ValRecord) {.vtype = TYPE_void,};
			if (!VALinit(&def, tpe, ATOMnilptr(tpe)) || !VALcopy(res, &def)) {
				VALclear(&def);
				msg = createException(SQL, "sql.ntile", SQLSTATE(HY013) MAL_MALLOC_FAIL);
				goto bailout;
			}
			VALclear(&def);
		} else {
			if (!VALcopy(res, in))
				msg = createException(SQL, "sql.nth_value", SQLSTATE(HY013) MAL_MALLOC_FAIL);
		}
	}

bailout:
	unfix_inputs(4, b, l, s, e);
	finalize_output(res, r, msg);
	return msg;
}

#define CHECK_L_VALUE(TPE) \
	do { \
		TPE rval; \
		if (tp2_is_a_bat) { \
			if (!(l = BATdescriptor(*getArgReference_bat(stk, pci, 2)))) { \
				msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor"); \
				goto bailout; \
			} \
			rval = ((TPE*)Tloc(l, 0))[0]; \
		} else { \
			rval = *getArgReference_##TPE(stk, pci, 2); \
		} \
		if (!is_##TPE##_nil(rval) && rval < 0) { \
			gdk_call = dual; \
			rval *= -1; \
		} \
		l_value = is_##TPE##_nil(rval) ? BUN_NONE : (BUN)rval; \
	} while(0)

static str
do_lead_lag(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci, const char *op, const char* desc,
			gdk_return (*func)(BAT *, BAT *, BAT *, BUN, const void* restrict, int),
			gdk_return (*dual)(BAT *, BAT *, BAT *, BUN, const void* restrict, int))
{
	int tp1, tp2, tp3, base = 2;
	BUN l_value = 1;
	const void *restrict default_value;
	gdk_return (*gdk_call)(BAT *, BAT *, BAT *, BUN, const void* restrict, int) = func;
	BAT *b = NULL, *l = NULL, *d = NULL, *p = NULL, *r = NULL;
	bool tp2_is_a_bat;
	str msg = MAL_SUCCEED;

	(void)cntxt;
	if (pci->argc < 4 || pci->argc > 6)
		throw(SQL, op, SQLSTATE(42000) "%s called with invalid number of arguments", desc);

	tp1 = getArgType(mb, pci, 1);

	if (pci->argc > 4) { //contains (lag or lead) value;
		tp2 = getArgType(mb, pci, 2);
		tp2_is_a_bat = isaBatType(tp2);
		if (tp2_is_a_bat)
			tp2 = getBatType(tp2);

		switch (tp2) {
			case TYPE_bte:
				CHECK_L_VALUE(bte);
				break;
			case TYPE_sht:
				CHECK_L_VALUE(sht);
				break;
			case TYPE_int:
				CHECK_L_VALUE(int);
				break;
			case TYPE_lng:
				CHECK_L_VALUE(lng);
				break;
#ifdef HAVE_HGE
			case TYPE_hge:
				CHECK_L_VALUE(hge);
				break;
#endif
			default:
				throw(SQL, op, SQLSTATE(42000) "%s value not available for %s", desc, ATOMname(tp2));
		}
		base = 3;
	}

	if (pci->argc > 5) { //contains default value;
		tp3 = getArgType(mb, pci, 3);
		if (isaBatType(tp3)) {
			BATiter bpi;

			tp3 = getBatType(tp3);
			if (!(d = BATdescriptor(*getArgReference_bat(stk, pci, 3)))) {
				msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
				goto bailout;
			}
			bpi = bat_iterator(d);
			default_value = BUNtail(bpi, 0);
		} else {
			ValRecord *in = &(stk)->stk[(pci)->argv[3]];
			default_value = VALget(in);
		}
		base = 4;
	} else {
		int tpe = tp1;
		if (isaBatType(tpe))
			tpe = getBatType(tp1);
		default_value = ATOMnilptr(tpe);
	}

	assert(default_value); //default value must be set

	if (isaBatType(tp1)) {
		bat *res = getArgReference_bat(stk, pci, 0);
		b = BATdescriptor(*getArgReference_bat(stk, pci, 1));
		if (!b) {
			msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}
		gdk_return gdk_code;

		tp1 = getBatType(tp1);
		voidresultBAT(r, tp1, BATcount(b), b, op);
		if (isaBatType(getArgType(mb, pci, base))) {
			p = BATdescriptor(*getArgReference_bat(stk, pci, base));
			if (!p) {
				msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
				goto bailout;
			}
		}

		gdk_code = gdk_call(r, b, p, l_value, default_value, tp1);

		if (gdk_code == GDK_SUCCEED)
			BBPkeepref(*res = r->batCacheid);
		else {
			msg = createException(SQL, op, GDK_EXCEPTION);
			goto bailout;
		}
	} else {
		ValRecord *res = &(stk)->stk[(pci)->argv[0]];
		ValRecord *in = &(stk)->stk[(pci)->argv[1]];

		if (l_value == 0) {
			if (!VALcopy(res, in))
				msg = createException(SQL, op, SQLSTATE(HY013) MAL_MALLOC_FAIL);
		} else {
			ValRecord def = (ValRecord) {.vtype = TYPE_void,};

			if (!VALinit(&def, tp1, default_value) || !VALcopy(res, &def))
				msg = createException(SQL, op, SQLSTATE(HY013) MAL_MALLOC_FAIL);
			VALclear(&def);
		}
	}

bailout:
	if (b) BBPunfix(b->batCacheid);
	if (p) BBPunfix(p->batCacheid);
	if (l) BBPunfix(l->batCacheid);
	if (d) BBPunfix(d->batCacheid);
	if (msg && r)
		BBPreclaim(r);
	return msg;
}

str
SQLlag(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_lead_lag(cntxt, mb, stk, pci, "sql.lag", "lag", GDKanalyticallag, GDKanalyticallead);
}

str
SQLlead(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_lead_lag(cntxt, mb, stk, pci, "sql.lead", "lead", GDKanalyticallead, GDKanalyticallag);
}

str
SQLmin_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return SQLanalytical_func(cntxt, mb, stk, pci, false, 4, "sql.min", GDKanalyticalmin);
}

str
SQLmin(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return SQLanalytical_func(cntxt, mb, stk, pci, true, 6, "sql.min", GDKanalyticalmin);
}

str
SQLmax_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return SQLanalytical_func(cntxt, mb, stk, pci, false, 4, "sql.max", GDKanalyticalmax);
}

str
SQLmax(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return SQLanalytical_func(cntxt, mb, stk, pci, true, 6, "sql.max", GDKanalyticalmax);
}

static str
do_count(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci, bool has_bounds, int max_arg)
{
	BAT *r = NULL, *b = NULL, *p = NULL, *o = NULL, *s = NULL, *e = NULL;
	int tpe, frame_type;
	bit ignore_nils;
	bat *res = NULL;
	str msg = MAL_SUCCEED;

	(void) cntxt;
	if ((has_bounds && pci->argc != max_arg && pci->argc != max_arg - 1) ||
		(!has_bounds && pci->argc != max_arg && pci->argc != max_arg - 1 && pci->argc != max_arg + 1))
		throw(SQL, "sql.count", SQLSTATE(42000) "wrong number of arguments to function count");
	tpe = getArgType(mb, pci, 1);
	ignore_nils = *getArgReference_bit(stk, pci, 2);
	frame_type = *getArgReference_int(stk, pci, 3);

	if (isaBatType(tpe))
		tpe = getBatType(tpe);
	if (isaBatType(getArgType(mb, pci, 1)) && (!(b = BATdescriptor(*getArgReference_bat(stk, pci, 1))))) {
		msg = createException(SQL, "sql.count", SQLSTATE(HY005) "Cannot access column descriptor");
		goto bailout;
	}
	if (b && !(r = COLnew(b->hseqbase, TYPE_lng, BATcount(b), TRANSIENT))) {
		msg = createException(SQL, "sql.count", SQLSTATE(HY013) MAL_MALLOC_FAIL);
		goto bailout;
	}
	if (has_bounds) {
		if (isaBatType(getArgType(mb, pci, 4)) && !(s = BATdescriptor(*getArgReference_bat(stk, pci, 4)))) {
			msg = createException(SQL, "sql.count", SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}
		if (isaBatType(getArgType(mb, pci, 5)) && !(e = BATdescriptor(*getArgReference_bat(stk, pci, 5)))) {
			msg = createException(SQL, "sql.count", SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}
	} else if (frame_type == 3 || frame_type == 4) {
		if (isaBatType(getArgType(mb, pci, 4)) && !(o = BATdescriptor(*getArgReference_bat(stk, pci, 4)))) {
			msg = createException(SQL, "sql.count", SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}
		max_arg++;
	}
	if (pci->argc == max_arg && isaBatType(getArgType(mb, pci, max_arg - 1)) &&
		!(p = BATdescriptor(*getArgReference_bat(stk, pci, max_arg - 1)))) {
		msg = createException(SQL, "sql.count", SQLSTATE(HY005) "Cannot access column descriptor");
		goto bailout;
	}

	if (b) {
		res = getArgReference_bat(stk, pci, 0);

		if (GDKanalyticalcount(r, p, o, b, s, e, ignore_nils, tpe, frame_type) != GDK_SUCCEED)
			msg = createException(SQL, "sql.count", GDK_EXCEPTION);
	} else {
		lng *res = getArgReference_lng(stk, pci, 0);
		ValRecord *in = &(stk)->stk[(pci)->argv[1]];

		*res = (VALisnil(in) && ignore_nils) ? 0 : 1;
	}

bailout:
	unfix_inputs(5, b, p, o, s, e);
	finalize_output(res, r, msg);
	return msg;
}

str
SQLcount_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_count(cntxt, mb, stk, pci, false, 5);
}

str
SQLcount(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_count(cntxt, mb, stk, pci, true, 7);
}

static str
do_analytical_sumprod(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci, const char *op,
					  gdk_return (*func)(BAT *, BAT *, BAT *, BAT *, BAT *, BAT *, int, int, int), bool has_bounds, int max_arg)
{
	BAT *r = NULL, *b = NULL, *p = NULL, *o = NULL, *s = NULL, *e = NULL;
	int tp1, tp2, frame_type;
	str msg = MAL_SUCCEED;
	bat *res = NULL;

	(void) cntxt;
	if ((has_bounds && pci->argc != max_arg && pci->argc != max_arg - 1) ||
		(!has_bounds && pci->argc != max_arg && pci->argc != max_arg - 1 && pci->argc != max_arg + 1))
		throw(SQL, op, SQLSTATE(42000) "wrong number of arguments to function %s", op);
	tp1 = getArgType(mb, pci, 1);
	frame_type = *getArgReference_int(stk, pci, 2);

	if (isaBatType(tp1)) {
		tp1 = getBatType(tp1);
		if (!(b = BATdescriptor(*getArgReference_bat(stk, pci, 1)))) {
			msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}
	}
	switch (tp1) {
		case TYPE_bte:
		case TYPE_sht:
		case TYPE_int:
		case TYPE_lng:
#ifdef HAVE_HGE
		case TYPE_hge:
			tp2 = TYPE_hge;
#else
			tp2 = TYPE_lng;
#endif
			break;
		case TYPE_flt:
			tp2 = TYPE_flt;
			break;
		case TYPE_dbl:
			tp2 = TYPE_dbl;
			break;
		default: {
			msg = createException(SQL, op, SQLSTATE(42000) "%s not available for %s", op, ATOMname(tp1));
			goto bailout;
		}
	}
	if (b) {
		res = getArgReference_bat(stk, pci, 0);
		if (!(r = COLnew(b->hseqbase, tp2, BATcount(b), TRANSIENT))) {
			msg = createException(SQL, op, SQLSTATE(HY013) MAL_MALLOC_FAIL);
			goto bailout;
		}
		if (has_bounds) {
			if (isaBatType(getArgType(mb, pci, 3)) && !(s = BATdescriptor(*getArgReference_bat(stk, pci, 3)))) {
				msg = createException(SQL, "sql.count", SQLSTATE(HY005) "Cannot access column descriptor");
				goto bailout;
			}
			if (isaBatType(getArgType(mb, pci, 4)) && !(e = BATdescriptor(*getArgReference_bat(stk, pci, 4)))) {
				msg = createException(SQL, "sql.count", SQLSTATE(HY005) "Cannot access column descriptor");
				goto bailout;
			}
		} else if (frame_type == 3 || frame_type == 4) {
			if (isaBatType(getArgType(mb, pci, 3)) && !(o = BATdescriptor(*getArgReference_bat(stk, pci, 3)))) {
				msg = createException(SQL, "sql.count", SQLSTATE(HY005) "Cannot access column descriptor");
				goto bailout;
			}
			max_arg++;
		}
		if (pci->argc == max_arg && isaBatType(getArgType(mb, pci, max_arg - 1)) &&
			!(p = BATdescriptor(*getArgReference_bat(stk, pci, max_arg - 1)))) {
			msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}

		if (func(r, p, o, b, s, e, tp1, tp2, frame_type) != GDK_SUCCEED)
			msg = createException(SQL, op, GDK_EXCEPTION);
	} else {
		/* the pointers here will always point from bte to dbl, so no strings are handled here */
		ptr res = getArgReference(stk, pci, 0);
		ptr in = getArgReference(stk, pci, 1);
		int scale = 0;

		switch (tp1) {
#ifdef HAVE_HGE
			case TYPE_bte:
				msg = bte_dec2_hge((hge*)res, &scale, (bte*)in);
				break;
			case TYPE_sht:
				msg = sht_dec2_hge((hge*)res, &scale, (sht*)in);
				break;
			case TYPE_int:
				msg = int_dec2_hge((hge*)res, &scale, (int*)in);
				break;
			case TYPE_lng:
				msg = lng_dec2_hge((hge*)res, &scale, (lng*)in);
				break;
			case TYPE_hge:
				*(hge*)res = *((hge*)in);
				break;
#else
			case TYPE_bte:
				msg = bte_dec2_lng((lng*)res, &scale, (bte*)in);
				break;
			case TYPE_sht:
				msg = sht_dec2_lng((lng*)res, &scale, (sht*)in);
				break;
			case TYPE_int:
				msg = int_dec2_lng((lng*)res, &scale, (int*)in);
				break;
			case TYPE_lng:
				*(lng*)res = *((lng*)in);
				break;
#endif
			case TYPE_flt: {
				flt fp = *((flt*)in);
				*(dbl*)res = is_flt_nil(fp) ? dbl_nil : (dbl) fp;
			} break;
			case TYPE_dbl:
				*(dbl*)res = *((dbl*)in);
				break;
			default:
				msg = createException(SQL, op, SQLSTATE(42000) "%s not available for %s", op, ATOMname(tp1));
		}
	}

bailout:
	unfix_inputs(5, b, p, o, s, e);
	finalize_output(res, r, msg);
	return msg;
}

str
SQLsum_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_analytical_sumprod(cntxt, mb, stk, pci, "sql.sum", GDKanalyticalsum, false, 4);
}

str
SQLsum(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_analytical_sumprod(cntxt, mb, stk, pci, "sql.sum", GDKanalyticalsum, true, 6);
}

str
SQLprod_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_analytical_sumprod(cntxt, mb, stk, pci, "sql.prod", GDKanalyticalprod, false, 4);
}

str
SQLprod(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_analytical_sumprod(cntxt, mb, stk, pci, "sql.prod", GDKanalyticalprod, true, 6);
}

static str
do_avg(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci, bool has_bounds, int max_arg)
{
	int tpe = getArgType(mb, pci, 1), frame_type;
	BAT *r = NULL, *b = NULL, *p = NULL, *o = NULL, *s = NULL, *e = NULL;
	str msg = SQLanalytics_args(&r, &b, &frame_type, &p, &o, &s, &e, cntxt, mb, stk, pci, TYPE_dbl, has_bounds, max_arg, "sql.avg");
	bat *res = NULL;

	if (msg)
		goto bailout;
	if (isaBatType(tpe))
		tpe = getBatType(tpe);

	if (b) {
		res = getArgReference_bat(stk, pci, 0);

		if (GDKanalyticalavg(r, p, o, b, s, e, tpe, frame_type) != GDK_SUCCEED)
			msg = createException(SQL, "sql.avg", GDK_EXCEPTION);
	} else {
		/* the pointers here will always point from bte to dbl, so no strings are handled here */
		ptr res = getArgReference(stk, pci, 0);
		ptr in = getArgReference(stk, pci, 1);
		int scale = 0;

		switch (tpe) {
			case TYPE_bte:
				msg = bte_dec2_dbl((dbl*)res, &scale, (bte*)in);
				break;
			case TYPE_sht:
				msg = sht_dec2_dbl((dbl*)res, &scale, (sht*)in);
				break;
			case TYPE_int:
				msg = int_dec2_dbl((dbl*)res, &scale, (int*)in);
				break;
			case TYPE_lng:
				msg = lng_dec2_dbl((dbl*)res, &scale, (lng*)in);
				break;
#ifdef HAVE_HGE
			case TYPE_hge:
				msg = hge_dec2_dbl((dbl*)res, &scale, (hge*)in);
				break;
#endif
			case TYPE_flt: {
				flt fp = *((flt*)in);
				*(dbl*)res = is_flt_nil(fp) ? dbl_nil : (dbl) fp;
			} break;
			case TYPE_dbl:
				*(dbl*)res = *((dbl*)in);
				break;
			default:
				msg = createException(SQL, "sql.avg", SQLSTATE(42000) "sql.avg not available for %s to dbl", ATOMname(tpe));
		}
	}

bailout:
	unfix_inputs(5, b, p, o, s, e);
	finalize_output(res, r, msg);
	return msg;
}

str
SQLavg_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_avg(cntxt, mb, stk, pci, false, 4);
}

str
SQLavg(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_avg(cntxt, mb, stk, pci, true, 6);
}

static str
do_avginteger(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci, bool has_bounds, int max_arg)
{
	int tpe = getArgType(mb, pci, 1), frame_type;
	BAT *r = NULL, *b = NULL, *p = NULL, *o = NULL, *s = NULL, *e = NULL;
	str msg = SQLanalytics_args(&r, &b, &frame_type, &p, &o, &s, &e, cntxt, mb, stk, pci, 0, has_bounds, max_arg, "sql.avg");
	bat *res = NULL;

	if (msg)
		goto bailout;
	if (isaBatType(tpe))
		tpe = getBatType(tpe);

	if (b) {
		res = getArgReference_bat(stk, pci, 0);

		if (GDKanalyticalavginteger(r, p, o, b, s, e, tpe, frame_type) != GDK_SUCCEED)
			msg = createException(SQL, "sql.avg", GDK_EXCEPTION);
	} else {
		ValRecord *res = &(stk)->stk[(pci)->argv[0]];
		ValRecord *in = &(stk)->stk[(pci)->argv[1]];

		switch (tpe) {
			case TYPE_bte:
			case TYPE_sht:
			case TYPE_int:
			case TYPE_lng:
#ifdef HAVE_HGE
			case TYPE_hge:
#endif
				if (!VALcopy(res, in))
					msg = createException(SQL, "sql.avg", SQLSTATE(HY013) MAL_MALLOC_FAIL); /* malloc failure should never happen, but let it be here */
				break;
			default:
				msg = createException(SQL, "sql.avg", SQLSTATE(42000) "sql.avg not available for %s to %s", ATOMname(tpe), ATOMname(tpe));
		}
	}

bailout:
	unfix_inputs(5, b, p, o, s, e);
	finalize_output(res, r, msg);
	return msg;
}

str
SQLavginteger_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_avginteger(cntxt, mb, stk, pci, false, 4);
}

str
SQLavginteger(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_avginteger(cntxt, mb, stk, pci, true, 6);
}

static str
do_stddev_and_variance(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci, const char *op,
					   gdk_return (*func)(BAT *, BAT *, BAT *, BAT *, BAT *, BAT *, int, int), bool has_bounds, int max_arg)
{
	int tpe = getArgType(mb, pci, 1), frame_type;
	BAT *r = NULL, *b = NULL, *p = NULL, *o = NULL, *s = NULL, *e = NULL;
	str msg = SQLanalytics_args(&r, &b, &frame_type, &p, &o, &s, &e, cntxt, mb, stk, pci, TYPE_dbl, has_bounds, max_arg, op);
	bat *res = NULL;

	if (msg)
		goto bailout;
	if (isaBatType(tpe))
		tpe = getBatType(tpe);

	if (b) {
		res = getArgReference_bat(stk, pci, 0);

		if (func(r, p, o, b, s, e, tpe, frame_type) != GDK_SUCCEED)
			msg = createException(SQL, op, GDK_EXCEPTION);
	} else {
		dbl *res = getArgReference_dbl(stk, pci, 0);
		ValRecord *input1 = &(stk)->stk[(pci)->argv[1]];

		switch (tpe) {
			case TYPE_bte:
			case TYPE_sht:
			case TYPE_int:
			case TYPE_lng:
#ifdef HAVE_HGE
			case TYPE_hge:
#endif
			case TYPE_flt:
			case TYPE_dbl:
				*res = VALisnil(input1) ? dbl_nil : 0;
				break;
			default:
				msg = createException(SQL, op, SQLSTATE(42000) "%s not available for %s", op, ATOMname(tpe));
		}
	}

bailout:
	unfix_inputs(5, b, p, o, s, e);
	finalize_output(res, r, msg);
	return msg;
}

str
SQLstddev_samp_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_stddev_and_variance(cntxt, mb, stk, pci, "sql.stdev", GDKanalytical_stddev_samp, false, 4);
}

str
SQLstddev_pop_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_stddev_and_variance(cntxt, mb, stk, pci, "sql.stdevp", GDKanalytical_stddev_pop, false, 4);
}

str
SQLvar_samp_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_stddev_and_variance(cntxt, mb, stk, pci, "sql.variance", GDKanalytical_variance_samp, false, 4);
}

str
SQLvar_pop_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_stddev_and_variance(cntxt, mb, stk, pci, "sql.variancep", GDKanalytical_variance_pop, false, 4);
}

str
SQLstddev_samp(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_stddev_and_variance(cntxt, mb, stk, pci, "sql.stdev", GDKanalytical_stddev_samp, true, 6);
}

str
SQLstddev_pop(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_stddev_and_variance(cntxt, mb, stk, pci, "sql.stdevp", GDKanalytical_stddev_pop, true, 6);
}

str
SQLvar_samp(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_stddev_and_variance(cntxt, mb, stk, pci, "sql.variance", GDKanalytical_variance_samp, true, 6);
}

str
SQLvar_pop(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_stddev_and_variance(cntxt, mb, stk, pci, "sql.variancep", GDKanalytical_variance_pop, true, 6);
}

#define COVARIANCE_AND_CORRELATION_ONE_SIDE_UNBOUNDED_TILL_CURRENT_ROW(TPE) \
	do { \
		TPE *restrict bp = (TPE*)Tloc(d, 0); \
		for (; k < i;) { \
			j = k; \
			do {	\
				n += !is_##TPE##_nil(bp[k]);	\
				k++; \
			} while (k < i && !opp[k]);	\
			if (n > minimum) { /* covariance_samp requires at least one value */ \
				rr = val; \
			} else { \
				rr = dbl_nil; \
				has_nils = true; \
			} \
			for (; j < k; j++) \
				rb[j] = rr; \
		} \
		n = 0;	\
		k = i; \
	} while (0)

#define COVARIANCE_AND_CORRELATION_ONE_SIDE_CURRENT_ROW_TILL_UNBOUNDED(TPE) \
	do { \
		TPE *restrict bp = (TPE*)Tloc(d, 0); \
		l = i - 1; \
		for (j = l; ; j--) { \
			n += !is_##TPE##_nil(bp[j]);	\
			if (opp[j] || j == k) {	\
				if (n > minimum) { /* covariance_samp requires at least one value */ \
					rr = val; \
				} else { \
					rr = dbl_nil; \
					has_nils = true; \
				} \
				for (; l >= j; l--) \
					rb[l] = rr; \
				if (j == k)	\
					break;	\
				l = j - 1;	\
			}	\
		}	\
		n = 0;	\
		k = i; \
	} while (0)

#define COVARIANCE_AND_CORRELATION_ONE_SIDE_ALL_ROWS(TPE) \
	do { \
		TPE *restrict bp = (TPE*)Tloc(d, 0); \
		for (; j < i; j++) \
			n += !is_##TPE##_nil(bp[j]);	\
		if (n > minimum) { /* covariance_samp requires at least one value */ \
			rr = val; \
		} else { \
			rr = dbl_nil; \
			has_nils = true; \
		} \
		for (; k < i; k++) \
			rb[k] = rr;	\
		n = 0; \
	} while (0)

#define COVARIANCE_AND_CORRELATION_ONE_SIDE_CURRENT_ROW(TPE) \
	do { \
		TPE *restrict bp = (TPE*)Tloc(d, 0); \
		for (; k < i; k++) { \
			n += !is_##TPE##_nil(bp[k]);	\
			if (n > minimum) { /* covariance_samp requires at least one value */ \
				rb[k] = val; \
			} else { \
				rb[k] = dbl_nil; \
				has_nils = true; \
			} \
			n = 0; \
		}	\
	} while (0)

#define COVARIANCE_AND_CORRELATION_ONE_SIDE_OTHERS(TPE) \
	do { \
		TPE *restrict bp = (TPE*)Tloc(d, 0); \
		for (; k < i; k++) { \
			for (j = start[k] ; j < end[k] ; j++) { \
				if (is_##TPE##_nil(bp[j])) \
					continue; \
				n++; \
			} \
			if (n > minimum) { /* covariance_samp requires at least one value */ \
				rb[k] = val; \
			} else { \
				rb[k] = dbl_nil; \
				has_nils = true; \
			} \
			n = 0; \
		} \
	} while (0)

#define COVARIANCE_AND_CORRELATION_ONE_SIDE_PARTITIONS(TPE, IMP)		\
	do {						\
		if (p) {					\
			for (; i < cnt; i++) {		\
				if (np[i]) 			\
					IMP(TPE);	\
			}						\
		}	\
		i = cnt;			\
		IMP(TPE);	\
	} while (0)

#ifdef HAVE_HGE
#define COVARIANCE_AND_CORRELATION_ONE_SIDE_LIMIT(IMP) \
	case TYPE_hge: \
		COVARIANCE_AND_CORRELATION_ONE_SIDE_PARTITIONS(hge, COVARIANCE_AND_CORRELATION_ONE_SIDE_##IMP); \
	break;
#else
#define COVARIANCE_AND_CORRELATION_ONE_SIDE_LIMIT(IMP)
#endif

#define COVARIANCE_AND_CORRELATION_ONE_SIDE_BRANCHES(IMP)		\
	do { \
		switch (tp1) {	\
		case TYPE_bte:	\
			COVARIANCE_AND_CORRELATION_ONE_SIDE_PARTITIONS(bte, COVARIANCE_AND_CORRELATION_ONE_SIDE_##IMP);	\
			break;	\
		case TYPE_sht:	\
			COVARIANCE_AND_CORRELATION_ONE_SIDE_PARTITIONS(sht, COVARIANCE_AND_CORRELATION_ONE_SIDE_##IMP);	\
			break;	\
		case TYPE_int:	\
			COVARIANCE_AND_CORRELATION_ONE_SIDE_PARTITIONS(int, COVARIANCE_AND_CORRELATION_ONE_SIDE_##IMP);	\
			break;	\
		case TYPE_lng:	\
			COVARIANCE_AND_CORRELATION_ONE_SIDE_PARTITIONS(lng, COVARIANCE_AND_CORRELATION_ONE_SIDE_##IMP);	\
			break;	\
		case TYPE_flt:	\
			COVARIANCE_AND_CORRELATION_ONE_SIDE_PARTITIONS(flt, COVARIANCE_AND_CORRELATION_ONE_SIDE_##IMP);	\
			break;	\
		case TYPE_dbl:	\
			COVARIANCE_AND_CORRELATION_ONE_SIDE_PARTITIONS(dbl, COVARIANCE_AND_CORRELATION_ONE_SIDE_##IMP);	\
			break;	\
		COVARIANCE_AND_CORRELATION_ONE_SIDE_LIMIT(IMP)	\
		default: {	\
			msg = createException(SQL, op, SQLSTATE(42000) "%s not available for %s", op, ATOMname(tp1)); \
			goto bailout; \
		} \
		}	\
	} while (0)

static str
do_covariance_and_correlation(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci, const char *op,
							  gdk_return (*func)(BAT *, BAT *, BAT *, BAT *, BAT *, BAT *, BAT *, int, int), lng minimum, dbl defaultv,
							  dbl single_case, bool has_bounds, int max_arg)
{
	BAT *r = NULL, *b = NULL, *c = NULL, *p = NULL, *o = NULL, *s = NULL, *e = NULL;
	int tp1, tp2, frame_type;
	bool is_a_bat1, is_a_bat2;
	str msg = MAL_SUCCEED;
	bat *res = NULL;

	(void)cntxt;
	if ((has_bounds && pci->argc != max_arg && pci->argc != max_arg - 1) ||
		(!has_bounds && pci->argc != max_arg && pci->argc != max_arg - 1 && pci->argc != max_arg + 1))
		throw(SQL, op, SQLSTATE(42000) "wrong number of arguments to function %s", op);

	tp1 = getArgType(mb, pci, 1);
	tp2 = getArgType(mb, pci, 2);
	frame_type = *getArgReference_int(stk, pci, 3);
	is_a_bat1 = isaBatType(tp1);
	is_a_bat2 = isaBatType(tp2);

	if (is_a_bat1)
		tp1 = getBatType(tp1);
	if (is_a_bat2)
		tp2 = getBatType(tp2);
	if (tp1 != tp2)
		throw(SQL, op, SQLSTATE(42000) "The input arguments for %s must be from the same type", op);

	if (is_a_bat1 || is_a_bat2) {
		res = getArgReference_bat(stk, pci, 0);

		if (is_a_bat1 && !(b = BATdescriptor(*getArgReference_bat(stk, pci, 1)))) {
			msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}
		if (is_a_bat2 && !(c = BATdescriptor(*getArgReference_bat(stk, pci, 2)))) {
			msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}
		if (!(r = COLnew(b->hseqbase, TYPE_dbl, BATcount(b), TRANSIENT))) {
			msg = createException(SQL, op, SQLSTATE(HY013) MAL_MALLOC_FAIL);
			goto bailout;
		}

		if (has_bounds) {
			if (isaBatType(getArgType(mb, pci, 4)) && !(s = BATdescriptor(*getArgReference_bat(stk, pci, 4)))) {
				msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
				goto bailout;
			}
			if (isaBatType(getArgType(mb, pci, 5)) && !(e = BATdescriptor(*getArgReference_bat(stk, pci, 5)))) {
				msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
				goto bailout;
			}
		} else if (frame_type == 3 || frame_type == 4) {
			if (isaBatType(getArgType(mb, pci, 4)) && !(o = BATdescriptor(*getArgReference_bat(stk, pci, 4)))) {
				msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
				goto bailout;
			}
			max_arg++;
		}
		if (pci->argc == max_arg && isaBatType(getArgType(mb, pci, max_arg - 1)) &&
			!(p = BATdescriptor(*getArgReference_bat(stk, pci, max_arg - 1)))) {
			msg = createException(SQL, op, SQLSTATE(HY005) "Cannot access column descriptor");
			goto bailout;
		}

		if (is_a_bat1 && is_a_bat2) {
			if (func(r, p, o, b, c, s, e, tp1, frame_type) != GDK_SUCCEED)
				msg = createException(SQL, op, GDK_EXCEPTION);
		} else {
			/* corner case, second column is a constant, calculate it this way... */
			BAT *d = b ? b : c;
			ValRecord *input2 = &(stk)->stk[(pci)->argv[b ? 2 : 1]];
			lng i = 0, j = 0, k = 0, l = 0, cnt = (lng) BATcount(d), n = 0;
			lng *restrict start = s ? (lng*)Tloc(s, 0) : NULL, *restrict end = e ? (lng*)Tloc(e, 0) : NULL;
			bit *np = p ? Tloc(p, 0) : NULL, *opp = o ? Tloc(o, 0) : NULL;
			dbl *restrict rb = (dbl *) Tloc(r, 0), val = VALisnil(input2) ? dbl_nil : defaultv, rr;
			bool has_nils = is_dbl_nil(val);

			switch (frame_type) {
			case 3: /* unbounded until current row */	{
				COVARIANCE_AND_CORRELATION_ONE_SIDE_BRANCHES(UNBOUNDED_TILL_CURRENT_ROW);
			} break;
			case 4: /* current row until unbounded */	{
				COVARIANCE_AND_CORRELATION_ONE_SIDE_BRANCHES(CURRENT_ROW_TILL_UNBOUNDED);
			} break;
			case 5: /* all rows */	{
				COVARIANCE_AND_CORRELATION_ONE_SIDE_BRANCHES(ALL_ROWS);
			} break;
			case 6: /* current row */ {
				COVARIANCE_AND_CORRELATION_ONE_SIDE_BRANCHES(CURRENT_ROW);
			} break;
			default: {
				COVARIANCE_AND_CORRELATION_ONE_SIDE_BRANCHES(OTHERS);
			}
			}

			BATsetcount(r, (BUN) cnt);
			r->tnonil = !has_nils;
			r->tnil = has_nils;
		}
	} else {
		dbl *res = getArgReference_dbl(stk, pci, 0);
		ValRecord *input1 = &(stk)->stk[(pci)->argv[1]];
		ValRecord *input2 = &(stk)->stk[(pci)->argv[2]];

		switch (tp1) {
			case TYPE_bte:
			case TYPE_sht:
			case TYPE_int:
			case TYPE_lng:
#ifdef HAVE_HGE
			case TYPE_hge:
#endif
			case TYPE_flt:
			case TYPE_dbl:
				*res = (VALisnil(input1) || VALisnil(input2)) ? dbl_nil : single_case;
				break;
			default:
				msg = createException(SQL, op, SQLSTATE(42000) "%s not available for %s", op, ATOMname(tp1));
		}
	}

bailout:
	unfix_inputs(6, b, c, p, o, s, e);
	finalize_output(res, r, msg);
	return msg;
}

str
SQLcovar_samp_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_covariance_and_correlation(cntxt, mb, stk, pci, "sql.covariance", GDKanalytical_covariance_samp, 1, 0.0f, dbl_nil, false, 5);
}

str
SQLcovar_pop_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_covariance_and_correlation(cntxt, mb, stk, pci, "sql.covariancep", GDKanalytical_covariance_pop, 0, 0.0f, 0.0f, false, 5);
}

str
SQLcorr_global(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_covariance_and_correlation(cntxt, mb, stk, pci, "sql.corr", GDKanalytical_correlation, 0, dbl_nil, dbl_nil, false, 5);
}

str
SQLcovar_samp(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_covariance_and_correlation(cntxt, mb, stk, pci, "sql.covariance", GDKanalytical_covariance_samp, 1, 0.0f, dbl_nil, true, 7);
}

str
SQLcovar_pop(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_covariance_and_correlation(cntxt, mb, stk, pci, "sql.covariancep", GDKanalytical_covariance_pop, 0, 0.0f, 0.0f, true, 7);
}

str
SQLcorr(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	return do_covariance_and_correlation(cntxt, mb, stk, pci, "sql.corr", GDKanalytical_correlation, 0, dbl_nil, dbl_nil, true, 7);
}

str
SQLstrgroup_concat(Client cntxt, MalBlkPtr mb, MalStkPtr stk, InstrPtr pci)
{
	BAT *r = NULL, *b = NULL, *sep = NULL, *s = NULL, *e = NULL;
	int parameters = 2;
	gdk_return gdk_res = GDK_SUCCEED;
	str msg = MAL_SUCCEED, separator = NULL;

	(void)cntxt;
	if (pci->argc != 4 && pci->argc != 5)
		throw(SQL, "sql.strgroup_concat", SQLSTATE(42000) "Requires 4 or 5 parameters");

	if (isaBatType(getArgType(mb, pci, 1))) {
		bat *res = getArgReference_bat(stk, pci, 0);

		b = BATdescriptor(*getArgReference_bat(stk, pci, 1));
		if (!b)
			throw(SQL, "sql.strgroup_concat", SQLSTATE(HY005) "Cannot access column descriptor");
		voidresultBAT(r, TYPE_str, BATcount(b), b, "sql.strgroup_concat");

		if (pci->argc == 5) {
			if (isaBatType(getArgType(mb, pci, 2))) {
				sep = BATdescriptor(*getArgReference_bat(stk, pci, 2));
				if (!sep) {
					BBPunfix(b->batCacheid);
					BBPunfix(r->batCacheid);
					throw(SQL, "sql.strgroup_concat", SQLSTATE(HY005) "Cannot access column descriptor");
				}
			} else
				separator = *getArgReference_str(stk, pci, 2);
			parameters++;
		} else
			separator = ",";

		s = BATdescriptor(*getArgReference_bat(stk, pci, parameters));
		if (!s) {
			BBPunfix(b->batCacheid);
			BBPunfix(r->batCacheid);
			if (sep) BBPunfix(sep->batCacheid);
			throw(SQL, "sql.strgroup_concat", SQLSTATE(HY005) "Cannot access column descriptor");
		}
		e = BATdescriptor(*getArgReference_bat(stk, pci, parameters + 1));
		if (!e) {
			BBPunfix(b->batCacheid);
			BBPunfix(r->batCacheid);
			if (sep) BBPunfix(sep->batCacheid);
			BBPunfix(s->batCacheid);
			throw(SQL, "sql.strgroup_concat", SQLSTATE(HY005) "Cannot access column descriptor");
		}

		assert((separator && !sep) || (!separator && sep)); /* only one of them must be set */
		gdk_res = GDKanalytical_str_group_concat(r, b, sep, s, e, separator);

		BBPunfix(b->batCacheid);
		BBPunfix(s->batCacheid);
		BBPunfix(e->batCacheid);
		if (sep) BBPunfix(sep->batCacheid);
		if (gdk_res == GDK_SUCCEED)
			BBPkeepref(*res = r->batCacheid);
		else
			throw(SQL, "sql.strgroup_concat", GDK_EXCEPTION);
	} else {
		str *res = getArgReference_str(stk, pci, 0);
		str in = *getArgReference_str(stk, pci, 1);

		if (strNil(in)) {
			*res = GDKstrdup(str_nil);
		} else if (pci->argc == 5) {
			str sep = *getArgReference_str(stk, pci, 2);
			size_t l1 = strlen(in), l2 = strNil(sep) ? 0 : strlen(sep);

			if ((*res = GDKmalloc(l1+l2+1))) {
				if (l1)
					memcpy(*res, in, l1);
				if (l2)
					memcpy((*res)+l1, sep, l2);
				(*res)[l1+l2] = '\0';
			}
		} else {
			*res = GDKstrdup(in);
		}
		if (!*res)
			throw(SQL, "sql.strgroup_concat", SQLSTATE(HY013) MAL_MALLOC_FAIL);
	}
	return msg;
}
