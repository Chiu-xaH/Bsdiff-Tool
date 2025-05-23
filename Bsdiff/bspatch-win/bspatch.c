/*-
 * Copyright 2003-2005 Colin Percival
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <io.h>
#include <bzlib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdbool.h>
#include "../main.h"

static long offtin(unsigned char *buf)
{
	long y;

	y = buf[7] & 0x7F;
	y = y * 256;y += buf[6];
	y = y * 256;y += buf[5];
	y = y * 256;y += buf[4];
	y = y * 256;y += buf[3];
	y = y * 256;y += buf[2];
	y = y * 256;y += buf[1];
	y = y * 256;y += buf[0];

	if (buf[7] & 0x80) y = -y;

	return y;
}

int main_patch(int argc, char * argv[])
{
	FILE * f, *cpf, *dpf, *epf;
	BZFILE * cpfbz2, *dpfbz2, *epfbz2;
	int cbz2err, dbz2err, ebz2err;
	FILE * fs;
	long oldsize, newsize;
	long bzctrllen, bzdatalen;
	unsigned char header[32], buf[8];
	unsigned char *pold, *pnew;
	long oldpos, newpos;
	long ctrl[3];
	long lenread;
	long i;

	if (argc != 4) return -1;

	/* Open patch file */
	if ((f = fopen(argv[3], "rb")) == NULL)
		return -2;

	/*
	File format:
		0	8	"BSDIFF40"
		8	8	X
		16	8	Y
		24	8	sizeof(newfile)
		32	X	bzip2(control block)
		32+X	Y	bzip2(diff block)
		32+X+Y	???	bzip2(extra block)
	with control block a set of triples (x,y,z) meaning "add x bytes
	from oldfile to x bytes from the diff block; copy y bytes from the
	extra block; seek forwards in oldfile by z bytes".
	*/

	/* Read header */
	if (fread(header, 1, 32, f) < 32) {
		if (feof(f))
			return -3;
		return -4;
	}

	/* Check for appropriate magic */
	if (memcmp(header, "BSDIFF40", 8) != 0)
		return -5;

	/* Read lengths from header */
	bzctrllen = offtin(header + 8);
	bzdatalen = offtin(header + 16);
	newsize = offtin(header + 24);
	if ((bzctrllen < 0) || (bzdatalen < 0) || (newsize < 0))
		return -6;

	/* Close patch file and re-open it via libbzip2 at the right places */
	if (fclose(f))
		return -7;
	if ((cpf = fopen(argv[3], "rb")) == NULL)
		return -7;
	if (fseek(cpf, 32, SEEK_SET))
		return -7;
	if ((cpfbz2 = BZ2_bzReadOpen(&cbz2err, cpf, 0, 0, NULL, 0)) == NULL)
		return -7;
	if ((dpf = fopen(argv[3], "rb")) == NULL)
		return -7;
	if (fseek(dpf, 32 + bzctrllen, SEEK_SET))
		return -7;
	if ((dpfbz2 = BZ2_bzReadOpen(&dbz2err, dpf, 0, 0, NULL, 0)) == NULL)
		return -7;
	if ((epf = fopen(argv[3], "rb")) == NULL)
		return -7;
	if (fseek(epf, 32 + bzctrllen + bzdatalen, SEEK_SET))
		return -7;
	if ((epfbz2 = BZ2_bzReadOpen(&ebz2err, epf, 0, 0, NULL, 0)) == NULL)
		return -7;

	fs = fopen(argv[1], "rb");
	if (fs == NULL) return -8;
	if (fseek(fs, 0, SEEK_END) != 0) return -9;
	oldsize = ftell(fs);
	pold = (unsigned char *)malloc(oldsize + 1);
	if (pold == NULL)	return -10;
	fseek(fs, 0, SEEK_SET);
	if (fread(pold, 1, oldsize, fs) == -1)	return -10;
	if (fclose(fs) == -1)	return -11;

	pnew = malloc(newsize + 1);
	if (pnew == NULL) return -12;

	oldpos = 0;newpos = 0;
	while (newpos < newsize) {
		/* Read control data */
		for (i = 0;i <= 2;i++) {
			lenread = BZ2_bzRead(&cbz2err, cpfbz2, buf, 8);
			if ((lenread < 8) || ((cbz2err != BZ_OK) &&
				(cbz2err != BZ_STREAM_END)))
				return -13;
			ctrl[i] = offtin(buf);
		};

		/* Sanity-check */
		if (newpos + ctrl[0] > newsize)
			return -13;

		/* Read diff string */
		lenread = BZ2_bzRead(&dbz2err, dpfbz2, pnew + newpos, ctrl[0]);
		if ((lenread < ctrl[0]) ||
			((dbz2err != BZ_OK) && (dbz2err != BZ_STREAM_END)))
			return -13;

		/* Add pold data to diff string */
		for (i = 0;i < ctrl[0];i++)
			if ((oldpos + i >= 0) && (oldpos + i < oldsize))
				pnew[newpos + i] += pold[oldpos + i];

		/* Adjust pointers */
		newpos += ctrl[0];
		oldpos += ctrl[0];

		/* Sanity-check */
		if (newpos + ctrl[1] > newsize)
			return -13;

		/* Read extra string */
		lenread = BZ2_bzRead(&ebz2err, epfbz2, pnew + newpos, ctrl[1]);
		if ((lenread < ctrl[1]) ||
			((ebz2err != BZ_OK) && (ebz2err != BZ_STREAM_END)))
			return -13;

		/* Adjust pointers */
		newpos += ctrl[1];
		oldpos += ctrl[2];
	};

	/* Clean up the bzip2 reads */
	BZ2_bzReadClose(&cbz2err, cpfbz2);
	BZ2_bzReadClose(&dbz2err, dpfbz2);
	BZ2_bzReadClose(&ebz2err, epfbz2);
	if (fclose(cpf) || fclose(dpf) || fclose(epf))
		return -14;

	/* Write the pnew file */
	fs = fopen(argv[2], "wb");
	if (fs == NULL) return -15;
	if (fwrite(pnew, 1, newsize, fs) == -1) return -15;
	if (fclose(fs) == -1) return -15;

	free(pnew);
	free(pold);
    return 0;
}

// 返回1 为成功
bool merge(const char *oldFile, const char *patchFile, const char *newFile) {
    char *bspatch = "bspatch";
    char* argv[] = { bspatch,(char*)oldFile, (char*)newFile, (char*)patchFile };
    int result = main_patch(4, argv);
    return result == 0;
}