/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0.  If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright 1997 - July 2008 CWI, August 2008 - 2020 MonetDB B.V.
 */

/*
 *  Niels Nes
 *  A simple interface to IO streams
 * All file IO is tunneled through the stream library, which guarantees
 * cross-platform capabilities.  Several protocols are provided, e.g. it
 * can be used to open 'non compressed, gzipped, bzip2ed' data files.  It
 * encapsulates the corresponding library managed in common/stream.
 */

#include "monetdb_config.h"
#include "streams.h"
#include "mal_exception.h"

str mnstr_open_rstreamwrap(Stream *S, str *filename)
{
	stream *s;

	if ((s = open_rstream(*filename)) == NULL || mnstr_errnr(s)) {
		int errnr = errno;
		if (s)
			close_stream(s);
		throw(IO, "streams.open", "could not open file '%s': %s",
				*filename, GDKstrerror(errnr, (char[128]){0}, 128));
	} else {
		*(stream**)S = s;
	}

	return MAL_SUCCEED;
}
str mnstr_open_wstreamwrap(Stream *S, str *filename)
{
	stream *s;

	if ((s = open_wstream(*filename)) == NULL || mnstr_errnr(s)) {
		int errnr = errno;
		if (s)
			close_stream(s);
		throw(IO, "streams.open", "could not open file '%s': %s",
				*filename, GDKstrerror(errnr, (char[128]){0}, 128));
	} else {
		*(stream**)S = s;
	}

	return MAL_SUCCEED;
}

str mnstr_open_rastreamwrap(Stream *S, str *filename)
{
	stream *s;

	if ((s = open_rastream(*filename)) == NULL || mnstr_errnr(s)) {
		int errnr = errno;
		if (s)
			close_stream(s);
		throw(IO, "streams.open", "could not open file '%s': %s",
				*filename, GDKstrerror(errnr, (char[128]){0}, 128));
	} else {
		*(stream**)S = s;
	}

	return MAL_SUCCEED;
}

str mnstr_open_wastreamwrap(Stream *S, str *filename)
{
	stream *s;

	if ((s = open_wastream(*filename)) == NULL || mnstr_errnr(s)) {
		int errnr = errno;
		if (s)
			close_stream(s);
		throw(IO, "streams.open", "could not open file '%s': %s",
				*filename, GDKstrerror(errnr, (char[128]){0}, 128));
	} else {
		*(stream**)S = s;
	}

	return MAL_SUCCEED;
}

str
mnstr_write_stringwrap(void *ret, Stream *S, str *data)
{
	stream *s = *(stream **)S;
	(void)ret;

	if (mnstr_write(s, *data, 1, strlen(*data)) < 0)
		throw(IO, "streams.writeStr", "failed to write string");

	return MAL_SUCCEED;
}

str
mnstr_writeIntwrap(void *ret, Stream *S, int *data)
{
	stream *s = *(stream **)S;
	(void)ret;

	if (!mnstr_writeInt(s, *data))
		throw(IO, "streams.writeInt", "failed to write int");

	return MAL_SUCCEED;
}

str
mnstr_readIntwrap(int *ret, Stream *S)
{
	stream *s = *(stream **)S;

	if (mnstr_readInt(s, ret) != 1)
		throw(IO, "streams.readInt", "failed to read int");

	return MAL_SUCCEED;
}

#define CHUNK (64 * 1024)
str
mnstr_read_stringwrap(str *res, Stream *S)
{
	stream *s = *(stream **)S;
	ssize_t len = 0;
	size_t size = CHUNK + 1;
	char *buf = GDKmalloc(size), *start = buf, *tmp;

	if( buf == NULL)
		throw(MAL,"mnstr_read_stringwrap", SQLSTATE(HY013) MAL_MALLOC_FAIL);
	while ((len = mnstr_read(s, start, 1, CHUNK)) > 0) {
		size += len;
		tmp = GDKrealloc(buf, size);
		if (tmp == NULL) {
			GDKfree(buf);
			throw(MAL,"mnstr_read_stringwrap", SQLSTATE(HY013) MAL_MALLOC_FAIL);
		}
		buf = tmp;
		start = buf + size - CHUNK - 1;

		*start = '\0';
	}
	if (len < 0)
		throw(IO, "streams.readStr", "failed to read string");
	start += len;
	*start = '\0';
	*res = buf;

	return MAL_SUCCEED;
}

str
mnstr_flush_streamwrap(void *ret, Stream *S)
{
	stream *s = *(stream **)S;
	(void)ret;

	if (mnstr_flush(s))
		throw(IO, "streams.flush", "failed to flush stream");

	return MAL_SUCCEED;
}

str
mnstr_close_streamwrap(void *ret, Stream *S)
{
	(void)ret;

	close_stream(*(stream **)S);

	return MAL_SUCCEED;
}

str
open_block_streamwrap(Stream *S, Stream *is)
{
	if ((*(stream **)S = block_stream(*(stream **)is)) == NULL)
		throw(IO, "bstreams.open", "failed to open block stream");

	return MAL_SUCCEED;
}

str
bstream_create_wrapwrap(Bstream *Bs, Stream *S, int *bufsize)
{
	if ((*(bstream **)Bs = bstream_create(*(stream **)S, (size_t)*bufsize)) == NULL)
		throw(IO, "bstreams.create", "failed to create block stream");

	return MAL_SUCCEED;
}

str
bstream_destroy_wrapwrap(void *ret, Bstream *BS)
{
	(void)ret;

	bstream_destroy(*(bstream **)BS);

	return MAL_SUCCEED;
}

str
bstream_read_wrapwrap(int *res, Bstream *BS, int *size)
{
	*res = (int)bstream_read(*(bstream **)BS, (size_t)*size);

	return MAL_SUCCEED;
}

#include "mel.h"
mel_atom streams_init_atoms[] = {
 { .name="streams", .basetype="ptr", },
 { .name="bstream", .basetype="ptr", },
 { .name=NULL } 
};
mel_func streams_init_funcs[] = {
 { .command=true, .mod="streams", .fcn="openReadBytes", .imp=(fptr)&mnstr_open_rstreamwrap, .unsafe=true, .comment="open a file stream for reading", .args={{ .name="filename", .type="str", .isbat=false, .vargs=false }, }, .res={{ .type="streams", .isbat=false, .vargs=false }, } },
 { .command=true, .mod="streams", .fcn="openWriteBytes", .imp=(fptr)&mnstr_open_wstreamwrap, .unsafe=true, .comment="open a file stream for writing", .args={{ .name="filename", .type="str", .isbat=false, .vargs=false }, }, .res={{ .type="streams", .isbat=false, .vargs=false }, } },
 { .command=true, .mod="streams", .fcn="openRead", .imp=(fptr)&mnstr_open_rastreamwrap, .unsafe=true, .comment="open ascii file stream for reading", .args={{ .name="filename", .type="str", .isbat=false, .vargs=false }, }, .res={{ .type="streams", .isbat=false, .vargs=false }, } },
 { .command=true, .mod="streams", .fcn="openWrite", .imp=(fptr)&mnstr_open_wastreamwrap, .unsafe=true, .comment="open ascii file stream for writing", .args={{ .name="filename", .type="str", .isbat=false, .vargs=false }, }, .res={{ .type="streams", .isbat=false, .vargs=false }, } },
 { .command=true, .mod="streams", .fcn="blocked", .imp=(fptr)&open_block_streamwrap, .unsafe=true, .comment="open a block based stream", .args={{ .name="s", .type="streams", .isbat=false, .vargs=false }, }, .res={{ .type="streams", .isbat=false, .vargs=false }, } },
 { .command=true, .mod="streams", .fcn="writeStr", .imp=(fptr)&mnstr_write_stringwrap, .unsafe=true, .comment="write data on the stream", .args={{ .name="s", .type="streams", .isbat=false, .vargs=false }, { .name="data", .type="str", .isbat=false, .vargs=false }, }, .res={{ .type="void", .isbat=false, .vargs=false }, } },
 { .command=true, .mod="streams", .fcn="writeInt", .imp=(fptr)&mnstr_writeIntwrap, .unsafe=true, .comment="write data on the stream", .args={{ .name="s", .type="streams", .isbat=false, .vargs=false }, { .name="data", .type="int", .isbat=false, .vargs=false }, }, .res={{ .type="void", .isbat=false, .vargs=false }, } },
 { .command=true, .mod="streams", .fcn="readStr", .imp=(fptr)&mnstr_read_stringwrap, .unsafe=true, .comment="read string data from the stream", .args={{ .name="s", .type="streams", .isbat=false, .vargs=false }, }, .res={{ .type="str", .isbat=false, .vargs=false }, } },
 { .command=true, .mod="streams", .fcn="readInt", .imp=(fptr)&mnstr_readIntwrap, .unsafe=true, .comment="read integer data from the stream", .args={{ .name="s", .type="streams", .isbat=false, .vargs=false }, }, .res={{ .type="int", .isbat=false, .vargs=false }, } },
 { .command=true, .mod="streams", .fcn="flush", .imp=(fptr)&mnstr_flush_streamwrap, .unsafe=true, .comment="flush the stream", .args={{ .name="s", .type="streams", .isbat=false, .vargs=false }, }, .res={NULL} },
 { .command=true, .mod="streams", .fcn="close", .imp=(fptr)&mnstr_close_streamwrap, .unsafe=true, .comment="close and destroy the stream s", .args={{ .name="s", .type="streams", .isbat=false, .vargs=false }, }, .res={NULL} },
 { .command=true, .mod="streams", .fcn="create", .imp=(fptr)&bstream_create_wrapwrap, .unsafe=true, .comment="create a buffered stream", .args={{ .name="s", .type="streams", .isbat=false, .vargs=false }, { .name="bufsize", .type="int", .isbat=false, .vargs=false }, }, .res={{ .type="bstream", .isbat=false, .vargs=false }, } },
 { .command=true, .mod="streams", .fcn="destroy", .imp=(fptr)&bstream_destroy_wrapwrap, .unsafe=true, .comment="destroy bstream", .args={{ .name="s", .type="bstream", .isbat=false, .vargs=false }, }, .res={NULL} },
 { .command=true, .mod="streams", .fcn="read", .imp=(fptr)&bstream_read_wrapwrap, .unsafe=true, .comment="read at least size bytes into the buffer of s", .args={{ .name="s", .type="bstream", .isbat=false, .vargs=false }, { .name="size", .type="int", .isbat=false, .vargs=false }, }, .res={{ .type="int", .isbat=false, .vargs=false }, } },
 { .imp=NULL }
};
#include "mal_import.h"
#ifdef _MSC_VER
#undef read
#pragma section(".CRT$XCU",read)
#endif
LIB_STARTUP_FUNC(init_streams_mal)
{ mal_module("streams", streams_init_atoms, streams_init_funcs); }
