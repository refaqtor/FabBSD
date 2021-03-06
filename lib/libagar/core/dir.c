/*
 * Copyright (c) 2004-2008 Hypertriton, Inc. <http://hypertriton.com/>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1983, 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>

#ifdef _WIN32
# include <core/queue_close.h>			/* Conflicts */
# include <windows.h>
# include <core/queue_close.h>
# include <core/queue.h>
#else
# include <sys/stat.h>
# include <dirent.h>
# include <unistd.h>
#endif

#include <string.h>
#include <errno.h>

#include <core/core.h>

int
AG_MkDir(const char *dir)
{
#ifdef _WIN32
	if (CreateDirectory(dir, NULL)) {
		return (0);
	} else {
		AG_SetError(_("%s: Failed to create directory (%d)"), dir,
		    (int)GetLastError());
		return (-1);
	}
#else
	if (mkdir(dir, 0700) == 0) {
		return (0);
	} else {
		AG_SetError(_("%s: Failed to create directory (%s)"), dir,
		    strerror(errno));
		return (-1);
	}
#endif
}

int
AG_RmDir(const char *dir)
{
#ifdef _WIN32
	if (RemoveDirectory(dir)) {
		return (0);
	} else {
		AG_SetError(_("%s: Failed to remove directory (%d)"), dir,
		    (int)GetLastError());
		return (-1);
	}
#else
	if (rmdir(dir) == 0) {
		return (0);
	} else {
		AG_SetError(_("%s: Failed to remove directory (%s)"), dir,
		    strerror(errno));
		return (-1);
	}
#endif
}

int
AG_ChDir(const char *dir)
{
#ifdef _WIN32
	if (SetCurrentDirectory(dir)) {
		return (0);
	} else {
		AG_SetError(_("%s: Failed to change directory (%d)"), dir,
		    (int)GetLastError());
		return (-1);
	}
#else
	if (chdir(dir) == 0) {
		return (0);
	} else {
		AG_SetError(_("%s: Failed to change directory (%s)"), dir,
		    strerror(errno));
		return (-1);
	}
#endif
}

AG_Dir *
AG_OpenDir(const char *path)
{
	AG_Dir *dir;

	dir = Malloc(sizeof(AG_Dir));
	dir->ents = NULL;
	dir->nents = 0;

#ifdef _WIN32
	{
		char dpath[AG_PATHNAME_MAX];
		HANDLE h;
		WIN32_FIND_DATA fdata;
		DWORD rv;

		Strlcpy(dpath, path, sizeof(dpath));
		Strlcat(dpath, "\\*", sizeof(dpath));
		if ((h = FindFirstFile(dpath, &fdata))==INVALID_HANDLE_VALUE) {
			AG_SetError(_("Invalid file handle (%d)"),
			    (int)GetLastError());
			goto fail;
		}
		while (FindNextFile(h, &fdata) != 0) {
			dir->ents = Realloc(dir->ents,
			    (dir->nents+1)*sizeof(char *));
			dir->ents[dir->nents++] = Strdup(fdata.cFileName);
		}
		rv = GetLastError();
		FindClose(h);
		if (rv != ERROR_NO_MORE_FILES) {
			AG_SetError("FindNextFileError (%lu)", rv);
			goto fail;
		}
	}
#else /* !_WIN32 */
	{
		DIR *dp;
		struct dirent *dent;
		
		if ((dp = opendir(path)) == NULL) {
			AG_SetError(_("%s: Failed to open directory (%s)"),
			    path, strerror(errno));
			goto fail;
		}
		while ((dent = readdir(dp)) != NULL) {
			dir->ents = Realloc(dir->ents,
			    (dir->nents+1)*sizeof(char *));
			dir->ents[dir->nents++] = Strdup(dent->d_name);
		}
		closedir(dp);
	}
#endif /* _WIN32 */

	return (dir);
fail:
	Free(dir);
	return (NULL);
}

void
AG_CloseDir(AG_Dir *dir)
{
	int i;

	for (i = 0; i < dir->nents; i++) {
		Free(dir->ents[i]);
	}
	Free(dir->ents);
	Free(dir);
}

int
AG_MkPath(const char *path)
{
	AG_FileInfo info;
	char *pathp, *slash;
	int done = 0;
	int rv;

	slash = pathp = Strdup(path);

	while (!done) {
		slash += strspn(slash, AG_PATHSEP);
		slash += strcspn(slash, AG_PATHSEP);

		done = (*slash == '\0');
		*slash = '\0';

		if (AG_GetFileInfo(pathp, &info) == -1) {
			if ((rv = AG_FileExists(pathp)) == -1) {
				goto fail;
			} else if (rv == 0) {
				if (AG_MkDir(pathp) == -1)
					goto fail;
			}
		} else if (info.type != AG_FILE_DIRECTORY) {
			AG_SetError(_("%s: Existing file"), pathp);
			goto fail;
		}

		*slash = AG_PATHSEPCHAR;
	}
	Free(pathp);
	return (0);
fail:
	Free(pathp);
	return (-1);
}

int
AG_GetCWD(char *buf, size_t len)
{
#ifdef _WIN32
	DWORD rv;

	if ((rv = GetCurrentDirectory(len, buf)) == 0) {
		AG_SetError(_("Failed to get current directory (%d)"),
		    (int)GetLastError());
		return (-1);
	} else if (rv > len) {
		AG_SetError(_("Failed to get current directory (%s)"),
		    _("Path name is too long"));
		return (-1);
	}
	return (0);
#else
	if (getcwd(buf, len) == NULL) {
		AG_SetError(_("Failed to get current directory (%s)"),
		    strerror(errno));
		return (-1);
	}
	return (0);
#endif
}
