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

#include	<stdio.h>

#include	"Mx.h"
#include	"MxFcnDef.h"

#define TEXMODE	(textmode==M_TEX)
#define WWWMODE	(textmode==M_WWW)


	int	pr_hide = 0;
	int	pr_hide_text = 0;
	int	pr_env= 0;
	int	pr_math= 0;
	int	pr_pos= 0;

void	PrEnv(env)
int	env;
{
	if( Hide() ) return;

	if(env == pr_env){
		return;
	}

	switch( pr_env ){
	case E_TEXT:
		if TEXMODE ofile_printf("}");
		break;
	case E_CODE:
		if TEXMODE ofile_printf("\\end{verbatim}\\normalsize}\n");
		else if WWWMODE ofile_printf("</font></pre>\n");
		break;
	case E_CMD:
		break;
	}
	pr_env= env;
	pr_pos= 0;
	switch( pr_env ){
	case E_TEXT:
		if TEXMODE ofile_printf("{");
		break;
	case E_CODE:
		if TEXMODE ofile_printf("{\\codesize\\begin{verbatim}\n");
		else if WWWMODE ofile_printf("<pre><font size=\"-1\" color=\"%s\">", code_color);
		break;
	case E_CMD:
		break;
	}
}

/* Low Level Print routines
 */

void	PrRef(mod, sec)
int	mod;
int	sec;
{
	PrChr('[');
	if( mod != 0 )
		PrNum(mod);
	else
		PrChr('?');
	if( sec != 0 ){
		PrChr('.');
		PrNum(sec);
	}
	PrChr(']');
}

void	PrNum(n)
int	n;
{
	if( Hide() ) return;

	ofile_printf("%d", n);
}

void PrTxt(s)
    char * s;
{

    char c = *s;
    while((c = (*s++)))
	switch(c){
	case '\t':
	case '\n':
	case ' ':
		MathOff();
                ofile_printf("%c", c);	
		break;
	case '~':
		MathOff(); ofile_printf("\\~\\,");
		break;
	case '[':
	case ']':
		MathOff(); ofile_printf("{%c}", c);
		break;
	case '&':
	case '%':
	case '$':
	case '#':
	case '_':
	case '{':
	case '}':
		MathOff(); ofile_printf("\\%c", c);
		break;
	default:
		MathOff(); ofile_printf("%c", c);
		break;
	case '\\':
		MathOn(); ofile_printf("\\backslash");
		break;
	case '/':
	case '|':
	case '-':
	case '+':
	case '<':
	case '>':
	case '*':
	case '=':
		MathOn(); ofile_printf("%c", c);
		break;
	}
}


void	PrStr(s)
char *	s;
{
	char *	c;
	
	if( Hide() ) return;

	for( c = s; *c != '\0'; c++ ){
		if( (c[0] == '\\') && (c[1] == '@') )
			c++;
		PrChr(*c);
	}
	MathOff();
}

extern int codeline;

void PrCodeline()
{
	if (WWWMODE && (pr_pos<8)) {
		ofile_printf("<font color=\"8FBC8F\" size=\"-2\">%6d  </font>", codeline);
		pr_pos = 8;
	}
}

void	PrChr(char c)
{
	extern int opt_column;
	int start_pos = pr_pos;

	if( Hide() ) return;

	/* administer pr_pos and codeline */
	switch( c ){
	case'\t':
	    do {
	        pr_pos++; 
	    } while( (pr_pos % (8 / opt_column)) != 0 );
	    break;
	case '\n':
	    codeline++; pr_pos= 0;
	    break;
	default:
	    pr_pos++;
	}

	if (WWWMODE || (pr_env & E_TEXT) == E_TEXT ) {
	    switch( c ){
	    case '\n':
	    case '\t':
	    case ' ':
		MathOff();
		ofile_putc(c);
		break;
	    case '~':
		MathOff();
		if TEXMODE ofile_printf("\\~\\,");
		else	ofile_printf("%c",c);
		break;
	    case '[':
	    case ']':
		MathOff();
		if TEXMODE ofile_printf("{%c}", c);
		else	ofile_printf("%c",c);
		break;
	    case '&':
		if WWWMODE {
		    ofile_puts("&#38;");
		    break;
		}
	    case '%':
	    case '$':
	    case '#':
	    case '_':
	    case '{':
	    case '}':
		MathOff();
		if TEXMODE ofile_printf("\\%c", c);
		else	ofile_printf("%c",c);
		break;
	    case '\\':
		MathOn();
		if TEXMODE	ofile_printf("\\backslash");
		else if WWWMODE ofile_putc('\\'); 
		else	        ofile_printf("\\e");
		break;
	    case '<':
		if WWWMODE {
			ofile_puts("&#60;");
			break;
		}
	    case '>':
		if WWWMODE {
			ofile_puts("&#62;");
			break;
		}
	    case '/':
	    case '|':
	    case '-':
	    case '+':
	    case '*':
	    case '=':
		MathOn();
		ofile_putc(c);
		break;
	    case '"':
		if WWWMODE {
		    ofile_puts("&#34;");
		    break;
		}
	    default:
		MathOff();
		ofile_putc(c);
		break;
	    } 
  	    return;
	}

	if ((pr_env & E_CODE) == E_CODE ){
	    PrCodeline();
	    switch(c){
	    case'\t':
		while(start_pos++ < pr_pos) ofile_putc(' ');
		break;
	    case '\\':
		ofile_puts(TEXMODE?"\\":"\\e");
		break;
	    default:
		ofile_putc(c);
	    }
	    return;
	}

	if ((pr_env & E_CMD) == E_CMD){
	    switch(c){
	    default:
		ofile_putc(c);
		break;
	    }
	}
}

void	MathOn()
{
	if TEXMODE
	if( pr_math == 0 )
		ofile_printf("${\\scriptstyle");
	pr_math= 1;
}

void	MathOff()
{
	if TEXMODE
	if( pr_math == 1)
		ofile_printf("}$");
	pr_math= 0;
}

void	HideOn()
{
	pr_hide++;
}

void	HideText()
{
	pr_hide_text = 1;
}

void	HideOff()
{
    extern char *defHideText;

    if( (pr_hide == opt_hide) && !pr_hide_text && (defHideText != (char *)0))
	ofile_printf(defHideText);
    pr_hide--;
    pr_hide_text = 0;
}

int	Hide()
{
	if( archived) return 1;
	return (((opt_hide == NO_HIDE) && pr_hide_text)||
		((pr_hide != opt_hide) && pr_hide_text)||
		((pr_hide >= opt_hide)&&(opt_hide != NO_HIDE)&&!pr_hide_text));
}
