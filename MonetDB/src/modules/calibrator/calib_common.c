/*
 * The contents of this file are subject to the MonetDB Public
 * License Version 1.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at 
 * http://monetdb.cwi.nl/Legal/MonetDBPL-1.0.html
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Monet Database System.
 * 
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-2002 CWI.  
 * All Rights Reserved.
 * 
 * Contributor(s):
 * 		Martin Kersten <Martin.Kersten@cwi.nl>
 * 		Peter Boncz <Peter.Boncz@cwi.nl>
 * 		Niels Nes <Niels.Nes@cwi.nl>
 * 		Stefan Manegold  <Stefan.Manegold@cwi.nl>
 */

/***************************************************************************\
*              COMMON DEFINITIONS AND FUNCTIONS                             *
\***************************************************************************/

#define VERSION "0.9i"

#define	MIN(a,b)	(a<b?a:b)
#define	MAX(a,b)	(a>b?a:b)
#define NOABS(x)	((calibdbl)x)
#define FABS		fabs

#define	caliblng	long
#define	calibdbl	float

#define fatal(m) printf("Fatal '%s' file=%s, line=%d", m, __FILE__, __LINE__);
#define fatalex(m) {fatal(m); exit(1); }

#ifdef WIN32

#include <Windows.h>

/*	#define	MINTIME  100000	*/

size_t getpagesize() { return 4096; }

caliblng oldtp = 0;

caliblng now(void)
{
	caliblng tp = (caliblng)timeGetTime();
	if (oldtp == 0) {
		/* timeBeginPeriod(1); */
		tp += 11;
		while ((caliblng)timeGetTime() <= tp) ;
		oldtp = tp = (caliblng)timeGetTime();
	}
        return (caliblng)((tp - oldtp) * (caliblng)1000);
}

#else

# include <config.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

/*	#define	MINTIME  10000	*/

struct timeval oldtp = { 0 };

caliblng now(void)
{
	struct timeval tp;
	gettimeofday(&tp, 0);
	if (oldtp.tv_sec == 0 && oldtp.tv_usec == 0) {
		oldtp = tp;
	}
        return (caliblng)( (caliblng)(tp.tv_sec  - oldtp.tv_sec ) * (caliblng)1000000 + 
			(caliblng)(tp.tv_usec - oldtp.tv_usec)	);
}

#endif

void ErrXit(char *format, ...) {
        va_list	ap;
        char	s[1024];

	va_start(ap,format);
        vsprintf(s, format, ap);
  	va_end(ap);
	fprintf(stderr, "\n! %s !\n", s);
	fflush(stderr);
	exit(1);
}

caliblng round(calibdbl x)
{
	return (caliblng)(x + 0.5);
}

char last(char *s)
{
	while (*s++);
	return (s[-2]);
}

caliblng bytes(char *s)
{
	caliblng	n = atoi(s);

	if ((last(s) == 'k') || (last(s) == 'K'))
		n *= 1024;
	if ((last(s) == 'm') || (last(s) == 'M'))
		n *= (1024 * 1024);
	if ((last(s) == 'g') || (last(s) == 'G'))
		n *= (1024 * 1024 * 1024);
	return (n);
}

caliblng getMINTIME () {
	caliblng t0=0, t1=0;
	t0=t1=now();
	while(t0>=t1){
		t1=now();
	}
	return (t1-t0);
}



