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
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../main.h"

#define MIN(x,y) (((x)<(y)) ? (x) : (y))

#define errx_diff err_diff
void err_diff(int exitcode, const char * fmt, ...)
{
	va_list valist;
	va_start(valist, fmt);
	vprintf(fmt, valist);
	va_end(valist);
	exit(exitcode);
}

static void split(long *I, long *V, long start, long len, long h)
{
	long i, j, k, x, tmp, jj, kk;

	if (len < 16) {
		for (k = start;k < start + len;k += j) {
			j = 1;x = V[I[k] + h];
			for (i = 1;k + i < start + len;i++) {
				if (V[I[k + i] + h] < x) {
					x = V[I[k + i] + h];
					j = 0;
				};
				if (V[I[k + i] + h] == x) {
					tmp = I[k + j];I[k + j] = I[k + i];I[k + i] = tmp;
					j++;
				};
			};
			for (i = 0;i < j;i++) V[I[k + i]] = k + j - 1;
			if (j == 1) I[k] = -1;
		};
		return;
	};

	x = V[I[start + len / 2] + h];
	jj = 0;kk = 0;
	for (i = start;i < start + len;i++) {
		if (V[I[i] + h] < x) jj++;
		if (V[I[i] + h] == x) kk++;
	};
	jj += start;kk += jj;

	i = start;j = 0;k = 0;
	while (i < jj) {
		if (V[I[i] + h] < x) {
			i++;
		}
		else if (V[I[i] + h] == x) {
			tmp = I[i];I[i] = I[jj + j];I[jj + j] = tmp;
			j++;
		}
		else {
			tmp = I[i];I[i] = I[kk + k];I[kk + k] = tmp;
			k++;
		};
	};

	while (jj + j < kk) {
		if (V[I[jj + j] + h] == x) {
			j++;
		}
		else {
			tmp = I[jj + j];I[jj + j] = I[kk + k];I[kk + k] = tmp;
			k++;
		};
	};

	if (jj > start) split(I, V, start, jj - start, h);

	for (i = 0;i < kk - jj;i++) V[I[jj + i]] = kk - 1;
	if (jj == kk - 1) I[jj] = -1;

	if (start + len > kk) split(I, V, kk, start + len - kk, h);
}

static void qsufsort(long *I, long *V, unsigned char *pold, long oldsize)
{
	long buckets[256];
	long i, h, len;

	for (i = 0;i < 256;i++) buckets[i] = 0;
	for (i = 0;i < oldsize;i++) buckets[pold[i]]++;
	for (i = 1;i < 256;i++) buckets[i] += buckets[i - 1];
	for (i = 255;i > 0;i--) buckets[i] = buckets[i - 1];
	buckets[0] = 0;

	for (i = 0;i < oldsize;i++) I[++buckets[pold[i]]] = i;
	I[0] = oldsize;
	for (i = 0;i < oldsize;i++) V[i] = buckets[pold[i]];
	V[oldsize] = 0;
	for (i = 1;i < 256;i++) if (buckets[i] == buckets[i - 1] + 1) I[buckets[i]] = -1;
	I[0] = -1;

	for (h = 1;I[0] != -(oldsize + 1);h += h) {
		len = 0;
		for (i = 0;i < oldsize + 1;) {
			if (I[i] < 0) {
				len -= I[i];
				i -= I[i];
			}
			else {
				if (len) I[i - len] = -len;
				len = V[I[i]] + 1 - i;
				split(I, V, i, len, h);
				i += len;
				len = 0;
			};
		};
		if (len) I[i - len] = -len;
	};

	for (i = 0;i < oldsize + 1;i++) I[V[i]] = i;
}

static long matchlen(unsigned char *pold, long oldsize, unsigned char *pnew, long newsize)
{
	long i;

	for (i = 0;(i < oldsize) && (i < newsize);i++)
		if (pold[i] != pnew[i]) break;

	return i;
}

static long search(long *I, unsigned char *pold, long oldsize,
	unsigned char *pnew, long newsize, long st, long en, long *pos)
{
	long x, y;

	if (en - st < 2) {
		x = matchlen(pold + I[st], oldsize - I[st], pnew, newsize);
		y = matchlen(pold + I[en], oldsize - I[en], pnew, newsize);

		if (x > y) {
			*pos = I[st];
			return x;
		}
		else {
			*pos = I[en];
			return y;
		}
	};

	x = st + (en - st) / 2;
	if (memcmp(pold + I[x], pnew, MIN(oldsize - I[x], newsize)) < 0) {
		return search(I, pold, oldsize, pnew, newsize, x, en, pos);
	}
	else {
		return search(I, pold, oldsize, pnew, newsize, st, x, pos);
	};
}

static void offtout(long x, unsigned char *buf)
{
	long y;

	if (x < 0) y = -x; else y = x;

	buf[0] = y % 256;y -= buf[0];
	y = y / 256;buf[1] = y % 256;y -= buf[1];
	y = y / 256;buf[2] = y % 256;y -= buf[2];
	y = y / 256;buf[3] = y % 256;y -= buf[3];
	y = y / 256;buf[4] = y % 256;y -= buf[4];
	y = y / 256;buf[5] = y % 256;y -= buf[5];
	y = y / 256;buf[6] = y % 256;y -= buf[6];
	y = y / 256;buf[7] = y % 256;

	if (x < 0) buf[7] |= 0x80;
}

int main_diff(int argc, char *argv[])
{
	FILE * fs;
	unsigned char *pold, *pnew;
	long oldsize, newsize;
	long *I, *V;
	long scan, pos, len;
	long lastscan, lastpos, lastoffset;
	long oldscore, scsc;
	long s, Sf, lenf, Sb, lenb;
	long overlap, Ss, lens;
	long i;
	long dblen, eblen;
	unsigned char *db, *eb;
	unsigned char buf[8];
	unsigned char header[32];
	FILE * pf;
	BZFILE * pfbz2;
	int bz2err;

	if (argc != 4) errx_diff(1, "usage: %s oldfile newfile patchfile\n", argv[0]);

	/* Allocate oldsize+1 bytes instead of oldsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	fs = fopen(argv[1], "rb");
	if (fs == NULL)err_diff(1, "Open failed :%s", argv[1]);
	if (fseek(fs, 0, SEEK_END) != 0)err_diff(1, "Seek failed :%s", argv[1]);
	oldsize = ftell(fs);
	pold = (unsigned char *)malloc(oldsize + 1);
	if (pold == NULL)	err_diff(1, "Malloc failed :%s", argv[1]);
	fseek(fs, 0, SEEK_SET);
	if (fread(pold, 1, oldsize, fs) == -1)	err_diff(1, "Read failed :%s", argv[1]);
	if (fclose(fs) == -1)	err_diff(1, "Close failed :%s", argv[1]);

	if (((I = (long *)malloc((oldsize + 1) * sizeof(long))) == NULL) ||
		((V = (long *)malloc((oldsize + 1) * sizeof(long))) == NULL)) err_diff(1, NULL);

	qsufsort(I, V, pold, oldsize);

	free(V);

	/* Allocate newsize+1 bytes instead of newsize bytes to ensure
		that we never try to malloc(0) and get a NULL pointer */
	fs = fopen(argv[2], "rb");
	if (fs == NULL)	err_diff(1, "Open failed :%s", argv[2]);
	if (fseek(fs, 0, SEEK_END) != 0)err_diff(1, "Seek failed :%s", argv[2]);
	newsize = ftell(fs);
	pnew = (unsigned char *)malloc(newsize + 1);
	if (pnew == NULL)	err_diff(1, "Malloc failed :%s", argv[2]);
	fseek(fs, 0, SEEK_SET);
	if (fread(pnew, 1, newsize, fs) == -1)	err_diff(1, "Read failed :%s", argv[2]);
	if (fclose(fs) == -1)	err_diff(1, "Close failed :%s", argv[2]);

	if (((db = (unsigned char *)malloc(newsize + 1)) == NULL) ||
		((eb = (unsigned char *)malloc(newsize + 1)) == NULL)) err_diff(1, NULL);
	dblen = 0;
	eblen = 0;

	/* Create the patch file */
	if ((pf = fopen(argv[3], "wb")) == NULL)
		err_diff(1, "Open failed %s", argv[3]);

	/* Header is
		0	8	 "BSDIFF40"
		8	8	length of bzip2ed ctrl block
		16	8	length of bzip2ed diff block
		24	8	length of pnew file */
		/* File is
			0	32	Header
			32	??	Bzip2ed ctrl block
			??	??	Bzip2ed diff block
			??	??	Bzip2ed extra block */
	memcpy(header, "BSDIFF40", 8);
	offtout(0, header + 8);
	offtout(0, header + 16);
	offtout(newsize, header + 24);
	if (fwrite(header, 32, 1, pf) != 1)
		err_diff(1, "fwrite(%s)", argv[3]);

	/* Compute the differences, writing ctrl as we go */
	if ((pfbz2 = BZ2_bzWriteOpen(&bz2err, pf, 9, 0, 0)) == NULL)
		errx_diff(1, "BZ2_bzWriteOpen, bz2err = %d", bz2err);
	scan = 0;len = 0;
	lastscan = 0;lastpos = 0;lastoffset = 0;
	while (scan < newsize) {
		oldscore = 0;

		for (scsc = scan += len;scan < newsize;scan++) {
			len = search(I, pold, oldsize, pnew + scan, newsize - scan,
				0, oldsize, &pos);

			for (;scsc < scan + len;scsc++)
				if ((scsc + lastoffset < oldsize) &&
					(pold[scsc + lastoffset] == pnew[scsc]))
					oldscore++;

			if (((len == oldscore) && (len != 0)) ||
				(len > oldscore + 8)) break;

			if ((scan + lastoffset < oldsize) &&
				(pold[scan + lastoffset] == pnew[scan]))
				oldscore--;
		};

		if ((len != oldscore) || (scan == newsize)) {
			s = 0;Sf = 0;lenf = 0;
			for (i = 0;(lastscan + i < scan) && (lastpos + i < oldsize);) {
				if (pold[lastpos + i] == pnew[lastscan + i]) s++;
				i++;
				if (s * 2 - i > Sf * 2 - lenf) { Sf = s; lenf = i; };
			};

			lenb = 0;
			if (scan < newsize) {
				s = 0;Sb = 0;
				for (i = 1;(scan >= lastscan + i) && (pos >= i);i++) {
					if (pold[pos - i] == pnew[scan - i]) s++;
					if (s * 2 - i > Sb * 2 - lenb) { Sb = s; lenb = i; };
				};
			};

			if (lastscan + lenf > scan - lenb) {
				overlap = (lastscan + lenf) - (scan - lenb);
				s = 0;Ss = 0;lens = 0;
				for (i = 0;i < overlap;i++) {
					if (pnew[lastscan + lenf - overlap + i] ==
						pold[lastpos + lenf - overlap + i]) s++;
					if (pnew[scan - lenb + i] ==
						pold[pos - lenb + i]) s--;
					if (s > Ss) { Ss = s; lens = i + 1; };
				};

				lenf += lens - overlap;
				lenb -= lens;
			};

			for (i = 0;i < lenf;i++)
				db[dblen + i] = pnew[lastscan + i] - pold[lastpos + i];
			for (i = 0;i < (scan - lenb) - (lastscan + lenf);i++)
				eb[eblen + i] = pnew[lastscan + lenf + i];

			dblen += lenf;
			eblen += (scan - lenb) - (lastscan + lenf);

			offtout(lenf, buf);
			BZ2_bzWrite(&bz2err, pfbz2, buf, 8);
			if (bz2err != BZ_OK)
				errx_diff(1, "BZ2_bzWrite, bz2err = %d", bz2err);

			offtout((scan - lenb) - (lastscan + lenf), buf);
			BZ2_bzWrite(&bz2err, pfbz2, buf, 8);
			if (bz2err != BZ_OK)
				errx_diff(1, "BZ2_bzWrite, bz2err = %d", bz2err);

			offtout((pos - lenb) - (lastpos + lenf), buf);
			BZ2_bzWrite(&bz2err, pfbz2, buf, 8);
			if (bz2err != BZ_OK)
				errx_diff(1, "BZ2_bzWrite, bz2err = %d", bz2err);

			lastscan = scan - lenb;
			lastpos = pos - lenb;
			lastoffset = pos - scan;
		};
	};
	BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
	if (bz2err != BZ_OK)
		errx_diff(1, "BZ2_bzWriteClose, bz2err = %d", bz2err);

	/* Compute size of compressed ctrl data */
	if ((len = ftell(pf)) == -1)
		err_diff(1, "ftello");
	offtout(len - 32, header + 8);

	/* Write compressed diff data */
	if ((pfbz2 = BZ2_bzWriteOpen(&bz2err, pf, 9, 0, 0)) == NULL)
		errx_diff(1, "BZ2_bzWriteOpen, bz2err = %d", bz2err);
	BZ2_bzWrite(&bz2err, pfbz2, db, dblen);
	if (bz2err != BZ_OK)
		errx_diff(1, "BZ2_bzWrite, bz2err = %d", bz2err);
	BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
	if (bz2err != BZ_OK)
		errx_diff(1, "BZ2_bzWriteClose, bz2err = %d", bz2err);

	/* Compute size of compressed diff data */
	if ((newsize = ftell(pf)) == -1)
		err_diff(1, "ftello");
	offtout(newsize - len, header + 16);

	/* Write compressed extra data */
	if ((pfbz2 = BZ2_bzWriteOpen(&bz2err, pf, 9, 0, 0)) == NULL)
		errx_diff(1, "BZ2_bzWriteOpen, bz2err = %d", bz2err);
	BZ2_bzWrite(&bz2err, pfbz2, eb, eblen);
	if (bz2err != BZ_OK)
		errx_diff(1, "BZ2_bzWrite, bz2err = %d", bz2err);
	BZ2_bzWriteClose(&bz2err, pfbz2, 0, NULL, NULL);
	if (bz2err != BZ_OK)
		errx_diff(1, "BZ2_bzWriteClose, bz2err = %d", bz2err);

	/* Seek to the beginning, write the header, and close the file */
	if (fseek(pf, 0, SEEK_SET))
		err_diff(1, "fseeko");
	if (fwrite(header, 32, 1, pf) != 1)
		err_diff(1, "fwrite(%s)", argv[3]);
	if (fclose(pf))
		err_diff(1, "fclose");

	/* Free the memory we used */
	free(db);
	free(eb);
	free(I);
	free(pold);
	free(pnew);

	return 0;
}
// 返回0 为成功
bool patch(const char *oldFile, const char *newFile, const char *patchFile) {
    char *bsdiff = "bsdiff";
    char* argv[] = { bsdiff,(char*)oldFile, (char*)newFile, (char*)patchFile };
    int result = main_diff(4, argv);
    return result == 0;
}

