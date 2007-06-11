/**
 * @file
 *
 * Search a fclist (either the disk, a database generated by indexcon
 * or apol, or a file_contexts file for entries that match a given
 * SELinux file context.
 *
 *  @author Jeremy A. Mowery jmowery@tresys.com
 *  @author Jason Tang jtang@tresys.com
 *
 *  Copyright (C) 2003-2007 Tresys Technology, LLC
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <config.h>

#include <sefs/db.hh>
#include <sefs/fcfile.hh>
#include <sefs/filesystem.hh>
#include <sefs/entry.hh>
#include <sefs/query.hh>

using namespace std;

#include <errno.h>
#include <iostream>
#include <getopt.h>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define COPYRIGHT_INFO "Copyright (C) 2003-2007 Tresys Technology, LLC"

static struct option const longopts[] = {
	{"class", required_argument, NULL, 'c'},
	{"type", required_argument, NULL, 't'},
	{"user", required_argument, NULL, 'u'},
	{"role", required_argument, NULL, 'r'},
	{"mls-range", required_argument, NULL, 'm'},
	{"path", required_argument, NULL, 'p'},
	{"regex", no_argument, NULL, 'R'},
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'V'},
	{NULL, 0, NULL, 0}
};

static void usage(const char *program_name, bool brief)
{
	cout << "Usage: " << program_name << " FCLIST [OPTIONS] [EXPRESSION]" << endl << endl;
	if (brief)
	{
		cout << "\tTry " << program_name << " --help for more help." << endl << endl;
		return;
	}

	cout << "Find files matching the given SELinux context." << endl << endl;

	cout << "FCLIST is a directory name, a file_contexts file, or a database" << endl;
	cout << "created by a previous run of indexcon." << endl;
	cout << endl;

	cout << "EXPRESSION:" << endl;
	cout << "  -t TYPE,  --type=TYPE          find contexts with type TYPE" << endl;
	cout << "  -u USER,  --user=USER          find contexts with user USER" << endl;
	cout << "  -r ROLE,  --role=ROLE          find contexts with role ROLE" << endl;
	cout << "  -m RANGE, --mls-range=RANGE    find contexts with MLS range RANGE" << endl;
	cout << "  -p PATH,  --path=PATH          find files in PATH" << endl;
	cout << "  -c CLASS, --class=CLASS        find files of object class CLASS" << endl;
	cout << endl;

	cout << "OPTIONS:" << endl;
	cout << "  -d, --direct                   do not search for type's attributes" << endl;
	cout << "  -R, --regex                    enable regular expressions" << endl;
	cout << "  -h, --help                     print this help text and exit" << endl;
	cout << "  -V, --version                  print version information and exit" << endl;
	cout << endl;
	cout << "If the fclist does not contain MLS ranges and -m was given," << endl;
	cout << "then the search will return nothing." << endl;
}

int main(int argc, char *argv[])
{
	int optc;
	sefs_query *query = new sefs_query();

	char *type = NULL;
	bool indirect = true;
	try
	{
		while ((optc = getopt_long(argc, argv, "t:u:r:m:p:c:RhV", longopts, NULL)) != -1)
		{
			switch (optc)
			{
			case 't':
				type = optarg;
				break;
			case 'u':
				query->user(optarg);
				break;
			case 'r':
				query->role(optarg);
				break;
			case 'm':
				query->range(optarg, APOL_QUERY_EXACT);
				break;
			case 'p':
				query->path(optarg);
				break;
			case 'c':
				query->objectClass(optarg);
				break;
			case 'd':
				indirect = false;
				break;
			case 'R':
				query->regex(true);
				break;
			case 'h':     // help
				usage(argv[0], false);
				exit(0);
			case 'V':     // version
				cout << "findcon " << VERSION << endl << COPYRIGHT_INFO << endl;
				exit(0);
			default:
				usage(argv[0], true);
				exit(1);
			}
		}
		query->type(type, indirect);
	}
	catch(std::bad_alloc)
	{
		delete query;
		exit(-1);
	}
	if (optind + 1 != argc)
	{
		usage(argv[0], 1);
		delete query;
		exit(-1);
	}

	// try to autodetect the type of thing being searched
	struct stat sb;
	if (stat(argv[optind], &sb) != 0)
	{
		cerr << "Could not open " << argv[optind] << ": " << strerror(errno) << endl;
		delete query;
		exit(-1);
	}

	sefs_fclist *fclist = NULL;
	apol_vector_t *results = NULL;
	try
	{
		if (S_ISDIR(sb.st_mode))
		{
			fclist = new sefs_filesystem(argv[optind], NULL, NULL);
		}
		else if (sefs_db::isDB(argv[optind]))
		{
			fclist = new sefs_db(argv[optind], NULL, NULL);
		}
		else
		{
			fclist = new sefs_fcfile(argv[optind], NULL, NULL);
		}

		results = fclist->runQuery(query);
		for (size_t i = 0; i < apol_vector_get_size(results); i++)
		{
			const sefs_entry *e = static_cast < sefs_entry * >(apol_vector_get_element(results, i));
			char *str = e->toString();
			cout << str << endl;
			free(str);
		}
	}
	catch(...)
	{
		delete query;
		delete fclist;
		apol_vector_destroy(&results);
		exit(-1);
	}

	delete query;
	delete fclist;
	apol_vector_destroy(&results);
	return 0;
}
