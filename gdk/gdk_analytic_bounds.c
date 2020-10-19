/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2020 MonetDB B.V.
 */

#include "monetdb_config.h"
#include "gdk.h"
#include "gdk_analytic.h"
#include "gdk_calc_private.h"

#define ANALYTICAL_WINDOW_BOUNDS_ROWS_PRECEDING(LIMIT)			\
	do {								\
		lng calc1, calc2;					\
		j = k;							\
		for (; k < i; k++) {				\
			lng rlimit = LIMIT;				\
			SUB_WITH_CHECK(k, rlimit, lng, calc1, GDK_lng_max, goto calc_overflow); \
			ADD_WITH_CHECK(calc1, !first_half, lng, calc2, GDK_lng_max, goto calc_overflow); \
			rb[k] = MAX(calc2, j);				\
		}							\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_ROWS_FOLLOWING(LIMIT)			\
	do {								\
		lng calc1, calc2;					\
		for (; k < i; k++) {				\
			lng rlimit = LIMIT;				\
			ADD_WITH_CHECK(rlimit, k, lng, calc1, GDK_lng_max, goto calc_overflow); \
			ADD_WITH_CHECK(calc1, !first_half, lng, calc2, GDK_lng_max, goto calc_overflow); \
			rb[k] = MIN(calc2, i);				\
		}							\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(IMP, LIMIT)		\
	do {								\
		if (p) {						\
			for (; i < cnt; i++) {			\
				if (np[i]) 			\
					ANALYTICAL_WINDOW_BOUNDS_ROWS##IMP(LIMIT); \
			}						\
			i = cnt;				\
			ANALYTICAL_WINDOW_BOUNDS_ROWS##IMP(LIMIT);	\
		} else {						\
			i = cnt;					\
			ANALYTICAL_WINDOW_BOUNDS_ROWS##IMP(LIMIT);	\
		}							\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_GROUPS_PRECEDING(LIMIT) \
	do {								\
		lng m = k - 1;						\
		for (; k < i; k++) {		\
			lng rlimit = LIMIT;		\
			for (j = k; ; j--) {		\
				if (j == m) {		\
					j++; \
					break;		\
				} \
				if (bp[j]) {		\
					if (rlimit == 0)		\
						break;		\
					rlimit--;		\
				}				\
			}				\
			rb[k] =j;		\
		}					\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_GROUPS_FOLLOWING(LIMIT) \
	do {								\
		for (; k < i; k++) {		\
			lng rlimit = LIMIT;		\
			for (j = k + 1; j < i; j++) {	\
				if (bp[j]) {		\
					if (rlimit == 0)		\
						break;		\
					rlimit--;		\
				}		\
			}		\
			rb[k] = j;		\
		}		\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(IMP, LIMIT)		\
	do {								\
		if (p) {						\
			for (; i < cnt; i++) {			\
				if (np[i]) 			\
					ANALYTICAL_WINDOW_BOUNDS_GROUPS##IMP(LIMIT); \
			}						\
			i = cnt;				\
			ANALYTICAL_WINDOW_BOUNDS_GROUPS##IMP(LIMIT);	\
		} else {						\
			i = cnt;					\
			ANALYTICAL_WINDOW_BOUNDS_GROUPS##IMP(LIMIT);	\
		}							\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE_PRECEDING(TPE1, LIMIT, TPE2) \
	do {								\
		lng m = k - 1;						\
		TPE1 v, calc;						\
		TPE2 rlimit;						\
		if (b->tnonil) {					\
			for (; k < i; k++) {			\
				rlimit = (TPE2) LIMIT;			\
				v = bp[k];				\
				for (j = k; ; j--) {			\
					if (j == m)			\
						break;			\
					SUB_WITH_CHECK(v, bp[j], TPE1, calc, GDK_##TPE1##_max, goto calc_overflow); \
					if ((TPE2)(ABSOLUTE(calc)) > rlimit) \
						break;			\
				}					\
				rb[k] = ++j;				\
			}						\
		} else {						\
			for (; k < i; k++) {			\
				rlimit = (TPE2) LIMIT;			\
				v = bp[k];				\
				if (is_##TPE1##_nil(v)) {		\
					for (j = k; ; j--) {		\
						if (j == m)		\
							break;		\
						if (!is_##TPE1##_nil(bp[j])) \
							break;		\
					}				\
				} else {				\
					for (j = k; ; j--) {		\
						if (j == m)		\
							break;		\
						if (is_##TPE1##_nil(bp[j])) \
							break;		\
						SUB_WITH_CHECK(v, bp[j], TPE1, calc, GDK_##TPE1##_max, goto calc_overflow); \
						if ((TPE2)(ABSOLUTE(calc)) > rlimit) \
							break;		\
					}				\
				}					\
				rb[k] = ++j;				\
			}						\
		}							\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE_FOLLOWING(TPE1, LIMIT, TPE2) \
	do {								\
		TPE1 v, calc;						\
		TPE2 rlimit;						\
		if (b->tnonil) {					\
			for (; k < i; k++) {			\
				rlimit = (TPE2) LIMIT;			\
				v = bp[k];				\
				for (j = k + 1; j < i; j++) {		\
					SUB_WITH_CHECK(v, bp[j], TPE1, calc, GDK_##TPE1##_max, goto calc_overflow); \
					if ((TPE2)(ABSOLUTE(calc)) > rlimit) \
						break;			\
				}					\
				rb[k] = j;				\
			}						\
		} else {						\
			for (; k < i; k++) {			\
				rlimit = (TPE2) LIMIT;			\
				v = bp[k];				\
				if (is_##TPE1##_nil(v)) {		\
					for (j =k + 1; j < i; j++) {	\
						if (!is_##TPE1##_nil(bp[j])) \
							break;		\
					}				\
				} else {				\
					for (j = k + 1; j < i; j++) {	\
						if (is_##TPE1##_nil(bp[j])) \
							break;		\
						SUB_WITH_CHECK(v, bp[j], TPE1, calc, GDK_##TPE1##_max, goto calc_overflow); \
						if ((TPE2)(ABSOLUTE(calc)) > rlimit) \
							break;		\
					}				\
				}					\
				rb[k] = j;				\
			}						\
		}							\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_CALC_FIXED(TPE1, IMP, LIMIT, TPE2)	\
	do {								\
		TPE1 *restrict bp = (TPE1*)Tloc(b, 0);			\
		if (np) {						\
			for (; i < cnt; i++) {			\
				if (np[i])				\
					IMP(TPE1, LIMIT, TPE2);		\
			}						\
			i = cnt;				\
			IMP(TPE1, LIMIT, TPE2);				\
		} else {						\
			i = cnt;					\
			IMP(TPE1, LIMIT, TPE2);				\
		}							\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_VARSIZED_RANGE_PRECEDING(LIMIT, TPE)	\
	do {								\
		lng m = k - 1;						\
		if (b->tnonil) {					\
			for (; k < i; k++) {			\
				void *v = BUNtail(bpi, (BUN) k);	\
				for (j = k; ; j--) {			\
					void *next;			\
					if (j == m)			\
						break;			\
					next = BUNtail(bpi, (BUN) j);	\
					if (ABSOLUTE((TPE) atomcmp(v, next)) > (TPE) LIMIT) \
						break;			\
				}					\
				rb[k] = ++j;					\
			}						\
		} else {						\
			for (; k < i; k++) {			\
				void *v = BUNtail(bpi, (BUN) k);	\
				if (atomcmp(v, nil) == 0) {		\
					for (j = k; ; j--) {		\
						if (j == m)		\
							break;		\
						if (atomcmp(BUNtail(bpi, (BUN) j), nil) != 0) \
							break;		\
					}				\
				} else {				\
					for (j = k; ; j--) {		\
						void *next;		\
						if (j == m)		\
							break;		\
						next = BUNtail(bpi, (BUN) j); \
						if (atomcmp(next, nil) == 0) \
							break;		\
						if (ABSOLUTE((TPE) atomcmp(v, next)) > (TPE) LIMIT) \
							break;		\
					}				\
				}					\
				rb[k] = ++j;					\
			}						\
		}							\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_VARSIZED_RANGE_FOLLOWING(LIMIT, TPE)	\
	do {								\
		if (b->tnonil) {					\
			for (; k < i; k++) {			\
				void *v = BUNtail(bpi, (BUN) k);	\
				for (j = k + 1; j < i; j++) {		\
					void *next = BUNtail(bpi, (BUN) j); \
					if (ABSOLUTE((TPE) atomcmp(v, next)) > (TPE) LIMIT) \
						break;			\
				}					\
				rb[k] = j;				\
			}						\
		} else {						\
			for (; k < i; k++) {			\
				void *v = BUNtail(bpi, (BUN) k);	\
				if (atomcmp(v, nil) == 0) {		\
					for (j = k + 1; j < i; j++) {	\
						if (atomcmp(BUNtail(bpi, (BUN) j), nil) != 0) \
							break;		\
					}				\
				} else {				\
					for (j = k + 1; j < i; j++) {	\
						void *next = BUNtail(bpi, (BUN) j); \
						if (atomcmp(next, nil) == 0) \
							break;		\
						if (ABSOLUTE((TPE) atomcmp(v, next)) > (TPE) LIMIT) \
							break;		\
					}				\
				}					\
				rb[k] = j;				\
			}						\
		}							\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_NUM(IMP, LIMIT, CAST)	\
	do {								\
		switch (tp1) {						\
		case TYPE_bit:						\
		case TYPE_flt:						\
		case TYPE_dbl:						\
			goto type_not_supported;			\
		case TYPE_bte:						\
			ANALYTICAL_WINDOW_BOUNDS_CALC_FIXED(bte, ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE##IMP, LIMIT, lng); \
			break;						\
		case TYPE_sht:						\
			ANALYTICAL_WINDOW_BOUNDS_CALC_FIXED(sht, ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE##IMP, LIMIT, lng); \
			break;						\
		case TYPE_int:						\
			ANALYTICAL_WINDOW_BOUNDS_CALC_FIXED(int, ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE##IMP, LIMIT, lng); \
			break;						\
		case TYPE_lng:						\
			ANALYTICAL_WINDOW_BOUNDS_CALC_FIXED(lng, ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE##IMP, LIMIT, lng); \
			break;						\
		default: {						\
			if (p) {					\
				np = (bit*)Tloc(p, 0);			\
				for (; i < cnt; i++) {			\
					if (np[i]) 			\
						ANALYTICAL_WINDOW_BOUNDS_VARSIZED_RANGE##IMP(LIMIT, CAST); \
				}					\
				i = cnt;			\
				ANALYTICAL_WINDOW_BOUNDS_VARSIZED_RANGE##IMP(LIMIT, CAST); \
			} else {					\
				i = cnt;				\
				ANALYTICAL_WINDOW_BOUNDS_VARSIZED_RANGE##IMP(LIMIT, CAST); \
			}						\
		}							\
		}							\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_FLT(IMP, LIMIT)		\
	do {								\
		switch (tp1) {						\
			case TYPE_flt:					\
				ANALYTICAL_WINDOW_BOUNDS_CALC_FIXED(flt, ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE##IMP, LIMIT, flt); \
				break;					\
			default:					\
				goto type_not_supported;		\
		}							\
	} while (0)

#define ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_DBL(IMP, LIMIT)		\
	do {								\
		switch (tp1) {						\
			case TYPE_dbl:					\
				ANALYTICAL_WINDOW_BOUNDS_CALC_FIXED(dbl, ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE##IMP, LIMIT, dbl); \
				break;					\
			default:					\
				goto type_not_supported;		\
		}							\
	} while (0)

#ifdef HAVE_HGE
#define ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_HGE(IMP, LIMIT)		\
	do {								\
		switch (tp1) {						\
		case TYPE_bit:						\
		case TYPE_flt:						\
		case TYPE_dbl:						\
			goto type_not_supported;			\
		case TYPE_bte:						\
			ANALYTICAL_WINDOW_BOUNDS_CALC_FIXED(bte, ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE##IMP, LIMIT, hge); \
			break;						\
		case TYPE_sht:						\
			ANALYTICAL_WINDOW_BOUNDS_CALC_FIXED(sht, ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE##IMP, LIMIT, hge); \
			break;						\
		case TYPE_int:						\
			ANALYTICAL_WINDOW_BOUNDS_CALC_FIXED(int, ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE##IMP, LIMIT, hge); \
			break;						\
		case TYPE_lng:						\
			ANALYTICAL_WINDOW_BOUNDS_CALC_FIXED(lng, ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE##IMP, LIMIT, hge); \
			break;						\
		case TYPE_hge:						\
			ANALYTICAL_WINDOW_BOUNDS_CALC_FIXED(hge, ANALYTICAL_WINDOW_BOUNDS_FIXED_RANGE##IMP, LIMIT, hge); \
			break;						\
		default: {						\
			if (p) {					\
				np = (bit*)Tloc(p, 0);		\
				for (; i < cnt; i++) {		\
					if (np[i])			\
						ANALYTICAL_WINDOW_BOUNDS_VARSIZED_RANGE##IMP(LIMIT, hge); \
				}					\
				i = cnt;			\
				ANALYTICAL_WINDOW_BOUNDS_VARSIZED_RANGE##IMP(LIMIT, hge); \
			} else {					\
				i = cnt;				\
				ANALYTICAL_WINDOW_BOUNDS_VARSIZED_RANGE##IMP(LIMIT, hge); \
			}						\
		}							\
		}							\
	} while (0)
#endif

static gdk_return
GDKanalyticalrowbounds(BAT *r, BAT *b, BAT *p, BAT *l, const void *restrict bound, int tp2, bool preceding, lng first_half)
{
	lng cnt = (BUN) BATcount(b), nils = 0, *restrict rb = (lng *) Tloc(r, 0), i = 0, k = 0, j = 0;
	bit *restrict np = p ? (bit*)Tloc(p, 0) : NULL;
	int abort_on_error = 1;

	if (l) {		/* dynamic bounds */
		switch (tp2) {
		case TYPE_bte:{
			bte *restrict limit = (bte *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(_PRECEDING, (lng) limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(_FOLLOWING, (lng) limit[k]);
			}
			break;
		}
		case TYPE_sht:{
			sht *restrict limit = (sht *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(_PRECEDING, (lng) limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(_FOLLOWING, (lng) limit[k]);
			}
			break;
		}
		case TYPE_int:{
			int *restrict limit = (int *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(_PRECEDING, (lng) limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(_FOLLOWING, (lng) limit[k]);
			}
			break;
		}
		case TYPE_lng:{
			lng *restrict limit = (lng *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(_PRECEDING, (lng) limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(_FOLLOWING, (lng) limit[k]);
			}
			break;
		}
#ifdef HAVE_HGE
		case TYPE_hge:{
			hge *restrict limit = (hge *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(_PRECEDING, (limit[k] > (hge) GDK_lng_max) ? GDK_lng_max : (lng) limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(_FOLLOWING, (limit[k] > (hge) GDK_lng_max) ? GDK_lng_max : (lng) limit[k]);
			}
			break;
		}
#endif
		default:
			goto bound_not_supported;
		}
	} else {	/* static bounds, all the limits are cast to lng */
		lng limit;
		switch (tp2) {
		case TYPE_bte:
			limit = is_bte_nil(*(bte *) bound) ? lng_nil : (lng) *(bte *) bound;
			break;
		case TYPE_sht:
			limit = is_sht_nil(*(sht *) bound) ? lng_nil : (lng) *(sht *) bound;
			break;
		case TYPE_int:
			limit = is_int_nil(*(int *) bound) ? lng_nil : (lng) *(int *) bound;
			break;
		case TYPE_lng:
			limit = (lng) (*(lng *) bound);
			break;
#ifdef HAVE_HGE
		case TYPE_hge: {
			hge nval = *(hge *) bound;
			limit = is_hge_nil(nval) ? lng_nil : (nval > (hge) GDK_lng_max) ? GDK_lng_max : (lng) nval;
			break;
		}
#endif
		default:
			goto bound_not_supported;
		}
		if (preceding) {
			ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(_PRECEDING, limit);
		} else {
			ANALYTICAL_WINDOW_BOUNDS_BRANCHES_ROWS(_FOLLOWING, limit);
		}
	}

	BATsetcount(r, cnt);
	r->tnonil = (nils == 0);
	r->tnil = (nils > 0);
	return GDK_SUCCEED;
      bound_not_supported:
	GDKerror("rows frame bound type %s not supported.\n", ATOMname(tp2));
	return GDK_FAIL;
      calc_overflow:
	GDKerror("22003!overflow in calculation.\n");
	return GDK_FAIL;
}

static gdk_return
GDKanalyticalrangebounds(BAT *r, BAT *b, BAT *p, BAT *l, const void *restrict bound, int tp1, int tp2, bool preceding)
{
	lng cnt = (lng) BATcount(b), nils = 0, *restrict rb = (lng *) Tloc(r, 0), i = 0, k = 0, j = 0;
	bit *np = p ? (bit *) Tloc(p, 0) : NULL;
	BATiter bpi = bat_iterator(b);
	int (*atomcmp) (const void *, const void *) = ATOMcompare(tp1);
	const void *nil = ATOMnilptr(tp1);
	int abort_on_error = 1;

	if (l) {		/* dynamic bounds */
		switch (tp2) {
		case TYPE_bte:{
			bte *restrict limit = (bte *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_NUM(_PRECEDING, limit[k], int);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_NUM(_FOLLOWING, limit[k], int);
			}
			break;
		}
		case TYPE_sht:{
			sht *restrict limit = (sht *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_NUM(_PRECEDING, limit[k], int);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_NUM(_FOLLOWING, limit[k], int);
			}
			break;
		}
		case TYPE_int:{
			int *restrict limit = (int *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_NUM(_PRECEDING, limit[k], int);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_NUM(_FOLLOWING, limit[k], int);
			}
			break;
		}
		case TYPE_lng:{
			lng *restrict limit = (lng *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_NUM(_PRECEDING, limit[k], lng);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_NUM(_FOLLOWING, limit[k], lng);
			}
			break;
		}
		case TYPE_flt:{
			flt *restrict limit = (flt *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_FLT(_PRECEDING, limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_FLT(_FOLLOWING, limit[k]);
			}
			break;
		}
		case TYPE_dbl:{
			dbl *restrict limit = (dbl *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_DBL(_PRECEDING, limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_DBL(_FOLLOWING, limit[k]);
			}
			break;
		}
#ifdef HAVE_HGE
		case TYPE_hge:{
			hge *restrict limit = (hge *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_HGE(_PRECEDING, limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_HGE(_FOLLOWING, limit[k]);
			}
			break;
		}
#endif
		default:
			goto bound_not_supported;
		}
	} else {		/* static bounds */
		switch (tp2) {
		case TYPE_bte:
		case TYPE_sht:
		case TYPE_int:
		case TYPE_lng:{
			lng limit = 0;
			switch (tp2) {
			case TYPE_bte:{
				bte ll = (*(bte *) bound);
				limit = (lng) ll;
				break;
			}
			case TYPE_sht:{
				sht ll = (*(sht *) bound);
				limit = (lng) ll;
				break;
			}
			case TYPE_int:{
				int ll = (*(int *) bound);
				limit = (lng) ll;
				break;
			}
			case TYPE_lng:{
				lng ll = (*(lng *) bound);
				limit = (lng) ll;
				break;
			}
			default:
				assert(0);
			}
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_NUM(_PRECEDING, limit, lng);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_NUM(_FOLLOWING, limit, lng);
			}
			break;
		}
		case TYPE_flt:{
			flt limit = (*(flt *) bound);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_FLT(_PRECEDING, limit);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_FLT(_FOLLOWING, limit);
			}
			break;
		}
		case TYPE_dbl:{
			dbl limit = (*(dbl *) bound);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_DBL(_PRECEDING, limit);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_DBL(_FOLLOWING, limit);
			}
			break;
		}
#ifdef HAVE_HGE
		case TYPE_hge:{
			hge limit = (*(hge *) bound);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_HGE(_PRECEDING, limit);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_RANGE_HGE(_FOLLOWING, limit);
			}
			break;
		}
#endif
		default:
			goto bound_not_supported;
		}
	}
	BATsetcount(r, cnt);
	r->tnonil = (nils == 0);
	r->tnil = (nils > 0);
	return GDK_SUCCEED;
      bound_not_supported:
	GDKerror("range frame bound type %s not supported.\n", ATOMname(tp2));
	return GDK_FAIL;
      type_not_supported:
	GDKerror("type %s not supported for %s frame bound type.\n", ATOMname(tp1), ATOMname(tp2));
	return GDK_FAIL;
      calc_overflow:
	GDKerror("22003!overflow in calculation.\n");
	return GDK_FAIL;
}

static gdk_return
GDKanalyticalgroupsbounds(BAT *r, BAT *b, BAT *p, BAT *l, const void *restrict bound, int tp2, bool preceding)
{
	lng cnt = (lng) BATcount(b), *restrict rb = (lng *) Tloc(r, 0), i = 0, k = 0, j = 0;
	bit *restrict np = p ? (bit*)Tloc(p, 0) : NULL, *restrict bp = (bit*) Tloc(b, 0);

	if (b->ttype != TYPE_bit) {
		GDKerror("groups frame bound type must be of type bit\n");
		return GDK_FAIL;
	}

	if (l) {		/* dynamic bounds */
		switch (tp2) {
		case TYPE_bte:{
			bte *restrict limit = (bte *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(_PRECEDING, (lng) limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(_FOLLOWING, (lng) limit[k]);
			}
			break;
		}
		case TYPE_sht:{
			sht *restrict limit = (sht *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(_PRECEDING, (lng) limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(_FOLLOWING, (lng) limit[k]);
			}
			break;
		}
		case TYPE_int:{
			int *restrict limit = (int *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(_PRECEDING, (lng) limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(_FOLLOWING, (lng) limit[k]);
			}
			break;
		}
		case TYPE_lng:{
			lng *restrict limit = (lng *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(_PRECEDING, (lng) limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(_FOLLOWING, (lng) limit[k]);
			}
			break;
		}
#ifdef HAVE_HGE
		case TYPE_hge:{
			hge *restrict limit = (hge *) Tloc(l, 0);
			if (preceding) {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(_PRECEDING, (limit[k] > (hge) GDK_lng_max) ? GDK_lng_max : (lng) limit[k]);
			} else {
				ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(_FOLLOWING, (limit[k] > (hge) GDK_lng_max) ? GDK_lng_max : (lng) limit[k]);
			}
			break;
		}
#endif
		default:
			goto bound_not_supported;
		}
	} else {	/* static bounds, all the limits are cast to lng */
		lng limit;
		switch (tp2) {
		case TYPE_bte:
			limit = is_bte_nil(*(bte *) bound) ? lng_nil : (lng) *(bte *) bound;
			break;
		case TYPE_sht:
			limit = is_sht_nil(*(sht *) bound) ? lng_nil : (lng) *(sht *) bound;
			break;
		case TYPE_int:
			limit = is_int_nil(*(int *) bound) ? lng_nil : (lng) *(int *) bound;
			break;
		case TYPE_lng:
			limit = (lng) (*(lng *) bound);
			break;
#ifdef HAVE_HGE
		case TYPE_hge: {
			hge nval = *(hge *) bound;
			limit = is_hge_nil(nval) ? lng_nil : (nval > (hge) GDK_lng_max) ? GDK_lng_max : (lng) nval;
			break;
		}
#endif
		default:
			goto bound_not_supported;
		}
		if (preceding) {
			ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(_PRECEDING, limit);
		} else {
			ANALYTICAL_WINDOW_BOUNDS_BRANCHES_GROUPS(_FOLLOWING, limit);
		}
	}
	BATsetcount(r, cnt);
	return GDK_SUCCEED;
      bound_not_supported:
	GDKerror("groups frame bound type %s not supported.\n", ATOMname(tp2));
	return GDK_FAIL;
}

gdk_return
GDKanalyticalwindowbounds(BAT *r, BAT *b, BAT *p, BAT *l, const void *restrict bound, int tp1, int tp2, int unit, bool preceding, lng first_half)
{
	assert((l && !bound) || (!l && bound));

	switch (unit) {
	case 0:
		return GDKanalyticalrowbounds(r, b, p, l, bound, tp2, preceding, first_half);
	case 1:
		return GDKanalyticalrangebounds(r, b, p, l, bound, tp1, tp2, preceding);
	case 2:
		return GDKanalyticalgroupsbounds(r, b, p, l, bound, tp2, preceding);
	default:
		assert(0);
	}
	GDKerror("unit type %d not supported (this is a bug).\n", unit);
	return GDK_FAIL;
}
